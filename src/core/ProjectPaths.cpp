#include "core/ProjectPaths.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include <algorithm>
#include <filesystem>
#include <vector>

namespace SatyrAV{
namespace ProjectPaths{

namespace fs = std::filesystem;

bool IsFile(const std::string& path){
	if(path.empty()) return false;
	std::error_code ec;
	return fs::is_regular_file(Platform::Utf8ToPath(path), ec);
}

bool IsDirectory(const std::string& path){
	if(path.empty()) return false;
	std::error_code ec;
	return fs::is_directory(Platform::Utf8ToPath(path), ec);
}

static bool Exists(const std::string& path){
	std::error_code ec;
	return fs::exists(Platform::Utf8ToPath(path), ec);
}

static std::string Lower(std::string s){
	for(char& c : s) if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
	return s;
}

static std::vector<std::string> Split(const std::string& rel){
	std::vector<std::string> parts;
	std::string cur;
	for(char c : rel){
		if(c == '/'){ if(!cur.empty()) parts.push_back(cur); cur.clear(); }
		else cur += c;
	}
	if(!cur.empty()) parts.push_back(cur);
	return parts;
}

// The entries of a folder, by UTF-8 name.
static std::vector<std::pair<std::string, bool>> List(const std::string& dir){
	std::vector<std::pair<std::string, bool>> out;
	std::error_code ec;
	for(fs::directory_iterator it(Platform::Utf8ToPath(dir), ec), end; !ec && it != end; it.increment(ec)){
		std::error_code ec2;
		out.push_back({it->path().filename().u8string(), it->is_directory(ec2)});
	}
	return out;
}

std::string Normalise(const std::string& name){
	std::string n = name;
	for(char& c : n) if(c == '\\') c = '/';
	while(n.size() > 2 && n.compare(0, 2, "./") == 0) n.erase(0, 2);
	return n;
}

bool IsAbsolute(const std::string& name){
	return !name.empty() && (name[0] == '/' || name[0] == '\\' ||
		(name.size() >= 2 && name[1] == ':'));
}

// `rel` under `root`, matching each folder and file name without regard
// to case — what Windows does anyway, so a scene written there
// (`Mega/1.png` for a folder `MEGA`) finds its files on Linux too.
static std::string FindNoCase(const std::string& root, const std::string& rel){
	std::string cur = root;
	for(const auto& part : Split(rel)){
		if(Exists(cur + "/" + part)){ cur += "/" + part; continue; }
		const std::string want = Lower(part);
		bool found = false;
		for(const auto& [name, isDir] : List(cur)){
			(void)isDir;
			if(Lower(name) == want){ cur += "/" + name; found = true; break; }
		}
		if(!found) return {};
	}
	return cur;
}

std::vector<std::string> MediaRoots(const std::string& projectPath){
	return {projectPath, projectPath + "/pictures", projectPath + "/movies",
		projectPath + "/sound"};
}

std::string ResolveMedia(const std::string& projectPath, const std::string& name){
	const std::string n = Normalise(name);
	if(IsAbsolute(n)) return n;
	const auto roots = MediaRoots(projectPath);
	for(const auto& root : roots){
		const std::string tryPath = root + "/" + n;
		if(Exists(tryPath)) return tryPath;
	}
	for(const auto& root : roots){
		const std::string found = FindNoCase(root, n);
		if(!found.empty()) return found;
	}
	return projectPath + "/" + n;
}

bool HasWildcard(const std::string& name){
	return name.find_first_of("*?") != std::string::npos;
}

// `*` any run of characters, `?` one, within one name; no case.
static bool Match(const std::string& pat, const std::string& name){
	const std::string p = Lower(pat), s = Lower(name);
	size_t pi = 0, si = 0, star = std::string::npos, mark = 0;
	while(si < s.size()){
		if(pi < p.size() && (p[pi] == '?' || p[pi] == s[si])){ pi++; si++; }
		else if(pi < p.size() && p[pi] == '*'){ star = pi++; mark = si; }
		else if(star != std::string::npos){ pi = star + 1; si = ++mark; }
		else return false;
	}
	while(pi < p.size() && p[pi] == '*') pi++;
	return pi == p.size();
}

static void Walk(const std::string& dir, const std::vector<std::string>& parts,
	size_t i, std::vector<std::string>& out){
	if(i == parts.size()){
		if(IsFile(dir)) out.push_back(dir);
		return;
	}
	const std::string& part = parts[i];
	if(part == "**"){
		Walk(dir, parts, i + 1, out);                 // zero folders
		for(const auto& [name, isDir] : List(dir)){
			if(isDir) Walk(dir + "/" + name, parts, i, out);
		}
		return;
	}
	if(!HasWildcard(part)){
		const std::string exact = dir + "/" + part;
		if(Exists(exact)){ Walk(exact, parts, i + 1, out); return; }
	}
	for(const auto& [name, isDir] : List(dir)){
		if(!Match(part, name)) continue;
		const bool last = (i + 1 == parts.size());
		if(last ? !isDir : isDir) Walk(dir + "/" + name, parts, i + 1, out);
	}
}

std::vector<std::string> ExpandWildcard(const std::string& projectPath, const std::string& pattern){
	const std::string n = Normalise(pattern);
	const auto parts = Split(n);
	std::vector<std::string> out;
	if(parts.empty()) return out;
	if(IsAbsolute(n)){
		// "/abs/dir/*.png" walks from "" (each step adds "/name");
		// "C:/dir/*.png" from the drive.
		const bool unixRoot = n[0] == '/';
		const std::vector<std::string> rest(parts.begin() + (unixRoot ? 0 : 1), parts.end());
		Walk(unixRoot ? std::string() : parts[0], rest, 0, out);
	} else{
		for(const auto& root : MediaRoots(projectPath)) Walk(root, parts, 0, out);
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
	return out;
}

std::vector<std::string> ExpandPreload(const std::string& projectPath,
	const std::vector<std::string>& entries){
	std::vector<std::string> out;
	for(const auto& e : entries){
		if(HasWildcard(e)){
			for(auto& f : ExpandWildcard(projectPath, e)) out.push_back(std::move(f));
		} else{
			out.push_back(ResolveMedia(projectPath, e));
		}
	}
	return out;
}

std::string FindFont(const std::string& projectPath, const std::string& name){
	if(name.empty()) return {};
	const bool hasDrive = name.size() >= 2 && name[1] == ':';
	const bool absolute = hasDrive || name.front() == '/' || name.front() == '\\';
	const bool hasSep = name.find('/') != std::string::npos
		|| name.find('\\') != std::string::npos;

	if(absolute) return IsFile(name) ? name : std::string();

	const std::string base = projectPath + "/";
	std::vector<std::string> tries;
	if(hasSep){
		tries = {base + name};
	} else{
		tries = {base + "fonts/" + name, base + "font/" + name,
		         base + "FONTS/" + name, base + name};
	}
	for(const auto& t : tries){
		if(IsFile(t)) return t;
	}
	const std::string shared = Config::Instance().ResolveFontPath(name);
	return IsFile(shared) ? shared : std::string();
}

} // namespace ProjectPaths
} // namespace SatyrAV
