#pragma once
#include "ui/Widget.hpp"
#include <vector>
#include <string>

namespace SatyrAV{

class Dropdown : public Widget{
public:
	void Draw(Renderer& r, TextRenderer& text) override;
	void DrawOverlay(Renderer& r, TextRenderer& text);

	void SetOptions(const std::vector<std::string>& opts);
	void Toggle();
	void Close();
	void SelectNext();
	void SelectPrev();
	bool IsExpanded() const;

	int GetSelectedIndex() const;
	void SetSelectedIndex(int index);
	const std::string& GetSelectedOption() const;

	std::string label;

private:
	std::vector<std::string> options;
	int selectedIndex = 0;
	bool expanded = false;
};

} // namespace SatyrAV
