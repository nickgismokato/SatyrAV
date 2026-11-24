#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

// Shared with the project scanner (core/ProjectScanner), so it reads a
// scene file the way the parser does.
// Start of a `#` comment, or npos; `#` inside quotes is text.
size_t FindCommentStart(const std::string& line);
// Same for an [Options] line, where a value may be a `#RRGGBB` colour.
size_t FindOptionCommentStart(const std::string& line);
// Particle type by name, any case; false for an unknown name.
bool ParseParticleTypeStrict(const std::string& s, ParticleType& out);

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
