#include "screens/OptionsScreen.hpp"
#include "App.hpp"
#include "core/Config.hpp"

namespace SatyrAV{

void OptionsScreen::OnEnter(){
	focusedField = 0;

	fontColourDropdown.label = "Font Colour";
	fontColourDropdown.SetOptions({"Orange", "White", "Lime Green"});

	fontSizeField.label = "Font Size";
	fontSizeField.maxLength = 3;

	projectsDirField.label = "Projects Directory";
	projectsDirField.maxLength = 256;

	displayDropdown.label = "Secondary Display";
	PopulateDisplays();

	// (1.5) Slave window mode.
	slaveModeDropdown.label = "Slave Window Mode";
	slaveModeDropdown.SetOptions({"Fullscreen", "Windowed (dev)"});

	// (1.5) Capture-mirror display. Label set here; options populated
	// *after* displayDropdown is selected so the hard-guard filter sees
	// the right slave display.
	captureDropdown.label = "Capture Mirror";

	// Load current values
	auto& config = Config::Instance();
	fontSizeField.SetText(std::to_string(config.fontSize));
	projectsDirField.SetText(config.projectsDir);
	displayDropdown.SetSelectedIndex(config.slaveDisplayIndex);
	slaveModeDropdown.SetSelectedIndex(config.slaveWindowed ? 1 : 0);

	PopulateCaptureOptions();

	// Map current captureDisplayIndex to a dropdown option. Option 0 = Off.
	int captureOpt = 0;
	if(config.captureEnabled){
		for(size_t i = 1; i < captureOptionToDisplay.size(); i++){
			if(captureOptionToDisplay[i] == config.captureDisplayIndex){
				captureOpt = (int)i;
				break;
			}
		}
	}
	captureDropdown.SetSelectedIndex(captureOpt);

	// Set font colour dropdown to match saved config
	if(config.fontColour.r == Colours::WHITE.r && config.fontColour.g == Colours::WHITE.g){
		fontColourDropdown.SetSelectedIndex(1);
	} else if(config.fontColour.r == Colours::LIME_GREEN.r && config.fontColour.g == Colours::LIME_GREEN.g){
		fontColourDropdown.SetSelectedIndex(2);
	} else{
		fontColourDropdown.SetSelectedIndex(0); // Orange default
	}

	UpdateFocus();
}

void OptionsScreen::OnExit(){
	app->GetInput().SetTextInputMode(false);
}

void OptionsScreen::PopulateDisplays(){
	auto& r = app->GetRenderer();
	int count = r.GetDisplayCount();
	std::vector<std::string> names;
	for(int i = 0; i < count; i++){
		names.push_back(std::to_string(i) + ": " + r.GetDisplayName(i));
	}
	if(names.empty()) names.push_back("0: No display found");
	displayDropdown.SetOptions(names);
}

void OptionsScreen::PopulateCaptureOptions(){
	auto& r = app->GetRenderer();
	int count = r.GetDisplayCount();

	// Hard guard: never list the slave's currently-selected display as a
	// capture target. Reads the *live* displayDropdown selection (not
	// config) so the filter tracks in-progress edits before save.
	int currentSlaveIdx = displayDropdown.GetSelectedIndex();

	captureOptionToDisplay.clear();
	std::vector<std::string> names;
	names.push_back("Off");
	captureOptionToDisplay.push_back(-1);
	for(int i = 0; i < count; i++){
		if(i == currentSlaveIdx) continue;
		names.push_back(std::to_string(i) + ": " + r.GetDisplayName(i));
		captureOptionToDisplay.push_back(i);
	}
	captureDropdown.SetOptions(names);
}

void OptionsScreen::HandleInput(InputAction action){
	auto& input = app->GetInput();

	if(action == InputAction::Quit){
		app->SwitchScreen(ScreenID::MainMenu);
		return;
	}

	if(action == InputAction::TextInput){
		if(focusedField == 1){
			fontSizeField.AppendText(input.GetLastTextInput());
		} else if(focusedField == 2){
			projectsDirField.AppendText(input.GetLastTextInput());
		}
		return;
	}

	if(action == InputAction::Backspace){
		if(focusedField == 1){
			fontSizeField.RemoveLastChar();
		} else if(focusedField == 2){
			projectsDirField.RemoveLastChar();
		}
		return;
	}

	// Dropdown expanded handling
	Dropdown* activeDropdown = nullptr;
	if(focusedField == 0) activeDropdown = &fontColourDropdown;
	if(focusedField == 3) activeDropdown = &displayDropdown;
	if(focusedField == 4) activeDropdown = &slaveModeDropdown;
	if(focusedField == 5) activeDropdown = &captureDropdown;

	if(activeDropdown && activeDropdown->IsExpanded()){
		switch(action){
			case InputAction::NavUp:   activeDropdown->SelectPrev(); return;
			case InputAction::NavDown: activeDropdown->SelectNext(); return;
			case InputAction::Execute: activeDropdown->Close(); return;
			default: break;
		}
	}

	if(activeDropdown && action == InputAction::Execute){
		activeDropdown->Toggle();
		return;
	}

	// Enter on save button
	if(focusedField == FIELD_COUNT - 1 && action == InputAction::Execute){
		SaveConfig();
		return;
	}

	// Navigate fields
	if(action == InputAction::NavUp){
		focusedField = (focusedField - 1 + FIELD_COUNT) % FIELD_COUNT;
		fontColourDropdown.Close();
		displayDropdown.Close();
		slaveModeDropdown.Close();
		captureDropdown.Close();
		UpdateFocus();
	} else if(action == InputAction::NavDown || action == InputAction::Tab){
		focusedField = (focusedField + 1) % FIELD_COUNT;
		fontColourDropdown.Close();
		displayDropdown.Close();
		slaveModeDropdown.Close();
		captureDropdown.Close();
		UpdateFocus();
	}
}

void OptionsScreen::UpdateFocus(){
	fontColourDropdown.focused = (focusedField == 0);
	fontSizeField.focused      = (focusedField == 1);
	projectsDirField.focused   = (focusedField == 2);
	displayDropdown.focused    = (focusedField == 3);
	slaveModeDropdown.focused  = (focusedField == 4);
	captureDropdown.focused    = (focusedField == 5);

	bool needsText = (focusedField == 1 || focusedField == 2);
	app->GetInput().SetTextInputMode(needsText);
}

void OptionsScreen::SaveConfig(){
	auto& config = Config::Instance();

	switch(fontColourDropdown.GetSelectedIndex()){
		case 0: config.fontColour = Colours::ORANGE; break;
		case 1: config.fontColour = Colours::WHITE; break;
		case 2: config.fontColour = Colours::LIME_GREEN; break;
	}

	try{
		config.fontSize = std::stoi(fontSizeField.GetText());
	} catch(...){}

	config.projectsDir = projectsDirField.GetText();
	config.slaveDisplayIndex = displayDropdown.GetSelectedIndex();
	config.slaveWindowed = (slaveModeDropdown.GetSelectedIndex() == 1);

	// Capture mirror: option 0 = Off, else physical display via map.
	int capOpt = captureDropdown.GetSelectedIndex();
	if(capOpt <= 0 || capOpt >= (int)captureOptionToDisplay.size()){
		config.captureEnabled = false;
		// Leave captureDisplayIndex alone so the previous choice persists.
	} else{
		config.captureEnabled = true;
		config.captureDisplayIndex = captureOptionToDisplay[capOpt];
	}

	config.Save();

	app->SwitchScreen(ScreenID::MainMenu);
}

void OptionsScreen::Update(float deltaTime){
	(void)deltaTime;
}

void OptionsScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();
	int centerX = r.GetMasterWidth() / 2;

