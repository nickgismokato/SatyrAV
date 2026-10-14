#pragma once
#include <string>
#include <filesystem>

namespace SatyrAV{

inline std::string FindAsset(const std::string& name){
	const char* SEARCH_PATHS[] = {
		"assets/",
		"../assets/",
		"/usr/local/share/satyr-av/assets/",
		"/usr/share/satyr-av/assets/",
		nullptr
	};
	for(int i = 0; SEARCH_PATHS[i]; i++){
		std::string path = std::string(SEARCH_PATHS[i]) + name;
		if(std::filesystem::exists(path)) return path;
	}
	return "assets/" + name;
}

} // namespace SatyrAV
