#include "ui/Popup.hpp"

namespace SatyrAV{

void Popup::Toggle(){ visible = !visible; }
void Popup::Show(){ visible = true; }
void Popup::Hide(){ visible = false; dragging = false; }
bool Popup::IsVisible() const{ return visible; }

void Popup::HandleMouseDown(int mx, int my){
	if(!visible) return;
	// Check if click is on the title bar (top 32px of the box)
	if(mx >= boxX && mx <= boxX + popW && my >= boxY && my <= boxY + 32){
		dragging = true;
		dragOffX = mx - boxX;
		dragOffY = my - boxY;
	}
}

void Popup::HandleMouseUp(){
	dragging = false;
}

void Popup::HandleMouseMove(int mx, int my){
	if(!dragging) return;
	popX = mx - dragOffX;
	popY = my - dragOffY;
}

void Popup::DrawBox(Renderer& r, TextRenderer& text,
	const std::string& title, int width, int height){
	int screenW = r.GetMasterWidth();
	int screenH = r.GetMasterHeight();

	popW = width;
	popH = height;

	// Auto-center if no position set
	if(popX < 0) popX = (screenW - popW) / 2;
	if(popY < 0) popY = (screenH - popH) / 2;

	// Clamp to screen
	if(popX < 0) popX = 0;
	if(popY < 0) popY = 0;
	if(popX + popW > screenW) popX = screenW - popW;
	if(popY + popH > screenH) popY = screenH - popH;

	boxX = popX;
	boxY = popY;

	auto* renderer = r.GetMaster();

	// Solid opaque background
	r.DrawRect(renderer, boxX, boxY, popW, popH, Colours::BACKGROUND);
	// Border
	r.DrawRect(renderer, boxX, boxY, popW, popH, Colours::ORANGE, false);

	// Title bar
	r.DrawRect(renderer, boxX + 1, boxY + 1, popW - 2, 30, Colours::HEADER_BG);
	text.DrawTextCentered(renderer, title, boxX + popW / 2, boxY + 7, Colours::ORANGE);

	// Drag hint
	text.DrawText(renderer, "=", boxX + 6, boxY + 7, {0x55, 0x55, 0x55, 0xFF});
}

} // namespace SatyrAV
