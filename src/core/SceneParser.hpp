#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

class SceneParser{
public:
	static Scene ParseFile(const std::string& scenePath);

private:
	static Command ParseCommand(const std::string& line, int lineNumber);
	// (1.8.1) NOT UNIT-TESTED — dead code. This is a stub that ignores its
	// arguments and returns {}; nothing calls it. Real loop expansion lives
	// in ExpandLoop inside SceneParser.cpp and *is* covered. Either delete
	// this declaration or give it a body — until then there is no behaviour
	// worth pinning down.
	static std::vector<Command> ParseLoop(
		const std::vector<std::string>& lines, size_t& index, int lineNumber);
};

} // namespace SatyrAV
