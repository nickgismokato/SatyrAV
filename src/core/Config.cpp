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
	// Ensure directories exist
	Platform::EnsureDirectoryExists(projectsDir);
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
		masterWidth    = tbl["display"]["width"].value_or(1280);
		masterHeight   = tbl["display"]["height"].value_or(720);
		fullscreen     = tbl["display"]["fullscreen"].value_or(false);
		slaveDisplayIndex = tbl["display"]["slave_index"].value_or(1);
		slaveWindowed     = tbl["display"]["slave_windowed"].value_or(false);
		captureEnabled      = tbl["display"]["capture_enabled"].value_or(false);
		captureDisplayIndex = tbl["display"]["capture_display"].value_or(0);
		projectsDir    = tbl["paths"]["projects"].value_or(std::string(""));
		lastProject    = tbl["paths"]["last_project"].value_or(std::string(""));
		videoPreloadBudgetMB   = tbl["video"]["preload_budget_mb"].value_or(4096);
		videoPreloadMaxSeconds = tbl["video"]["preload_max_seconds"].value_or(60);
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
	out << "path = '" << fontPath << "'\n\n";
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
	out << "last_project = '" << lastProject << "'\n\n";
	out << "[video]\n";
	out << "preload_budget_mb = " << videoPreloadBudgetMB << "\n";
	out << "preload_max_seconds = " << videoPreloadMaxSeconds << "\n";
}

} // namespace SatyrAV
