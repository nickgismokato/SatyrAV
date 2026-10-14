#pragma once
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

class Popup{
public:
	virtual ~Popup() = default;
	virtual void Draw(Renderer& r, TextRenderer& text) = 0;

	void Toggle();
	void Show();
	void Hide();
	bool IsVisible() const;

	// Dragging
	void HandleMouseDown(int mx, int my);
	void HandleMouseUp();
	void HandleMouseMove(int mx, int my);

protected:
	bool visible = false;

	// Position (top-left of popup). -1 = auto-center
	int popX = -1, popY = -1;
	int popW = 0, popH = 0;

	// Drag state
	bool dragging = false;
	int dragOffX = 0, dragOffY = 0;

	// Draw the popup box background + title bar
	void DrawBox(Renderer& r, TextRenderer& text,
		const std::string& title, int width, int height);

	// After DrawBox, these hold the computed position
	int boxX = 0, boxY = 0;
};

} // namespace SatyrAV
