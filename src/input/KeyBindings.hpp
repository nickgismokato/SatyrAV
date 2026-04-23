#pragma once
#include <SDL2/SDL.h>

namespace SatyrAV{

// Centralised key definitions for easy remapping
namespace Keys{
	constexpr SDL_Keycode STOP_VIDEO     = SDLK_k;
	constexpr SDL_Keycode STOP_MUSIC     = SDLK_m;
	constexpr SDL_Keycode HELP           = SDLK_h;
	constexpr SDL_Keycode NAV_RIGHT      = SDLK_RIGHT;
	constexpr SDL_Keycode NAV_LEFT       = SDLK_LEFT;
	constexpr SDL_Keycode NAV_UP         = SDLK_UP;
	constexpr SDL_Keycode NAV_DOWN       = SDLK_DOWN;
	constexpr SDL_Keycode EXECUTE        = SDLK_RETURN;
	constexpr SDL_Keycode QUIT           = SDLK_ESCAPE;
	constexpr SDL_Keycode TAB            = SDLK_TAB;
	constexpr SDL_Keycode DEBUG          = SDLK_d;
	constexpr SDL_Keycode RELOAD_SCENES  = SDLK_u;
}

} // namespace SatyrAV
