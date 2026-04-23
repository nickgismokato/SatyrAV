#include "ui/HelpPopup.hpp"
#include "core/Version.hpp"

namespace SatyrAV{

void HelpPopup::Draw(Renderer& r, TextRenderer& text){
	if(!visible) return;

	int lineH = text.GetLineHeight() + 2;
	int lines = inProject ? 19 : 14;
	int contentH = lines * lineH + 60;
	int contentW = 520;

	DrawBox(r, text, "Help - Controls", contentW, contentH);
	auto* renderer = r.GetMaster();

	int x = boxX + 20;
	int y = boxY + 40;
	Colour hdr = Colours::ORANGE;
	Colour txt = Colours::WHITE;
	Colour dim = {0x88, 0x88, 0x88, 0xFF};

	text.DrawText(renderer, "General", x, y, hdr); y += lineH;
	text.DrawText(renderer, "  H              Toggle this help menu", x, y, txt); y += lineH;
	text.DrawText(renderer, "  D              Toggle debug menu", x, y, txt); y += lineH;
	text.DrawText(renderer, "  ESC            Go back / quit", x, y, txt); y += lineH;
	text.DrawText(renderer, "  ENTER          Select / confirm", x, y, txt); y += lineH;
	text.DrawText(renderer, "  SHIFT+ENTER    Previous command", x, y, txt); y += lineH;
	text.DrawText(renderer, "  TAB            Next field (in forms)", x, y, txt); y += lineH;
	text.DrawText(renderer, "  UP/DOWN        Navigate", x, y, txt); y += lineH;
	y += 6;

	if(inProject){
		text.DrawText(renderer, "In a project", x, y, hdr); y += lineH;
		text.DrawText(renderer, "  RIGHT          Enter akt/scene/commands", x, y, txt); y += lineH;
		text.DrawText(renderer, "  LEFT           Go back a column", x, y, txt); y += lineH;
		text.DrawText(renderer, "  ENTER          Execute current command", x, y, txt); y += lineH;
		text.DrawText(renderer, "  SHIFT+UP       Jump 5 items up", x, y, txt); y += lineH;
		text.DrawText(renderer, "  SHIFT+DOWN     Jump 5 items down", x, y, txt); y += lineH;
		text.DrawText(renderer, "  CTRL+UP        Jump to top", x, y, txt); y += lineH;
		text.DrawText(renderer, "  CTRL+DOWN      Jump to bottom", x, y, txt); y += lineH;
		text.DrawText(renderer, "  K              Toggle video pause", x, y, txt); y += lineH;
		text.DrawText(renderer, "  M              Toggle music pause", x, y, txt); y += lineH;
		text.DrawText(renderer, "  U              Reload scene files from disk", x, y, txt); y += lineH;
	} else{
		text.DrawText(renderer, "Menus", x, y, hdr); y += lineH;
		text.DrawText(renderer, "  UP/DOWN        Select option", x, y, txt); y += lineH;
		text.DrawText(renderer, "  ENTER          Confirm selection", x, y, txt); y += lineH;
		text.DrawText(renderer, "  ESC            Return to main menu", x, y, txt); y += lineH;
	}

	y += 8;
	text.DrawText(renderer, "Contact: nickvillumlaursen@gmail.com", x, y, dim); y += lineH;
	std::string ver = "SatyrAV v" + std::string(SATYR_AV_VERSION);
	text.DrawText(renderer, ver, x, y, dim);
}

} // namespace SatyrAV
