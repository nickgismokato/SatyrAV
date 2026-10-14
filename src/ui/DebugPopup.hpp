#pragma once
#include "ui/Popup.hpp"
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

// (2.0.0) Which decode path the *currently active* video is using.
// Preloaded-but-idle videos each choose independently at Setup time; this
// reports the one actually on screen, which is what the operator cares about.
enum class DecodeMode{
	None,      // no video loaded
	HardwareZeroCopy, // D3D11VA, frames never leave the GPU (2.0.0)
	Hardware,  // D3D11VA, frames read back through system RAM
	Software   // FFmpeg multi-threaded software decoders
};

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
	DecodeMode videoDecodeMode = DecodeMode::None;
	// (2.0.0) Standalone sounds playing concurrently. Can exceed 1 now that
	// SFX mix rather than replacing one another.
	int activeSounds = 0;

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

	// (2.0.2) Backstage screen: the last POST's outcome in a few words.
	enum class BackstageLook{ Off, Neutral, Ok, Failed };
	BackstageLook backstageLook = BackstageLook::Off;
	std::string backstageText = "off";
};

class DebugPopup : public Popup{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	DebugInfo info;
};

} // namespace SatyrAV