	text.DrawTextCentered(renderer, "Options",
		centerX, 30, Colours::WHITE);

	text.DrawTextCentered(renderer, "ESC: Back  |  TAB/UP/DOWN: Navigate  |  ENTER: Select/Save",
		centerX, r.GetMasterHeight() - 30, Colours::WHITE);

	int fieldW = 400;
	int fieldX = centerX - fieldW / 2;
	int y = 80;

	fontColourDropdown.x = fieldX; fontColourDropdown.y = y;
	fontColourDropdown.w = fieldW;
	fontColourDropdown.Draw(r, text);

	y += 80;
	fontSizeField.x = fieldX; fontSizeField.y = y;
	fontSizeField.w = fieldW;
	fontSizeField.Draw(r, text);

	y += 80;
	projectsDirField.x = fieldX; projectsDirField.y = y;
	projectsDirField.w = fieldW;
	projectsDirField.Draw(r, text);

	y += 80;
	displayDropdown.x = fieldX; displayDropdown.y = y;
	displayDropdown.w = fieldW;
	displayDropdown.Draw(r, text);

	y += 80;
	slaveModeDropdown.x = fieldX; slaveModeDropdown.y = y;
	slaveModeDropdown.w = fieldW;
	slaveModeDropdown.Draw(r, text);

	// Keep the capture dropdown's visible set in sync with the currently
	// selected slave display, so flipping slave display here immediately
	// removes it from the capture options.
	if((int)captureOptionToDisplay.size() > 0){
		// captureOptionToDisplay[1..] enumerates all non-slave displays
		// at the time we last populated. If the user changed slave display
		// since, rebuild.
		int slaveIdx = displayDropdown.GetSelectedIndex();
		bool needsRebuild = false;
		for(size_t i = 1; i < captureOptionToDisplay.size(); i++){
			if(captureOptionToDisplay[i] == slaveIdx){ needsRebuild = true; break; }
		}
		if(needsRebuild){
			int prevDisp = -1;
			int selOpt = captureDropdown.GetSelectedIndex();
			if(selOpt > 0 && selOpt < (int)captureOptionToDisplay.size()){
				prevDisp = captureOptionToDisplay[selOpt];
			}
			PopulateCaptureOptions();
			int restore = 0;
			for(size_t i = 1; i < captureOptionToDisplay.size(); i++){
				if(captureOptionToDisplay[i] == prevDisp){ restore = (int)i; break; }
			}
			captureDropdown.SetSelectedIndex(restore);
		}
	}

	y += 80;
	captureDropdown.x = fieldX; captureDropdown.y = y;
	captureDropdown.w = fieldW;
	captureDropdown.Draw(r, text);
	// Hint: when Windowed slave mode is currently selected, the capture
	// mirror is suppressed at runtime (see App::OpenProject). The setting
	// still saves; we just tell the user it's dormant right now.
	if(slaveModeDropdown.GetSelectedIndex() == 1){
		text.DrawText(renderer, "(ignored in windowed mode)",
			fieldX + fieldW + 10, y + 10, Colours::ORANGE);
	}

	y += 80;
	Colour btnC = (focusedField == FIELD_COUNT - 1) ? Colours::ORANGE : Colours::WHITE;
	text.DrawTextCentered(renderer, "[ Save ]", centerX, y, btnC);

	// Draw dropdown overlays on top
	fontColourDropdown.DrawOverlay(r, text);
	displayDropdown.DrawOverlay(r, text);
	slaveModeDropdown.DrawOverlay(r, text);
	captureDropdown.DrawOverlay(r, text);

}

} // namespace SatyrAV
