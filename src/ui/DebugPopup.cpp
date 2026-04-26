#include "ui/DebugPopup.hpp"
#include "core/Version.hpp"
#include <cstdio>

namespace SatyrAV{

void DebugPopup::Draw(Renderer& r, TextRenderer& text){
	if(!visible) return;

	int lineH = text.GetLineHeight() + 2;

	// Two-column layout
	int colW = 340;
	int totalW = colW * 2 + 40;
	// Count max lines in either column to compute height
	int leftLines = 12; // version + scene(3) + command(4) + display(3)
	int rightLines = 10; // fonts(4) + cache(3) + blank(3)
	int maxLines = (leftLines > rightLines) ? leftLines : rightLines;
	int totalH = maxLines * lineH + 60 + lineH + 8; // + footer hint row

	DrawBox(r, text, "Debug", totalW, totalH);
	auto* renderer = r.GetMaster();

	Colour hdr = Colours::ORANGE;
	Colour txt = Colours::WHITE;
	Colour ok  = Colours::LIME_GREEN;
	Colour err = {0xFF, 0x44, 0x44, 0xFF};

	// ── Left column ──────────────────────────────────────
	int lx = boxX + 16;
	int y = boxY + 40;

	std::string ver = "SatyrAV v" + std::string(SATYR_AV_VERSION);
	text.DrawText(renderer, ver, lx, y, hdr); y += lineH + 2;

	text.DrawText(renderer, "Scene", lx, y, hdr); y += lineH;
	text.DrawText(renderer, "  Akt:   " + info.currentAktName, lx, y, txt); y += lineH;
	text.DrawText(renderer, "  Scene: " + info.currentSceneName, lx, y, txt); y += lineH;
	y += 4;

	text.DrawText(renderer, "Last command", lx, y, hdr); y += lineH;
	std::string cmdDisplay = info.lastCommand;
	if(cmdDisplay.size() > 38) cmdDisplay = cmdDisplay.substr(0, 35) + "...";
	text.DrawText(renderer, "  " + cmdDisplay, lx, y, txt); y += lineH;
	if(!info.lastCommandPath.empty()){
		std::string pathDisplay = info.lastCommandPath;
		if(pathDisplay.size() > 36) pathDisplay = "..." + pathDisplay.substr(pathDisplay.size() - 33);
		text.DrawText(renderer, "  " + pathDisplay, lx, y, txt); y += lineH;
		Colour fc = info.lastCommandFileExists ? ok : err;
		std::string status = info.lastCommandFileExists ? "  EXISTS" : "  NOT FOUND";
		text.DrawText(renderer, status, lx, y, fc); y += lineH;
	}
	y += 4;

	text.DrawText(renderer, "Slave display", lx, y, hdr); y += lineH;
	std::string res = "  " + std::to_string(info.slaveWidth) + "x" + std::to_string(info.slaveHeight)
		+ "  [Display " + std::to_string(info.slaveDisplayIndex) + "]";
	text.DrawText(renderer, res, lx, y, txt);

	// ── Right column ─────────────────────────────────────
	int rx = boxX + colW + 24;
	y = boxY + 40 + lineH + 2; // align with left column after version

	text.DrawText(renderer, "Font sizes", rx, y, hdr); y += lineH;
	text.DrawText(renderer, "  Global:  " + std::to_string(info.globalFontSize), rx, y, txt); y += lineH;
	text.DrawText(renderer, "  Project: " + std::to_string(info.projectFontSize), rx, y, txt); y += lineH;
	text.DrawText(renderer, "  Scene:   " + std::to_string(info.sceneFontSize), rx, y, txt); y += lineH;
	y += 4;

	text.DrawText(renderer, "Cache", rx, y, hdr); y += lineH;
	text.DrawText(renderer, "  Files: " + std::to_string(info.preloadedFiles), rx, y, txt); y += lineH;
	char sizeBuf[64];
	if(info.cacheSize >= 1024 * 1024){
		snprintf(sizeBuf, sizeof(sizeBuf), "  Size: %.1f MB", (double)info.cacheSize / (1024.0 * 1024.0));
	} else{
		snprintf(sizeBuf, sizeof(sizeBuf), "  Size: %zu KB", info.cacheSize / 1024);
	}
	text.DrawText(renderer, sizeBuf, rx, y, txt); y += lineH;
	y += 4;

	text.DrawText(renderer, "Video buffer", rx, y, hdr); y += lineH;
	std::string vbuf = "  Frames: " + std::to_string(info.videoBufferedFrames)
		+ " / " + std::to_string(info.videoRingSize);
	text.DrawText(renderer, vbuf, rx, y, txt);

	// (1.6) Footer hint — sub-menus reachable from this popup.
	int footerY = boxY + totalH - lineH - 10;
	Colour hint = {0x90, 0xA0, 0xB0, 0xFF};
	text.DrawTextCentered(renderer,
		"Press S: Display rect    O: Overview Debug",
		boxX + totalW / 2, footerY, hint);
}

} // namespace SatyrAV
