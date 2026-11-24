#include "core/ProjectPaths.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
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

std::string ResolveMedia(const std::string& projectPath, const std::string& name){
	const std::string full = projectPath + "/" + name;
	if(Exists(full)) return full;
	for(const char* sub : {"pictures", "movies", "sound"}){
		const std::string tryPath = projectPath + "/" + sub + "/" + name;
		if(Exists(tryPath)) return tryPath;
	}
	return full;
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
