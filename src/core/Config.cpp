#include "core/Config.hpp"
#include "core/Platform.hpp"
#include <toml++/toml.hpp>
#include <fstream>

namespace SatyrAV{

Config& Config::Instance(){
	static Config instance;
	return instance;
}

void Config::EnsureDefaults(){
	if(projectsDir.empty()){
		projectsDir = Platform::GetDefaultProjectsDir();
	}
	if(configPath.empty()){
		configPath = Platform::GetDefaultConfigPath();
	}
	if(fontsDir.empty()){
		fontsDir = Platform::GetDefaultFontsDir();
	}
	// Ensure directories exist
	Platform::EnsureDirectoryExists(projectsDir);
	Platform::EnsureDirectoryExists(fontsDir);
	auto configDir = configPath.substr(0, configPath.find_last_of("/\\"));
	if(!configDir.empty()){
		Platform::EnsureDirectoryExists(configDir);
	}
}

void Config::Load(const std::string& path){
	configPath = path;
	try{
		auto tbl = toml::parse_file(path);
		fontSize       = tbl["font"]["size"].value_or(25);
		fontPath       = tbl["font"]["path"].value_or(std::string(""));
		fontPathItalic     = tbl["font"]["italic"].value_or(std::string(""));
		fontPathBold       = tbl["font"]["bold"].value_or(std::string(""));
		fontPathBoldItalic = tbl["font"]["bold_italic"].value_or(std::string(""));
		masterWidth    = tbl["display"]["width"].value_or(1280);
		masterHeight   = tbl["display"]["height"].value_or(720);
		fullscreen     = tbl["display"]["fullscreen"].value_or(false);
		slaveDisplayIndex = tbl["display"]["slave_index"].value_or(1);
		slaveWindowed     = tbl["display"]["slave_windowed"].value_or(false);
		captureEnabled      = tbl["display"]["capture_enabled"].value_or(false);
		captureDisplayIndex = tbl["display"]["capture_display"].value_or(0);
		projectsDir    = tbl["paths"]["projects"].value_or(std::string(""));
		lastProject    = tbl["paths"]["last_project"].value_or(std::string(""));
		fontsDir       = tbl["paths"]["fonts"].value_or(std::string(""));
		videoPreloadBudgetMB   = tbl["video"]["preload_budget_mb"].value_or(4096);
		videoPreloadMaxSeconds = tbl["video"]["preload_max_seconds"].value_or(60);
		audioDeviceName        = tbl["audio"]["device"].value_or(std::string(""));
	} catch(const toml::parse_error&){
		// Use defaults
	}
	EnsureDefaults();
}

void Config::Save(){
	Save(configPath);
}

void Config::Save(const std::string& path){
	// Ensure parent dir exists
	auto dir = path.substr(0, path.find_last_of("/\\"));
	if(!dir.empty()){
		Platform::EnsureDirectoryExists(dir);
	}

	std::ofstream out(path);
	// NOTE: path-bearing values are written as TOML *literal* strings
	// (single quotes). Windows paths contain backslashes (e.g. "C:\Users")
	// which are invalid escape sequences inside basic (double-quoted) TOML
	// strings and cause toml::parse_file to throw — silently sending every
	// config value back to its default on the next launch. Literal strings
	// take no escapes, so backslashes pass through verbatim.
	out << "[font]\n";
	out << "size = " << fontSize << "\n";
	out << "path = '" << fontPath << "'\n";
	out << "italic = '" << fontPathItalic << "'\n";
	out << "bold = '" << fontPathBold << "'\n";
	out << "bold_italic = '" << fontPathBoldItalic << "'\n\n";
	out << "[display]\n";
	out << "width = " << masterWidth << "\n";
	out << "height = " << masterHeight << "\n";
	out << "fullscreen = " << (fullscreen ? "true" : "false") << "\n";
	out << "slave_index = " << slaveDisplayIndex << "\n";
	out << "slave_windowed = " << (slaveWindowed ? "true" : "false") << "\n";
	out << "capture_enabled = " << (captureEnabled ? "true" : "false") << "\n";
	out << "capture_display = " << captureDisplayIndex << "\n\n";
	out << "[paths]\n";
	out << "projects = '" << projectsDir << "'\n";
	out << "last_project = '" << lastProject << "'\n";
	out << "fonts = '" << fontsDir << "'\n\n";
	out << "[video]\n";
	out << "preload_budget_mb = " << videoPreloadBudgetMB << "\n";
	out << "preload_max_seconds = " << videoPreloadMaxSeconds << "\n\n";
	// (1.6.3) Output device by name. Empty string = system default.
	// Literal-string quoting because device names on Windows can contain
	// backslashes and parentheses that are unsafe in basic TOML strings.
	out << "[audio]\n";
	out << "device = '" << audioDeviceName << "'\n";
}

std::string Config::ResolveFontPath(const std::string& name) const{
	if(name.empty()) return name;
	// Absolute path? Leave it. We treat anything containing a path
	// separator (or a Windows drive-letter like "C:\") as already-resolved.
	bool hasSep = name.find('/') != std::string::npos
		|| name.find('\\') != std::string::npos;
	bool hasDrive = name.size() >= 2 && name[1] == ':';
	if(hasSep || hasDrive) return name;
	if(fontsDir.empty()) return name;

	// Pick the platform's preferred separator so the renderer's `fopen`
	// path lookups don't surprise anyone debugging from a log file.
#ifdef _WIN32
	const char sep = '\\';
#else
	const char sep = '/';
#endif
	std::string out = fontsDir;
	if(!out.empty() && out.back() != '/' && out.back() != '\\') out += sep;
	out += name;
	return out;
}

} // namespace SatyrAV
