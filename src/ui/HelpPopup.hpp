#pragma once
#include "ui/Popup.hpp"

namespace SatyrAV{

class HelpPopup : public Popup{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	// Set to true when inside a loaded project
	bool inProject = false;
};

} // namespace SatyrAV
