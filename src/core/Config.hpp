#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

class Config{
public:
	static Config& Instance();

	void Load(const std::string& path);
	void Save();
	void Save(const std::string& path);
	void EnsureDefaults();

	// Font
	std::string fontPath     = "";
	int fontSize             = 25;
	Colour fontColour        = Colours::ORANGE;

	// Display
	int masterWidth          = 1280;
	int masterHeight         = 720;
	bool fullscreen          = false;
	int slaveDisplayIndex    = 1;
	// (1.5) Dev-mode slave window. When true, InitSlave opens the slave
	// as a resizable, decorated window on the primary display instead of
	// fullscreen on `slaveDisplayIndex`. Production setups leave this off.
	bool slaveWindowed       = false;
	// (1.5) Capture-mirror window. Borderless-fullscreen on a separate
	// display that shows the slave's pre-targeted-display composite at
	// slave logical resolution — feeds a capture card / recorder.
	// Disabled at runtime when slaveWindowed is on, but the values here
	// are preserved so switching back to fullscreen restores the choice.
	bool captureEnabled      = false;
	int  captureDisplayIndex = 0;

	// Paths
	std::string projectsDir  = "";
	std::string lastProject  = "";
	std::string configPath   = "";

	// Video preload
	int videoPreloadBudgetMB  = 4096; // 4 GB total across all preloaded videos
	int videoPreloadMaxSeconds = 60;  // per-video cap on preload length

private:
	Config() = default;
};

} // namespace SatyrAV
