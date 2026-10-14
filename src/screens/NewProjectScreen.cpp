#include "screens/NewProjectScreen.hpp"
#include "App.hpp"
#include "core/Project.hpp"
#include "core/Config.hpp"

namespace SatyrAV{

void NewProjectScreen::OnEnter(){
	focusedIndex = 0;
	isJubi = false;

	// (1.6.4) Drop any expanded state left over from a previous visit so a
	// dropdown doesn't reappear already-open after ESC + return.
	revyTypeDropdown.Close();

	revyTypeDropdown.label = "Revy";
	revyTypeDropdown.SetOptions({
		"MatRevy", "BioRevy", "KemiRevy", "MBKRevy",
		"PsykoRevy", "FysikRevy", "GeoRevy", "DIKURevy",
		"SaTyR", "Other"
	});

	customNameField.label = "Custom Name";
	customNameField.maxLength = 32;
	customNameField.Clear();

	yearField.label = "Year";
	yearField.maxLength = 4;
	yearField.Clear();

	creatorField.label = "Creator";
	creatorField.maxLength = MAX_CREATOR_NAME_LENGTH;
	creatorField.Clear();

	UpdateFocus();
}

void NewProjectScreen::OnExit(){
	app->GetInput().SetTextInputMode(false);
}

bool NewProjectScreen::IsOtherSelected() const{
	// "Other" is always the last option in the dropdown list.
	return revyTypeDropdown.GetSelectedOption() == "Other";
}

std::vector<NPField> NewProjectScreen::ActiveFields() const{
	std::vector<NPField> f;
	f.push_back(NPField::RevyType);
	f.push_back(NPField::Jubi);
	if(IsOtherSelected()) f.push_back(NPField::CustomName);
	f.push_back(NPField::Year);
	f.push_back(NPField::Creator);
	f.push_back(NPField::Create);
	return f;
}

NPField NewProjectScreen::FocusedField() const{
	auto f = ActiveFields();
	int idx = focusedIndex;
	if(idx < 0) idx = 0;
	if(idx >= (int)f.size()) idx = (int)f.size() - 1;
	return f[idx];
}

std::string NewProjectScreen::ComputeRevyName() const{
	std::string base;
	if(IsOtherSelected()){
		base = customNameField.GetText();
	} else{
		base = revyTypeDropdown.GetSelectedOption();
	}
	if(isJubi) base += "Jubi";
	return base;
}

void NewProjectScreen::HandleInput(InputAction action){
	auto& input = app->GetInput();

	// ESC always goes back
	if(action == InputAction::Quit){
		app->SwitchScreen(ScreenID::MainMenu);
		return;
	}

	NPField focused = FocusedField();

	// Text input goes to the currently-focused text field.
	if(action == InputAction::TextInput){
		if(focused == NPField::Year){
			yearField.AppendText(input.GetLastTextInput());
		} else if(focused == NPField::Creator){
			creatorField.AppendText(input.GetLastTextInput());
		} else if(focused == NPField::CustomName){
			customNameField.AppendText(input.GetLastTextInput());
		}
		return;
	}

	if(action == InputAction::Backspace){
		if(focused == NPField::Year){
			yearField.RemoveLastChar();
		} else if(focused == NPField::Creator){
			creatorField.RemoveLastChar();
		} else if(focused == NPField::CustomName){
			customNameField.RemoveLastChar();
		}
		return;
	}

	// Dropdown expanded → route arrows/enter to it
	if(focused == NPField::RevyType && revyTypeDropdown.IsExpanded()){
		switch(action){
			case InputAction::NavUp:   revyTypeDropdown.SelectPrev(); return;
			case InputAction::NavDown: revyTypeDropdown.SelectNext(); return;
			case InputAction::Execute:
				revyTypeDropdown.Close();
				// Toggling "Other" changes the active field list; re-clamp
				// focus so we stay on the dropdown.
				UpdateFocus();
				return;
			default: break;
		}
	}

	// Enter on dropdown → toggle it
	if(focused == NPField::RevyType && action == InputAction::Execute){
		revyTypeDropdown.Toggle();
		return;
	}

	// Enter on Jubi → flip the toggle
	if(focused == NPField::Jubi && action == InputAction::Execute){
		isJubi = !isJubi;
		return;
	}

	// Enter on create button → create
	if(focused == NPField::Create && action == InputAction::Execute){
		CreateProject();
		return;
	}

	// Up/Down/Tab changes focused field (using the active-field list so the
	// Custom Name row is skipped automatically when not Other).
	auto fields = ActiveFields();
	int count = (int)fields.size();
	if(action == InputAction::NavUp){
		focusedIndex = (focusedIndex - 1 + count) % count;
		revyTypeDropdown.Close();
		UpdateFocus();
	} else if(action == InputAction::NavDown || action == InputAction::Tab){
		focusedIndex = (focusedIndex + 1) % count;
		revyTypeDropdown.Close();
		UpdateFocus();
	}
}

void NewProjectScreen::UpdateFocus(){
	// Clamp focused index to the active field list length in case we just
	// toggled the RevyType between Other and non-Other.
	auto fields = ActiveFields();
	if(focusedIndex >= (int)fields.size()) focusedIndex = (int)fields.size() - 1;

	NPField focused = FocusedField();
	revyTypeDropdown.focused = (focused == NPField::RevyType);
	customNameField.focused  = (focused == NPField::CustomName);
	yearField.focused        = (focused == NPField::Year);
	creatorField.focused     = (focused == NPField::Creator);

	// Enable text input only when a text field is focused.
	bool needsText = (focused == NPField::Year ||
	                  focused == NPField::Creator ||
	                  focused == NPField::CustomName);
	app->GetInput().SetTextInputMode(needsText);
}

void NewProjectScreen::Update(float deltaTime){
	(void)deltaTime;
}

void NewProjectScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();
	int centerX = r.GetMasterWidth() / 2;

