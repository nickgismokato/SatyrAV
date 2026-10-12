#include "input/InputHandler.hpp"
#include "input/KeyBindings.hpp"

namespace SatyrAV{

InputAction InputHandler::Poll(SDL_Event& event){
	lastTextInput.clear();
	lastFunctionKey = 0;

	// TAB and ESC always work, even in text input mode
	if(event.type == SDL_EVENT_KEY_DOWN){
		if(event.key.key == Keys::TAB) return InputAction::Tab;
		if(event.key.key == Keys::QUIT) return InputAction::Quit;
		// (1.6.3) F1..F12 — encode the number on the side channel and
		// surface a single InputAction::FunctionKey to the screen.
		// Suppressed in text-input mode so a function-key press inside
		// a text field doesn't accidentally fire a hotkey while the
		// user is typing.
		if(!textInputMode){
			SDL_Keycode k = event.key.key;
			if(k >= SDLK_F1 && k <= SDLK_F12){
				lastFunctionKey = (int)(k - SDLK_F1) + 1;
				return InputAction::FunctionKey;
			}
		}
	}

	// Text input events — only when text mode is active
	if(textInputMode){
		if(event.type == SDL_EVENT_TEXT_INPUT){
			lastTextInput = event.text.text;
			return InputAction::TextInput;
		}
		if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_BACKSPACE){
			return InputAction::Backspace;
		}
	}

	if(event.type == SDL_EVENT_KEY_DOWN){
		return TranslateKey(event.key.key, event.key.mod);
	}

	return InputAction::None;
}

InputAction InputHandler::TranslateKey(SDL_Keycode key, uint16_t mod){
	bool shift = (mod & SDL_KMOD_SHIFT) != 0;
	bool ctrl  = (mod & SDL_KMOD_CTRL) != 0;

	if(key == Keys::NAV_UP){
		if(ctrl)  return InputAction::JumpTop;
		if(shift) return InputAction::JumpUp;
		return InputAction::NavUp;
	}
	if(key == Keys::NAV_DOWN){
		if(ctrl)  return InputAction::JumpBottom;
		if(shift) return InputAction::JumpDown;
		return InputAction::NavDown;
	}
	if(key == Keys::NAV_LEFT)   return InputAction::NavLeft;
	if(key == Keys::NAV_RIGHT)  return InputAction::NavRight;
	if(key == Keys::EXECUTE){
		if(shift) return InputAction::StepBack;
		return InputAction::Execute;
	}
	if(key == Keys::STOP_VIDEO) return InputAction::ToggleVideo;
	if(key == Keys::STOP_MUSIC) return InputAction::ToggleMusic;
	if(key == Keys::HELP)       return InputAction::Help;
	if(key == Keys::DEBUG)      return InputAction::Debug;
	if(key == Keys::RELOAD_SCENES) return InputAction::ReloadScenes;
	if(key == Keys::QUIT)       return InputAction::Quit;

	return InputAction::None;
}

bool InputHandler::IsTextInputMode() const{ return textInputMode; }

void InputHandler::SetTextInputWindow(SDL_Window* w){ textInputWindow = w; }

void InputHandler::SetTextInputMode(bool enabled){
	// (2.0.0) SDL3's start/stop take the target window. If the window was
	// never registered there is nothing to type into, so keep the flag in
	// sync but make no SDL call.
	if(textInputWindow){
		if(enabled && !textInputMode){
			SDL_StartTextInput(textInputWindow);
		} else if(!enabled && textInputMode){
			SDL_StopTextInput(textInputWindow);
		}
	}
	textInputMode = enabled;
}

const std::string& InputHandler::GetLastTextInput() const{
	return lastTextInput;
}

int InputHandler::GetLastFunctionKey() const{
	return lastFunctionKey;
}

} // namespace SatyrAV
