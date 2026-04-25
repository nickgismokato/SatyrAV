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
	static std::string GetHomeDir();
	static void EnsureDirectoryExists(const std::string& path);
};

} // namespace SatyrAV
