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
	// (1.6.1) Optional italic/bold/bold-italic face paths. Empty = no
	// dedicated face for that style; the renderer falls back to regular.
	// Project-level [Options] in schema.toml can override these per project.
	std::string fontPathItalic     = "";
	std::string fontPathBold       = "";
	std::string fontPathBoldItalic = "";
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

	// (1.6.3) Output audio device. Empty string = default device (matches
	// pre-1.6.3 behaviour where SDL_OpenAudioDevice was called with
	// `device = nullptr`). Non-empty = a device name as returned by
	// SDL_GetAudioDeviceName(i, 0). The Options screen lists every
	// detected output device so the user can pick a specific HDMI/USB
	// interface for production setups where sound and video go to
	// different boxes.
	std::string audioDeviceName = "";

	// Paths
	std::string projectsDir  = "";
	std::string lastProject  = "";
	std::string configPath   = "";
	// (1.6.1) Shared fonts directory. Sits parallel to `projectsDir` —
	// `~/Documents/SatyrAV/FONTS` on Windows, `~/satyrav/FONTS` elsewhere
	// — and `font.path` / `font.italic` / etc. accept either an absolute
	// path or a bare filename that's resolved against this directory.
	std::string fontsDir     = "";

	// Resolve a font reference: if `name` already contains a path
	// separator or drive letter it's returned as-is; otherwise it's
	// prepended with `fontsDir`. Empty `name` returns empty.
	std::string ResolveFontPath(const std::string& name) const;

	// Video preload
	int videoPreloadBudgetMB  = 4096; // 4 GB total across all preloaded videos
	int videoPreloadMaxSeconds = 60;  // per-video cap on preload length

private:
	Config() = default;
};

} // namespace SatyrAV
