#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

class Project{
public:
	static ProjectData Create(
		RevyType type, int year,
		const std::string& creatorName,
		const std::string& projectPath,
		const std::string& revyName = "");

	static ProjectData Load(const std::string& projectPath);
	static void Save(const ProjectData& data);

private:
	static void GenerateDefaultSchema(const std::string& path, const ProjectData& data);
	// (1.8.0) Writes the four tutorial scenes listed in the generated
	// schema's two akter — text, picture, particle, and group/macro
	// showcases. `scenesDir` is the project's `scenes/` folder.
	static void GenerateExampleScenes(const std::string& scenesDir);
};

} // namespace SatyrAV
