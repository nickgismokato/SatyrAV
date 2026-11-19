#pragma once
#include <SDL3/SDL.h>

namespace SatyrAV{

// Centralised key definitions for easy remapping
namespace Keys{
	constexpr SDL_Keycode STOP_VIDEO     = SDLK_K;
	constexpr SDL_Keycode STOP_MUSIC     = SDLK_M;
	constexpr SDL_Keycode HELP           = SDLK_H;
	constexpr SDL_Keycode NAV_RIGHT      = SDLK_RIGHT;
	constexpr SDL_Keycode NAV_LEFT       = SDLK_LEFT;
	constexpr SDL_Keycode NAV_UP         = SDLK_UP;
	constexpr SDL_Keycode NAV_DOWN       = SDLK_DOWN;
	constexpr SDL_Keycode EXECUTE        = SDLK_RETURN;
	constexpr SDL_Keycode QUIT           = SDLK_ESCAPE;
	constexpr SDL_Keycode TAB            = SDLK_TAB;
	constexpr SDL_Keycode DEBUG          = SDLK_D;
	constexpr SDL_Keycode RELOAD_SCENES  = SDLK_U;
	// (2.0.1) Mark the last executed cue in the project's fix-it log.
	// D then O (debug popup open) is still the Overview popup; App
	// handles that chord before the key reaches a screen.
	constexpr SDL_Keycode MARK_LINE      = SDLK_O;
	// (2.1.0) Wipe the slave, like a `clearAll` that is always available.
	// Only outside text-input mode: while typing, BACKSPACE deletes a
	// character (InputHandler checks text mode first).
	constexpr SDL_Keycode CLEAR_SCREEN   = SDLK_BACKSPACE;
}

} // namespace SatyrAV
