#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace SatyrAV{

enum class InputAction{
	None,
	NavUp,
	NavDown,
	NavLeft,
	NavRight,
	Execute,
	JumpUp,
	JumpDown,
	JumpTop,
	JumpBottom,
	ToggleVideo,
	ToggleMusic,
	Help,
	Debug,
	Quit,
	Tab,
	PrevCommand,
	ReloadScenes,
	TextInput,
	Backspace
};

class InputHandler{
public:
	InputAction Poll(SDL_Event& event);

	// Text input mode — screens enable this when a text field is focused
	bool IsTextInputMode() const;
	void SetTextInputMode(bool enabled);

	// The last character(s) entered via SDL_TEXTINPUT (valid after TextInput action)
	const std::string& GetLastTextInput() const;

private:
	bool textInputMode = false;
	std::string lastTextInput;

	InputAction TranslateKey(SDL_Keycode key, uint16_t mod);
};

} // namespace SatyrAV
