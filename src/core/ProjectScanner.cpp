#include "core/ProjectScanner.hpp"
#include "core/OptionKeys.hpp"
#include "core/Platform.hpp"
#include "core/ProjectPaths.hpp"
#include "core/SceneParser.hpp"
#include "core/Types.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <toml++/toml.hpp>

namespace SatyrAV{

namespace fs = std::filesystem;

namespace{

using Level = ScanFinding::Level;

std::string Trim(const std::string& s){
	size_t a = 0, b = s.size();
	while(a < b && std::isspace((unsigned char)s[a])) a++;
	while(b > a && std::isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

std::string Lower(std::string s){
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });
	return s;
}

std::string Unquote(const std::string& v){
	if(v.size() >= 2 && ((v.front() == '"' && v.back() == '"') ||
	                     (v.front() == '\'' && v.back() == '\''))){
		return v.substr(1, v.size() - 2);
	}
	return v;
}

bool IsFontFile(const std::string& name){
	const std::string l = Lower(name);
	auto ends = [&](const char* ext){
		const size_t n = std::char_traits<char>::length(ext);
		return l.size() >= n && l.compare(l.size() - n, n, ext) == 0;
	};
	return ends(".ttf") || ends(".otf") || ends(".ttc");
}

bool IsBoolText(const std::string& v){
	const std::string l = Lower(v);
	return l == "true" || l == "false" || l == "1" || l == "0";
}

bool IsInt(const std::string& v){
	if(v.empty()) return false;
	size_t i = (v[0] == '-' || v[0] == '+') ? 1 : 0;
	if(i == v.size()) return false;
	for(; i < v.size(); i++) if(!std::isdigit((unsigned char)v[i])) return false;
	return true;
}

bool IsNumber(const std::string& v){
	if(v.empty()) return false;
	char* end = nullptr;
	std::strtod(v.c_str(), &end);
	return end && *end == '\0';
}

enum class OptKind{ Number, Bool, Colour, Font, Other };

OptKind KindOf(OptionKey k){
	switch(k){
		case OptionKey::FontSize:
		case OptionKey::TextOutline:
		case OptionKey::SubtitleTransparency:
		case OptionKey::SubtitlePosY:
			return OptKind::Number;
		case OptionKey::Capitalize:
		case OptionKey::AudioFade:
		case OptionKey::SubtitleItalic:
		case OptionKey::FixedTextCentre:
		case OptionKey::DisableTextOutline:
		case OptionKey::TransparentBackground:
			return OptKind::Bool;
		case OptionKey::BackgroundColour:
		case OptionKey::TextColour:
		case OptionKey::SubtitleColour:
			return OptKind::Colour;
		case OptionKey::Font:
		case OptionKey::FontItalic:
		case OptionKey::FontBold:
		case OptionKey::FontBoldItalic:
			return OptKind::Font;
		default:
			return OptKind::Other;
	}
}

bool ProjectOnly(OptionKey k){
	return k == OptionKey::SubtitleItalic || k == OptionKey::SubtitleColour ||
		k == OptionKey::SubtitleTransparency || k == OptionKey::SubtitlePosY;
}

// `<TYPE>.SPEED` and friends, as a scene's [Options] accepts them.
bool IsParticleTunable(const std::string& key){
	const size_t dot = key.find('.');
	if(dot == std::string::npos) return false;
	ParticleType pt;
	if(!ParseParticleTypeStrict(key.substr(0, dot), pt)) return false;
	std::string rest = key.substr(dot + 1);
	std::transform(rest.begin(), rest.end(), rest.begin(), ::toupper);
	static const char* ok[] = {"SPEED", "DENS", "DENSITY", "X.DIST", "X",
		"VX.DIST", "VX", "VY.DIST", "VY"};
	for(const char* o : ok) if(rest == o) return true;
	return false;
}

bool IsColour(const std::string& v, bool subtitle){
	Colour c;
	if(ParseColour(v, c)) return true;
	const std::string l = Lower(v);
	return subtitle && (l == "grey" || l == "gray");
}

class Collector{
public:
	std::vector<ScanFinding> list;
	void Add(Level lv, const std::string& file, int line, const std::string& msg){
		ScanFinding f;
		f.level = lv; f.file = file; f.line = line; f.message = msg;
		// Loops and macros repeat a command; report its line once.
		const std::string key = file + "\x1f" + std::to_string(line) + "\x1f" + msg;
		if(seen.insert(key).second) list.push_back(std::move(f));
	}
	bool HasLine(const std::string& file, int line) const{
		return std::any_of(list.begin(), list.end(), [&](const ScanFinding& f){
			return f.file == file && f.line == line;
		});
	}
private:
	std::set<std::string> seen;
};

// ── schema.toml ──────────────────────────────────────────────

int LineOf(const toml::node& n){
	return (int)n.source().begin.line;
}

void CheckOptionValue(Collector& out, const std::string& file, int line,
	const std::string& name, OptionKey key, const std::string& value,
	const std::string& projectPath){
	switch(KindOf(key)){
		case OptKind::Number:
			if(!IsNumber(value))
				out.Add(Level::Error, file, line, name + " needs a number, not '" + value + "'");
			break;
		case OptKind::Bool:
			if(!IsBoolText(value))
				out.Add(Level::Warning, file, line, name + " should be true or false, not '"
					+ value + "' (read as false)");
			break;
		case OptKind::Colour:
			if(!IsColour(value, key == OptionKey::SubtitleColour))
				out.Add(Level::Error, file, line, name + ": unknown colour '" + value + "'");
			break;
		case OptKind::Font:
			if(!value.empty() && ProjectPaths::FindFont(projectPath, value).empty())
				out.Add(Level::Error, file, line, name + ": font file '" + value + "' not found");
			break;
		default: break;
	}
}

// Returns the scene names the akter list, in order.
std::vector<std::string> ScanSchema(Collector& out, const std::string& projectPath){
	const std::string file = "schema.toml";
	const std::string path = projectPath + "/schema.toml";
	std::vector<std::string> scenes;
	if(!ProjectPaths::IsFile(path)){
		out.Add(Level::Error, file, 0, "schema.toml is missing: this is not a SatyrAV project");
		return scenes;
	}

	toml::table tbl;
	try{
		std::ifstream in(Platform::Utf8ToPath(path), std::ios::binary);
		tbl = toml::parse(in, path);
	} catch(const toml::parse_error& e){
		out.Add(Level::Error, file, (int)e.source().begin.line,
			"does not parse: " + std::string(e.description()));
		return scenes;
	}

	// [Options]
	if(const toml::table* options = tbl["Options"].as_table()){
		for(auto&& [k, v] : *options){
			const std::string name(k.str());
			const OptionKey key = LookupOptionKey(name);
			const int line = LineOf(v);
			if(key == OptionKey::Unknown){
				out.Add(Level::Warning, file, line, "unknown option '" + name + "' (ignored)");
				continue;
			}
			// Check the value the way SchemaParser reads it: by TOML type.
			const OptKind kind = KindOf(key);
			if(kind == OptKind::Number && !v.is_number()){
				out.Add(Level::Error, file, line, name + " needs a number (no quotes)");
			} else if(kind == OptKind::Bool && !v.is_boolean()){
				out.Add(Level::Error, file, line, name + " needs true or false (no quotes)");
			} else if((kind == OptKind::Colour || kind == OptKind::Font) && !v.is_string()){
				out.Add(Level::Error, file, line, name + " needs a quoted value");
			} else if(auto s = v.value<std::string>()){
				CheckOptionValue(out, file, line, name, key, *s, projectPath);
			}
		}
	}

	// [Hotkeys]
	if(const toml::table* hk = tbl["Hotkeys"].as_table()){
		for(auto&& [k, v] : *hk){
			const std::string name(k.str());
			bool fkey = false;
			if(name.size() >= 2 && (name[0] == 'F') && IsInt(name.substr(1))){
				const int n = std::stoi(name.substr(1));
				fkey = n >= 1 && n <= 12;
			}
			if(!fkey){
				out.Add(Level::Warning, file, LineOf(v), "[Hotkeys] '" + name
					+ "' is not F1-F12 (ignored)");
				continue;
			}
			auto s = v.value<std::string>();
			if(!s || s->empty()) continue;
			if(!ProjectPaths::IsFile(ProjectPaths::ResolveMedia(projectPath, *s))){
				out.Add(Level::Error, file, LineOf(v), name + ": sound '" + *s + "' not found");
			}
		}
	}

	// Akter
	const int aktCount = tbl["Structure"]["akter"].value_or(0);
	for(int i = 1; i <= aktCount; i++){
		const std::string key = "Structure.Akt" + std::to_string(i) + ".schema";
		const toml::array* arr = tbl.at_path(key).as_array();
		if(!arr){
			out.Add(Level::Warning, file, 0, "Akt " + std::to_string(i)
				+ " has no scene list ([Structure.Akt" + std::to_string(i) + "] schema = [...])");
			continue;
		}
		for(auto& elem : *arr){
			auto name = elem.value<std::string>();
			if(!name) continue;
			scenes.push_back(*name);
			if(!ProjectPaths::IsFile(projectPath + "/scenes/" + *name + ".ngk")){
				out.Add(Level::Error, file, LineOf(elem), "Akt " + std::to_string(i)
					+ ": scene '" + *name + "' has no file scenes/" + *name + ".ngk");
			}
		}
	}
	return scenes;
}

// ── A scene file ─────────────────────────────────────────────

void CheckMediaName(Collector& out, const std::string& file, int line,
	const std::string& what, const std::string& name, const std::string& projectPath){
	if(name.empty()) return;
	// A path with a folder in it is used as written (randomImages lists).
	const bool hasSep = name.find('/') != std::string::npos ||
		name.find('\\') != std::string::npos;
	const std::string full = hasSep && !ProjectPaths::IsFile(projectPath + "/" + name)
		? name : ProjectPaths::ResolveMedia(projectPath, name);
	if(!ProjectPaths::IsFile(full)){
		out.Add(Level::Error, file, line, what + ": file '" + name + "' not found");
	}
}

void CheckCommand(Collector& out, const std::string& file, const Command& c,
	const std::string& projectPath, int lineOverride = 0){
	const int line = lineOverride > 0 ? lineOverride : c.lineNumber;
	const bool fileCommand = (c.type == CommandType::Show || c.type == CommandType::Play)
		&& c.animationName.empty();
	if(fileCommand){
		const char* what = c.type == CommandType::Show ? "show" : "play";
		const auto& rnd = c.mods.randomImagePaths;
		if(!rnd.empty()){
			if(rnd.size() == 1 && rnd[0].rfind("DIR:", 0) == 0){
				const std::string dir = rnd[0].substr(4);
				const std::string full = projectPath + "/pictures/" + dir;
				if(!ProjectPaths::IsDirectory(full)){
					out.Add(Level::Error, file, line, "randomImages: folder 'pictures/"
						+ dir + "' not found");
				}
			} else{
				for(const auto& p : rnd) CheckMediaName(out, file, line, "randomImages", p, projectPath);
			}
		} else{
			CheckMediaName(out, file, line, what, c.argument, projectPath);
		}
	}
	if(c.directive == FontDirective::UseFont && !c.directiveFont.empty() &&
	   ProjectPaths::FindFont(projectPath, c.directiveFont).empty()){
		out.Add(Level::Error, file, line, "use: font file '" + c.directiveFont + "' not found");
	}
	for(const auto& s : c.subCommands) CheckCommand(out, file, s, projectPath, lineOverride);
	for(const auto& s : c.loopBody)    CheckCommand(out, file, s, projectPath, lineOverride);
}

void ScanScene(Collector& out, const std::string& projectPath, const std::string& sceneName){
	const std::string file = "scenes/" + sceneName + ".ngk";
	const std::string path = projectPath + "/" + file;
	std::ifstream in(Platform::Utf8ToPath(path), std::ios::binary);
	if(!in.is_open()) return; // reported by the schema check

	// Raw pass: sections and [Options], which the parser reads leniently.
	enum class Sec{ None, Options, Preload, Cue, Other };
	Sec sec = Sec::None;
	std::string raw;
	int lineNo = 0;
	while(std::getline(in, raw)){
		lineNo++;
		if(!raw.empty() && raw.back() == '\r') raw.pop_back();
		if(lineNo == 1 && raw.rfind("\xEF\xBB\xBF", 0) == 0) raw = raw.substr(3);

		std::string line = raw;
		const size_t cp = (sec == Sec::Options) ? FindOptionCommentStart(line)
			: FindCommentStart(line);
		if(cp != std::string::npos) line = line.substr(0, cp);
		line = Trim(line);
		if(line.empty()) continue;

		if(line.size() >= 2 && line.front() == '[' && line.back() == ']'){
			const std::string l = Lower(line);
			if(line == "[Options]")       sec = Sec::Options;
			else if(line == "[Preload]")  sec = Sec::Preload;
			else if(line == "[Cue]")      sec = Sec::Cue;
			else if(line == "[Macro]" || line == "[Commands]" ||
			        l == "[backstage]" || l == "[animation]" || l == "[animations]"){
				sec = Sec::Other;
			} else{
				static const char* exact[] = {"[Options]", "[Preload]", "[Macro]", "[Cue]", "[Commands]"};
				std::string hint;
				for(const char* e : exact) if(Lower(e) == l) hint = std::string(" - write it ") + e;
				// Only a real header ends the section; this line is read as
				// part of the one before it.
				out.Add(Level::Error, file, lineNo, "unknown section " + line + hint);
			}
			continue;
		}

		switch(sec){
			case Sec::None:
				out.Add(Level::Warning, file, lineNo, "outside any section (ignored): " + line);
				break;
			case Sec::Options:{
				const size_t eq = line.find('=');
				if(eq == std::string::npos){
					out.Add(Level::Error, file, lineNo, "[Options] expects 'Name = value', got '"
						+ line + "'");
					break;
				}
				const std::string name = Trim(line.substr(0, eq));
				const std::string value = Unquote(Trim(line.substr(eq + 1)));
				const OptionKey key = LookupOptionKey(name);
				if(key == OptionKey::Unknown){
					if(!IsParticleTunable(name)){
						out.Add(Level::Error, file, lineNo, "unknown option '" + name + "' (ignored)");
					}
					break;
				}
				if(ProjectOnly(key)){
					out.Add(Level::Warning, file, lineNo, name
						+ " only works in schema.toml (ignored in a scene)");
					break;
				}
				CheckOptionValue(out, file, lineNo, name, key, value, projectPath);
				break;
			}
			case Sec::Preload:{
				const std::string name = Unquote(line);
				if(IsFontFile(name)){
					if(ProjectPaths::FindFont(projectPath, name).empty())
						out.Add(Level::Error, file, lineNo, "[Preload]: font file '" + name + "' not found");
				} else{
					CheckMediaName(out, file, lineNo, "[Preload]", name, projectPath);
				}
				break;
			}
			default: break;
		}
	}

	// The parser's own pass: commands, files, fonts, and its warnings.
	const Scene scene = SceneParser::ParseFile(path);
	for(const auto& c : scene.commands) CheckCommand(out, file, c, projectPath);
	for(const auto& a : scene.animations){
		CheckCommand(out, file, a.command, projectPath, a.lineNumber);
	}
	for(const auto& w : scene.warnings){
		// An unknown section is already reported on its line.
		if(out.HasLine(file, w.lineNumber) && w.message.rfind("Unknown keyword: '[", 0) == 0) continue;
		out.Add(Level::Warning, file, w.lineNumber, w.message);
	}
}

} // namespace

namespace ProjectScanner{

ScanReport Scan(const std::string& projectPath){
	Collector out;
	ScanReport report;

	const std::vector<std::string> listed = ScanSchema(out, projectPath);
	if(ProjectPaths::IsFile(projectPath + "/schema.toml")) report.filesScanned++;

	std::vector<std::string> order; // files in report order
	order.push_back("schema.toml");
	std::set<std::string> done;
	for(const auto& name : listed){
		if(!done.insert(name).second) continue;
		order.push_back("scenes/" + name + ".ngk");
		if(ProjectPaths::IsFile(projectPath + "/scenes/" + name + ".ngk")){
			ScanScene(out, projectPath, name);
			report.filesScanned++;
		}
	}

	// .ngk files no akt lists: never shown, but scanned too.
	std::vector<std::string> unlisted;
	std::error_code ec;
	for(fs::directory_iterator it(Platform::Utf8ToPath(projectPath + "/scenes"), ec), end;
	    !ec && it != end; it.increment(ec)){
		if(!it->is_regular_file(ec)) continue;
		const fs::path p = it->path();
		if(p.extension() != ".ngk") continue;
		const std::string stem = p.stem().u8string();
		if(!done.count(stem)) unlisted.push_back(stem);
	}
	std::sort(unlisted.begin(), unlisted.end());
	for(const auto& name : unlisted){
		const std::string file = "scenes/" + name + ".ngk";
		order.push_back(file);
		out.Add(Level::Warning, file, 0, "not in any akt in schema.toml, so never shown");
		ScanScene(out, projectPath, name);
		report.filesScanned++;
	}

	// Report order: file order above, then line.
	auto rank = [&](const std::string& f){
		auto it = std::find(order.begin(), order.end(), f);
		return (int)(it - order.begin());
	};
	report.findings = std::move(out.list);
	std::stable_sort(report.findings.begin(), report.findings.end(),
		[&](const ScanFinding& a, const ScanFinding& b){
			const int ra = rank(a.file), rb = rank(b.file);
			if(ra != rb) return ra < rb;
			return a.line < b.line;
		});
	for(const auto& f : report.findings){
		if(f.level == Level::Error) report.errors++;
		else report.warnings++;
	}
	return report;
}

} // namespace ProjectScanner

} // namespace SatyrAV
