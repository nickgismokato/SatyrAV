#pragma once
#include <string>

namespace SatyrAV{

class Platform{
public:
	static std::string GetDefaultProjectsDir();
	static std::string GetDefaultConfigPath();
	static std::string GetHomeDir();
	static void EnsureDirectoryExists(const std::string& path);
};

} // namespace SatyrAV
