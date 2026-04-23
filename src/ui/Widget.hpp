#pragma once
#include "core/Types.hpp"
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include <string>

namespace SatyrAV{

class Widget{
public:
	virtual ~Widget() = default;
	virtual void Draw(Renderer& r, TextRenderer& text) = 0;

	int x = 0, y = 0, w = 0, h = 0;
	bool focused = false;
};

} // namespace SatyrAV
