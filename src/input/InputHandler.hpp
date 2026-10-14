#pragma once
#include <SDL3/SDL.h>
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
	StepBack,
	ReloadScenes,
	MarkLine,       // (2.0.1) O: log the last executed cue to fix-it/
	TextInput,
	Backspace,
	// (1.6.3) F1..F12 collapsed into one action with an integer payload
	// on InputHandler. Project-level `[Hotkeys]` table maps the number
	// to an audio file. Saves enumerating twelve actions and twelve
	// switch cases at every dispatch site.
	FunctionKey
};

class InputHandler{
public:
	InputAction Poll(SDL_Event& event);

	// (2.0.0) SDL3 scopes text input to a specific window rather than
	// globally. SatyrAV only ever accepts typing on the master window, so
	// it is registered once at startup and SetTextInputMode keeps its
	// existing bool-only signature — leaving all eight screen call sites
	// unchanged.
	void SetTextInputWindow(SDL_Window* w);

	// Text input mode — screens enable this when a text field is focused
	bool IsTextInputMode() const;
	void SetTextInputMode(bool enabled);

	// The last character(s) entered via SDL_EVENT_TEXT_INPUT (valid after TextInput action)
	const std::string& GetLastTextInput() const;

	// (1.6.3) Last F-number pressed; valid only on the same Poll() that
	// returned InputAction::FunctionKey. Range 1..12.
	int GetLastFunctionKey() const;

private:
	SDL_Window* textInputWindow = nullptr;
	bool textInputMode = false;
	std::string lastTextInput;
	int  lastFunctionKey = 0;

	InputAction TranslateKey(SDL_Keycode key, uint16_t mod);
};

} // namespace SatyrAV
