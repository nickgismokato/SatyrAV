#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

class SceneParser{
public:
	static Scene ParseFile(const std::string& scenePath);

private:
	static Command ParseCommand(const std::string& line, int lineNumber);
	static std::vector<Command> ParseLoop(
		const std::vector<std::string>& lines, size_t& index, int lineNumber);
};

} // namespace SatyrAV
