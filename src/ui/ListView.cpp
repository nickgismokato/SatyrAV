#include "ui/ListView.hpp"

namespace SatyrAV{

void ListView::SetItems(const std::vector<std::string>& newItems){
	items = newItems;
	selectedIndex = 0;
	scrollOffset = 0;
}

void ListView::Draw(Renderer& r, TextRenderer& text){
	auto* renderer = r.GetMaster();
	int lineH = text.GetLineHeight() + 4;
	int maxVisible = h / lineH;

	for(int i = 0; i < maxVisible && (scrollOffset + i) < (int)items.size(); i++){
		int idx = scrollOffset + i;
		int drawY = y + i * lineH;

		// Highlight selected
		if(idx == selectedIndex && focused){
			r.DrawRect(renderer, x, drawY, w, lineH, Colours::HIGHLIGHT);
		}

		// Line number
		int textX = x + 5;
		if(showLineNumbers){
			std::string num = std::to_string(idx + 1);
			text.DrawText(renderer, num, textX, drawY + 2, {0x55, 0x55, 0x55, 0xFF});
			textX += 40;
		}

		// Item text
		Colour c = (idx == selectedIndex) ? selectedColour : Colours::WHITE;
		text.DrawText(renderer, TruncateItem(items[idx]), textX, drawY + 2, c);
	}

	// Overflow indicators
	if(scrollOffset > 0){
		text.DrawText(renderer, "...", x + w / 2 - 10, y - lineH, Colours::WHITE);
	}
	if(scrollOffset + maxVisible < (int)items.size()){
		text.DrawText(renderer, "...", x + w / 2 - 10, y + maxVisible * lineH, Colours::WHITE);
	}
}

void ListView::MoveUp(int steps){
	selectedIndex = std::max(0, selectedIndex - steps);
	// Adjust scroll
	if(selectedIndex < scrollOffset){
		scrollOffset = selectedIndex;
	}
}

void ListView::MoveDown(int steps){
	selectedIndex = std::min((int)items.size() - 1, selectedIndex + steps);
	// Adjust scroll
	int lineH = 20; // approximate
	int maxVisible = h / lineH;
	if(selectedIndex >= scrollOffset + maxVisible){
		scrollOffset = selectedIndex - maxVisible + 1;
	}
}

void ListView::JumpToTop(){ selectedIndex = 0; scrollOffset = 0; }

void ListView::JumpToBottom(){
	selectedIndex = std::max(0, (int)items.size() - 1);
}

int ListView::GetSelectedIndex() const{ return selectedIndex; }

const std::string& ListView::GetSelectedItem() const{
	return items[selectedIndex];
}

std::string ListView::TruncateItem(const std::string& item) const{
	if(maxDisplayChars <= 0 || (int)item.size() <= maxDisplayChars){
		return item;
	}
	return item.substr(0, maxDisplayChars - 3) + "...";
}

} // namespace SatyrAV