	text.DrawTextCentered(renderer, "New Project",
		centerX, 30, Colours::WHITE);

	text.DrawTextCentered(renderer, "ESC: Back  |  UP/DOWN: Navigate  |  ENTER: Select/Toggle/Create",
		centerX, r.GetMasterHeight() - 30, Colours::WHITE);

	int fieldW = 300;
	int fieldX = centerX - fieldW / 2;
	int y = 80;
	const int rowGap = 70;

	NPField focused = FocusedField();
	auto fields = ActiveFields();

	for(NPField f : fields){
		switch(f){
			case NPField::RevyType:
				revyTypeDropdown.x = fieldX; revyTypeDropdown.y = y;
				revyTypeDropdown.w = fieldW;
				revyTypeDropdown.Draw(r, text);
				break;

			case NPField::Jubi:{
				std::string label = std::string("[") + (isJubi ? "X" : " ") + "] Jubi";
				Colour c = (focused == NPField::Jubi) ? Colours::ORANGE : Colours::WHITE;
				text.DrawTextCentered(renderer, label.c_str(), centerX, y + 15, c);
				break;
			}

			case NPField::CustomName:
				customNameField.x = fieldX; customNameField.y = y;
				customNameField.w = fieldW;
				customNameField.Draw(r, text);
				break;

			case NPField::Year:
				yearField.x = fieldX; yearField.y = y;
				yearField.w = fieldW;
				yearField.Draw(r, text);
				break;

			case NPField::Creator:
				creatorField.x = fieldX; creatorField.y = y;
				creatorField.w = fieldW;
				creatorField.Draw(r, text);
				break;

			case NPField::Create:{
				Colour btnC = (focused == NPField::Create) ? Colours::ORANGE : Colours::WHITE;
				text.DrawTextCentered(renderer, "[ Create ]", centerX, y + 15, btnC);
				break;
			}
		}
		// Jubi toggle is a single-line row — use a shorter gap so the
		// layout stays compact.
		y += (f == NPField::Jubi) ? 40 : rowGap;
	}

	// Preview the resulting folder name so the user sees the Jubi / custom
	// name effect live while editing.
	std::string preview = ComputeRevyName();
	if(preview.empty()) preview = "<name>";
	std::string year = yearField.GetText();
	if(year.empty()) year = "<year>";
	std::string folder = "Folder: " + preview + "_" + year;
	text.DrawTextCentered(renderer, folder.c_str(),
		centerX, r.GetMasterHeight() - 60, Colours::WHITE);

	// Dropdown's expanded list drawn last so it sits on top.
	revyTypeDropdown.DrawOverlay(r, text);
}

void NewProjectScreen::CreateProject(){
	int year = 0;
	try{
		year = std::stoi(yearField.GetText());
	} catch(...){
		return;
	}
	if(year < MIN_PROJECT_YEAR) return;

	if(creatorField.GetText().empty()) return;

	// For "Other" the custom name must be non-empty.
	if(IsOtherSelected() && customNameField.GetText().empty()) return;

	RevyType type = static_cast<RevyType>(revyTypeDropdown.GetSelectedIndex());
	std::string revyName = ComputeRevyName();

	auto& config = Config::Instance();
	std::string basePath = config.projectsDir;
	if(basePath.empty()) basePath = ".";
	std::string projectPath = basePath + "/" + revyName + "_" + std::to_string(year);

	ProjectData data = Project::Create(type, year,
		creatorField.GetText(), projectPath, revyName);
	app->OpenProject(data);
}

} // namespace SatyrAV
