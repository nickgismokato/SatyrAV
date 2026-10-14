#pragma once
#include <string>
#include <filesystem>

namespace SatyrAV{

class Platform{
public:
	static std::string GetDefaultProjectsDir();
	static std::string GetDefaultConfigPath();
	// (1.6.1) Shared fonts directory parallel to the projects folder.
	// Projects can reference font files here by bare name (e.g.
	// `FontBold = "Inter-Bold.ttf"`) and SatyrAV will resolve the path
	// against this directory. Cross-project font reuse without copying.
	static std::string GetDefaultFontsDir();
	// (1.6.3) Logs directory — `~/Documents/SatyrAV/log` on Windows,
	// `~/satyrav/log` elsewhere. Lives parallel to the projects folder
	// so log retention can be managed without touching project data.
	static std::string GetDefaultLogsDir();
	static std::string GetHomeDir();
	static void EnsureDirectoryExists(const std::string& path);

	// (1.6.6) Convert a UTF-8 std::string to a std::filesystem::path that
	// the OS can resolve. On Windows, MinGW's libstdc++ interprets the
	// narrow std::string overload as the system codepage (ANSI), so
	// .ngk files whose names contain æøå (UTF-8 bytes outside ASCII)
	// fail to open. Going through a wide string fixes that round-trip.
	// On POSIX the std::string is already taken as UTF-8 and we just
	// construct the path directly.
	static std::filesystem::path Utf8ToPath(const std::string& utf8);
};

} // namespace SatyrAV
