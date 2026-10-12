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
}

} // namespace SatyrAV
