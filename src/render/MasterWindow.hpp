#pragma once
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include "core/CueEngine.hpp"

namespace SatyrAV{

// Draws the master control UI (header + three-column navigator)
class MasterWindow{
public:
	void DrawHeader(Renderer& r, TextRenderer& text, const CueEngine& engine);
	void DrawNavigator(Renderer& r, TextRenderer& text, const CueEngine& engine,
		int primedAkt, int primedScene, int primedCmd, int activeColumn);
};

} // namespace SatyrAV
