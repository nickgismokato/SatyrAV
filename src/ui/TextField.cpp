#include "ui/TextField.hpp"

namespace SatyrAV{

void TextField::Draw(Renderer& r, TextRenderer& text){
	auto* renderer = r.GetMaster();
	int lineH = text.GetLineHeight();
	int boxH = 30;
	int textOffY = (boxH - lineH) / 2;

	// Label
	text.DrawText(renderer, label, x, y, Colours::WHITE);

	// Input box
	int boxY = y + lineH + 2;
	Colour border = focused ? Colours::ORANGE : Colours::WHITE;
	r.DrawRect(renderer, x, boxY, w, boxH, Colours::BACKGROUND);
	r.DrawRect(renderer, x, boxY, w, boxH, border, false);

	// Content text — vertically centered
	text.DrawText(renderer, content, x + 5, boxY + textOffY, Colours::ORANGE);

	// Cursor
	if(focused){
		int cursorX = x + 5 + text.MeasureWidth(content);
		int cursorH = lineH;
		int cursorY = boxY + textOffY;
		r.DrawRect(renderer, cursorX, cursorY, 2, cursorH, Colours::ORANGE);
	}
}

void TextField::SetText(const std::string& t){
	if((int)t.size() <= maxLength){
		content = t;
	}
}

const std::string& TextField::GetText() const{ return content; }

void TextField::AppendText(const std::string& ch){
	if((int)(content.size() + ch.size()) <= maxLength){
		content += ch;
	}
}

void TextField::RemoveLastChar(){
	if(!content.empty()){
		content.pop_back();
	}
}

void TextField::Clear(){
	content.clear();
}

} // namespace SatyrAV
