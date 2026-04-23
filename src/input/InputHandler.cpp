#include "input/InputHandler.hpp"
#include "input/KeyBindings.hpp"

namespace SatyrAV{

InputAction InputHandler::Poll(SDL_Event& event){
	lastTextInput.clear();

	// TAB and ESC always work, even in text input mode
	if(event.type == SDL_KEYDOWN){
		if(event.key.keysym.sym == Keys::TAB) return InputAction::Tab;
		if(event.key.keysym.sym == Keys::QUIT) return InputAction::Quit;
	}

	// Text input events — only when text mode is active
	if(textInputMode){
		if(event.type == SDL_TEXTINPUT){
			lastTextInput = event.text.text;
			return InputAction::TextInput;
		}
		if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE){
			return InputAction::Backspace;
		}
	}

	if(event.type == SDL_KEYDOWN){
		return TranslateKey(event.key.keysym.sym, event.key.keysym.mod);
	}

	return InputAction::None;
}

InputAction InputHandler::TranslateKey(SDL_Keycode key, uint16_t mod){
	bool shift = (mod & KMOD_SHIFT) != 0;
	bool ctrl  = (mod & KMOD_CTRL) != 0;

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
		if(shift) return InputAction::PrevCommand;
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

void InputHandler::SetTextInputMode(bool enabled){
	if(enabled && !textInputMode){
		SDL_StartTextInput();
	} else if(!enabled && textInputMode){
		SDL_StopTextInput();
	}
	textInputMode = enabled;
}

const std::string& InputHandler::GetLastTextInput() const{
	return lastTextInput;
}

} // namespace SatyrAV
