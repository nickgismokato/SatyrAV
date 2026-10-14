#pragma once
#include "screens/Screen.hpp"
#include "ui/TextField.hpp"
#include "ui/Dropdown.hpp"
#include <vector>

namespace SatyrAV{

class OptionsScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	Dropdown fontColourDropdown;
	TextField fontSizeField;
	TextField projectsDirField;
	Dropdown displayDropdown;
	// (1.5) Slave-window mode: Fullscreen (production) or Windowed
	// (single-monitor development on the primary display).
	Dropdown slaveModeDropdown;
	// (1.5) Capture-mirror output display. "Off" plus every display that
	// isn't currently the slave's display. Runtime-ignored while in
	// windowed-slave mode (see App::OpenProject), but the user's choice
	// persists in config.toml.
	Dropdown captureDropdown;
	// Maps capture-dropdown option index → physical display index.
	// Option 0 ("Off") maps to -1.
	std::vector<int> captureOptionToDisplay;

	// (1.6.3) Audio output device. Option 0 = "Default" (system default,
	// stored as empty string in config). Subsequent options enumerate
	// every detected output device by name.
	Dropdown audioDeviceDropdown;
	std::vector<std::string> audioDeviceNames; // option idx → SDL device name

	int focusedField = 0;
	// (1.6.3) +1 for the audio device dropdown.
	static constexpr int FIELD_COUNT = 8; // 7 fields + save button

	void SaveConfig();
	void UpdateFocus();
	void PopulateDisplays();
	void PopulateCaptureOptions();
	void PopulateAudioDevices();
};

} // namespace SatyrAV
