#pragma once
#include <string>

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
};

} // namespace SatyrAV
