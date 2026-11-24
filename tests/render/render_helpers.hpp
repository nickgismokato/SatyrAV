#pragma once
// Shared scaffolding for the render tests.

#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include "core/AssetPath.hpp"
#include <doctest/doctest.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

namespace SatyrRender{

using namespace SatyrAV;

struct Pixel{
	uint8_t r = 0, g = 0, b = 0, a = 0;
	int Luma() const{ return (r * 299 + g * 587 + b * 114) / 1000; }
};

// SDL with video (off-screen) and audio (dummy), a master window and a
// windowed slave, and the text renderer on the bundled DejaVu fonts.
// Everything is torn down again at the end of the test.
class OffscreenRig{
public:
	Renderer r;
	TextRenderer text;
	bool ok = false;

	OffscreenRig(){
		if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return;
		if(!r.InitMaster(1280, 720)) return;
		if(!r.InitSlave(0, true, false)) return;
		ok = text.Init(FindAsset("fonts/DejaVuSans.ttf"), 25,
			FindAsset("fonts/DejaVuSans-Oblique.ttf"),
			FindAsset("fonts/DejaVuSans-Bold.ttf"),
			FindAsset("fonts/DejaVuSans-BoldOblique.ttf"));
		text.SetSlaveFontPaths("", "", "", "");
		text.SetSlaveFontSize(60);
	}
	~OffscreenRig(){
		text.Shutdown();
		r.Shutdown();
		SDL_Quit();
	}

	int W() const{ return r.GetCanvasWidth(); }
	int H() const{ return r.GetCanvasHeight(); }

	// Draws into the slave canvas with `draw`, then reads the canvas back
	// (before the targeted-display composite: what the capture would see).
	SDL_Surface* RenderCanvas(const std::function<void()>& draw){
		if(!r.EnsureSlaveTarget(W(), H())) return nullptr;
		r.BeginSlaveContent();
		draw();
		SDL_Surface* s = SDL_RenderReadPixels(r.GetSlave(), nullptr);
		r.EndSlaveContent();
		return s;
	}
};

inline Pixel At(SDL_Surface* s, int x, int y){
	Pixel p;
	if(s) SDL_ReadSurfacePixel(s, x, y, &p.r, &p.g, &p.b, &p.a);
	return p;
}

// Pixels in the rectangle for which `pred` holds.
inline int Count(SDL_Surface* s, int x0, int y0, int x1, int y1,
	const std::function<bool(const Pixel&)>& pred){
	int n = 0;
	for(int y = y0; y < y1; y += 2)
		for(int x = x0; x < x1; x += 2)
			if(pred(At(s, x, y))) n++;
	return n;
}

// A solid-colour texture on the slave renderer.
inline SDL_Texture* SolidTexture(SDL_Renderer* rr, int w, int h, uint8_t r, uint8_t g, uint8_t b){
	SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
	if(!s) return nullptr;
	SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, r, g, b, 255));
	SDL_Texture* t = SDL_CreateTextureFromSurface(rr, s);
	SDL_DestroySurface(s);
	return t;
}

inline void SleepMs(int ms){
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Calls `step` every 10 ms until `done` holds or `ms` have passed.
inline bool WaitFor(int ms, const std::function<void()>& step, const std::function<bool()>& done){
	const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
	while(std::chrono::steady_clock::now() < end){
		step();
		if(done()) return true;
		SleepMs(10);
	}
	step();
	return done();
}

} // namespace SatyrRender
