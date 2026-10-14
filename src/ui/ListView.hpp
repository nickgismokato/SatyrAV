#pragma once
#include "ui/Widget.hpp"
#include <vector>
#include <string>
#include <functional>

namespace SatyrAV{

class ListView : public Widget{
public:
	void SetItems(const std::vector<std::string>& items);
	void Draw(Renderer& r, TextRenderer& text) override;

	void MoveUp(int steps = 1);
	void MoveDown(int steps = 1);
	void JumpToTop();
	void JumpToBottom();

	int GetSelectedIndex() const;
	const std::string& GetSelectedItem() const;

	bool showLineNumbers = false;
	int maxDisplayChars  = 0; // 0 = no limit
	Colour selectedColour = Colours::ORANGE;

private:
	std::vector<std::string> items;
	int selectedIndex = 0;
	int scrollOffset  = 0;

	std::string TruncateItem(const std::string& item) const;
};

} // namespace SatyrAV
