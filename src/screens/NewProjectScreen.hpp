#pragma once
#include "screens/Screen.hpp"
#include "ui/TextField.hpp"
#include "ui/Dropdown.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

// Field ordering in the New Project screen. The custom-name field is only
// reachable when the revy type is "Other"; the helper functions below resolve
// indices to field IDs and vice versa based on the current state.
enum class NPField{
	RevyType,    // dropdown
	Jubi,        // toggle — ENTER flips it
	CustomName,  // text field, only when RevyType::Other
	Year,        // text field
	Creator,     // text field
	Create       // button
};

class NewProjectScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	Dropdown revyTypeDropdown;
	TextField customNameField;
	TextField yearField;
	TextField creatorField;
	bool isJubi = false;
	int focusedIndex = 0; // index into the currently-active field list

	void CreateProject();
	void UpdateFocus();

	// Returns true if the selected revy type is "Other" (enables the custom
	// name field in the focus cycle and draw order).
	bool IsOtherSelected() const;

	// Builds the ordered list of active fields given IsOtherSelected().
	std::vector<NPField> ActiveFields() const;

	// Convenience: which NPField has focus right now?
	NPField FocusedField() const;

	// Computed display name (e.g. "MatRevyJubi" or "CosmicRevyJubi").
	std::string ComputeRevyName() const;
};

} // namespace SatyrAV
