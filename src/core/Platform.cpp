#include "core/Platform.hpp"
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace SatyrAV{

std::string Platform::GetHomeDir(){
#ifdef _WIN32
	// Use USERPROFILE on Windows
	const char* home = std::getenv("USERPROFILE");
	if(home) return std::string(home);
	// Fallback: HOMEDRIVE + HOMEPATH
	const char* drive = std::getenv("HOMEDRIVE");
	const char* path = std::getenv("HOMEPATH");
	if(drive && path) return std::string(drive) + std::string(path);
	return "C:\\Users\\Default";
#else
	const char* home = std::getenv("HOME");
	if(home) return std::string(home);
	return "/tmp";
#endif
}

std::string Platform::GetDefaultProjectsDir(){
#ifdef _WIN32
	return GetHomeDir() + "\\Documents\\SatyrAV\\projects";
#else
	return GetHomeDir() + "/satyrav/projects";
#endif
}

std::string Platform::GetDefaultConfigPath(){
#ifdef _WIN32
	return GetHomeDir() + "\\Documents\\SatyrAV\\config.toml";
#else
	return GetHomeDir() + "/.config/satyrav/config.toml";
#endif
}

std::string Platform::GetDefaultFontsDir(){
#ifdef _WIN32
	return GetHomeDir() + "\\Documents\\SatyrAV\\FONTS";
#else
	return GetHomeDir() + "/satyrav/FONTS";
#endif
}

std::string Platform::GetDefaultLogsDir(){
#ifdef _WIN32
	return GetHomeDir() + "\\Documents\\SatyrAV\\log";
#else
	return GetHomeDir() + "/satyrav/log";
#endif
}

void Platform::EnsureDirectoryExists(const std::string& path){
	if(!fs::exists(path)){
		fs::create_directories(path);
	}
}

fs::path Platform::Utf8ToPath(const std::string& utf8){
#ifdef _WIN32
	if(utf8.empty()) return fs::path();
	int wlen = MultiByteToWideChar(CP_UTF8, 0,
		utf8.c_str(), (int)utf8.size(), nullptr, 0);
	if(wlen <= 0) return fs::path(utf8);
	std::wstring wide((size_t)wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0,
		utf8.c_str(), (int)utf8.size(), wide.data(), wlen);
	return fs::path(wide);
#else
	return fs::path(utf8);
#endif
}

} // namespace SatyrAV
