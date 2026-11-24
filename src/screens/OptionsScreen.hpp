#pragma once
#include "screens/Screen.hpp"
#include "ui/TextField.hpp"
#include "ui/Dropdown.hpp"
#include <memory>
#include <mutex>
#include <string>
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

	// (2.1.0) Clock and stopwatches on the master window: Off / On.
	Dropdown showClockDropdown;
	// Show log and per-act .srt / .ass: Off / On.
	Dropdown showLogDropdown;

	// Native folder dialog for the projects directory, opened with ENTER on
	// that field. SDL may answer on another thread, and after this screen
	// has been left, so the answer goes into a shared, locked slot that
	// Update() picks up on the main thread.
	struct FolderPick{
		std::mutex mtx;
		bool open = false;     // a dialog is showing
		bool ready = false;    // `path` holds a new choice
		std::string path;
		std::string error;     // set if the dialog could not be shown
	};
	std::shared_ptr<FolderPick> folderPick = std::make_shared<FolderPick>();
	std::string folderPickStatus; // shown beside the field
	void OpenFolderDialog();
	static void SDLCALL OnFolderChosen(void* userdata,
		const char* const* filelist, int filter);

	int focusedField = 0;
	static constexpr int FIELD_COUNT = 10; // 9 fields + save button

	void SaveConfig();
	void UpdateFocus();
	void PopulateDisplays();
	void PopulateCaptureOptions();
	void PopulateAudioDevices();
};

} // namespace SatyrAV
