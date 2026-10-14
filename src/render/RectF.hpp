#pragma once
#include <SDL3/SDL.h>

namespace SatyrAV{

// (2.0.0) SDL3's 2D render API is float-based: SDL_RenderTexture and
// friends take SDL_FRect, where SDL2 took SDL_Rect. Integer SDL_Rect is
// still used for display bounds and texture-update regions, so the two
// coexist and the conversion needs to be visible rather than implicit —
// brace-initialising an SDL_FRect from ints is a narrowing conversion and
// GCC rejects it under -Wnarrowing.
//
// Layout maths throughout SatyrAV is done in integer pixels, so this keeps
// that unchanged and converts once, at the point of handing the rect to
// the renderer.
inline SDL_FRect FRectI(int x, int y, int w, int h){
	return SDL_FRect{(float)x, (float)y, (float)w, (float)h};
}

} // namespace SatyrAV
