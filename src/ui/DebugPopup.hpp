#pragma once
#include "ui/Popup.hpp"
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

struct DebugInfo{
	// Last command
	std::string lastCommand;
	std::string lastCommandPath;
	bool lastCommandFileExists = false;

	// File info
	std::string fileInfo; // bitrate, resolution, etc.

	// Memory
	size_t cacheSize = 0;
	int preloadedFiles = 0;
	int videoBufferedFrames = 0;
	int videoRingSize = 0;

	// Display
	int slaveWidth = 0;
	int slaveHeight = 0;
	int slaveDisplayIndex = 0;

	// Fonts
	int globalFontSize  = 0;
	int projectFontSize = 0;
	int sceneFontSize   = 0;

	// Scene info
	std::string currentSceneName;
	std::string currentAktName;
};

class DebugPopup : public Popup{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	DebugInfo info;
};

} // namespace SatyrAV
