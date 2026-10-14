#include "ui/Dropdown.hpp"

namespace SatyrAV{

void Dropdown::Draw(Renderer& r, TextRenderer& text){
	auto* renderer = r.GetMaster();
	int lineH = text.GetLineHeight();
	int boxH = 30;
	int textOffY = (boxH - lineH) / 2;

	// Label
	text.DrawText(renderer, label, x, y, Colours::WHITE);

	// Selected value box
	int boxY = y + lineH + 2;
	Colour border = focused ? Colours::ORANGE : Colours::WHITE;
	r.DrawRect(renderer, x, boxY, w, boxH, Colours::BACKGROUND);
	r.DrawRect(renderer, x, boxY, w, boxH, border, false);

	if(!options.empty()){
		text.DrawText(renderer, options[selectedIndex], x + 5, boxY + textOffY, Colours::ORANGE);
	}

	// Dropdown arrow
	text.DrawText(renderer, expanded ? "^" : "v", x + w - 20, boxY + textOffY, Colours::WHITE);
	// Expanded list is drawn separately via DrawOverlay() for z-ordering
}

void Dropdown::DrawOverlay(Renderer& r, TextRenderer& text){
	if(!expanded) return;

	auto* renderer = r.GetMaster();
	int lineH = text.GetLineHeight();
	int boxH = 30;
	int boxY = y + lineH + 2;
	int itemH = 25;
	int listH = (int)options.size() * itemH;
	int listY = boxY + boxH + 2;
	int itemTextOff = (itemH - lineH) / 2;

	// Solid background covering everything behind
	r.DrawRect(renderer, x, listY, w, listH, Colours::HEADER_BG);
	r.DrawRect(renderer, x, listY, w, listH, Colours::WHITE, false);

	for(int i = 0; i < (int)options.size(); i++){
		int iy = listY + i * itemH;
		if(i == selectedIndex){
			r.DrawRect(renderer, x + 1, iy, w - 2, itemH, Colours::HIGHLIGHT);
		}
		text.DrawText(renderer, options[i], x + 5, iy + itemTextOff, Colours::WHITE);
	}
}

void Dropdown::SetOptions(const std::vector<std::string>& opts){
	options = opts;
	selectedIndex = 0;
}

void Dropdown::Toggle(){ expanded = !expanded; }

void Dropdown::Close(){ expanded = false; }

bool Dropdown::IsExpanded() const{ return expanded; }

void Dropdown::SelectNext(){
	if(selectedIndex < (int)options.size() - 1) selectedIndex++;
}

void Dropdown::SelectPrev(){
	if(selectedIndex > 0) selectedIndex--;
}

int Dropdown::GetSelectedIndex() const{ return selectedIndex; }

void Dropdown::SetSelectedIndex(int index){
	if(index >= 0 && index < (int)options.size()){
		selectedIndex = index;
	}
}

const std::string& Dropdown::GetSelectedOption() const{
	return options[selectedIndex];
}

} // namespace SatyrAV
