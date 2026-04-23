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
	static void GenerateExampleScene(const std::string& path);
};

} // namespace SatyrAV
