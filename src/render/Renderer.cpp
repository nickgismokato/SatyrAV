#include "render/Renderer.hpp"
#include <cstdio>

namespace SatyrAV{

bool Renderer::InitMaster(int mW, int mH){
	// Prefer the D3D11 renderer so NV12 video textures take the GPU-shader fast path.
	// Falls through to the next best driver if D3D11 is unavailable (old Windows, Linux).
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	masterWindow = SDL_CreateWindow(
		"SatyrAV",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		mW, mH,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if(!masterWindow){
		fprintf(stderr, "Failed to create master window: %s\n", SDL_GetError());
		return false;
	}

	masterRenderer = SDL_CreateRenderer(masterWindow, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!masterRenderer){
		fprintf(stderr, "Failed to create master renderer: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

bool Renderer::InitSlave(int slaveDisplayIndex, bool windowed){
	// Close existing slave if any
	CloseSlave();

	if(windowed){
		// (1.5) Dev mode: resizable decorated window on the primary display.
		// Default size matches the typical master window; the user can drag
		// it to any size they like and the content layers follow (see
		// GetSlaveWidth/Height live-query below).
		slaveW = 1280;
		slaveH = 720;
		slaveWindow = SDL_CreateWindow(
			"SatyrAV Output (Windowed)",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			slaveW, slaveH,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
		slaveRenderer = SDL_CreateRenderer(slaveWindow, -1,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

		// Remember the intended production slave dims so
		// CompositeTargetedDisplay can letterbox correctly. Prefer the
		// configured slave display's bounds; if that index is out of range
		// (true single-monitor box), fall back to the primary display.
		windowedMode = true;
		slaveDisplay = -1;
		int idx = slaveDisplayIndex;
		int numDisp = SDL_GetNumVideoDisplays();
		if(idx < 0 || idx >= numDisp) idx = 0;
		SDL_Rect refBounds;
		if(SDL_GetDisplayBounds(idx, &refBounds) == 0 &&
		   refBounds.w > 0 && refBounds.h > 0){
			intendedSlaveW = refBounds.w;
			intendedSlaveH = refBounds.h;
		} else{
			intendedSlaveW = 1920;
			intendedSlaveH = 1080;
		}
	} else{
		int numDisplays = SDL_GetNumVideoDisplays();
		if(slaveDisplayIndex < numDisplays){
			SDL_Rect bounds;
			SDL_GetDisplayBounds(slaveDisplayIndex, &bounds);
			slaveW = bounds.w;
			slaveH = bounds.h;

			slaveWindow = SDL_CreateWindow(
				"SatyrAV Output",
				bounds.x, bounds.y, slaveW, slaveH,
				SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);
			slaveRenderer = SDL_CreateRenderer(slaveWindow, -1,
				SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
			slaveDisplay = slaveDisplayIndex;
		} else{
			fprintf(stderr, "Slave display %d not found (%d displays available)\n",
				slaveDisplayIndex, numDisplays);
			// Fallback window for testing with single monitor
			slaveW = 800;
			slaveH = 600;
			slaveWindow = SDL_CreateWindow(
				"SatyrAV Output (Fallback)",
				SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				slaveW, slaveH, SDL_WINDOW_SHOWN);
			slaveRenderer = SDL_CreateRenderer(slaveWindow, -1,
				SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		}
	}

	if(!slaveRenderer){
		fprintf(stderr, "Failed to create slave renderer: %s\n", SDL_GetError());
		return false;
	}

	// Immediately show black
	ClearSlave(Colours::BLACK);
	PresentSlave();
	return true;
}

void Renderer::CloseSlave(){
	if(slaveTarget){ SDL_DestroyTexture(slaveTarget); slaveTarget = nullptr; }
	targetW = targetH = 0;
	targetActive = false;
	if(slaveRenderer){ SDL_DestroyRenderer(slaveRenderer); slaveRenderer = nullptr; }
	if(slaveWindow){ SDL_DestroyWindow(slaveWindow); slaveWindow = nullptr; }
	slaveW = slaveH = 0;
	windowedMode = false;
	intendedSlaveW = intendedSlaveH = 0;
	slaveDisplay = -1;
}

void Renderer::Shutdown(){
	CloseCapture();
	CloseSlave();
	if(masterRenderer) SDL_DestroyRenderer(masterRenderer);
	if(masterWindow)   SDL_DestroyWindow(masterWindow);
	masterRenderer = nullptr;
	masterWindow = nullptr;
}

SDL_Renderer* Renderer::GetMaster() const{ return masterRenderer; }
SDL_Renderer* Renderer::GetSlave() const{ return slaveRenderer; }
SDL_Window* Renderer::GetMasterWindow() const{ return masterWindow; }
SDL_Window* Renderer::GetSlaveWindow() const{ return slaveWindow; }

void Renderer::ClearMaster(Colour c){
	SetDrawColour(masterRenderer, c);
	SDL_RenderClear(masterRenderer);
}

void Renderer::ClearSlave(Colour c){
	if(!slaveRenderer) return;
	SetDrawColour(slaveRenderer, c);
	SDL_RenderClear(slaveRenderer);
}

void Renderer::PresentMaster(){ SDL_RenderPresent(masterRenderer); }

void Renderer::PresentSlave(){
	if(slaveRenderer) SDL_RenderPresent(slaveRenderer);
}

void Renderer::SetDrawColour(SDL_Renderer* r, Colour c){
	SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void Renderer::DrawRect(SDL_Renderer* r, int x, int y, int w, int h, Colour c, bool filled){
	SetDrawColour(r, c);
	SDL_Rect rect = {x, y, w, h};
	if(filled) SDL_RenderFillRect(r, &rect);
	else       SDL_RenderDrawRect(r, &rect);
}

int Renderer::GetMasterWidth() const{
	int w, h;
	SDL_GetWindowSize(masterWindow, &w, &h);
	return w;
}

int Renderer::GetMasterHeight() const{
	int w, h;
	SDL_GetWindowSize(masterWindow, &w, &h);
	return h;
}

// (1.5) Live-query the current slave drawable size. Required by windowed
// mode so a user-resized window's dimensions propagate into the letterbox
// composite without needing to route SDL_WINDOWEVENT_SIZE_CHANGED events.
// Fullscreen-desktop slaves never resize, so this is a pure getter for them.
int Renderer::GetSlaveWidth() const{
	if(slaveRenderer){
		int w = 0, h = 0;
		SDL_GetRendererOutputSize(slaveRenderer, &w, &h);
		if(w > 0) return w;
	}
	return slaveW;
}
int Renderer::GetSlaveHeight() const{
	if(slaveRenderer){
		int w = 0, h = 0;
		SDL_GetRendererOutputSize(slaveRenderer, &w, &h);
		if(h > 0) return h;
	}
	return slaveH;
}

int Renderer::GetTargetWidth() const{
	return (targetActive && targetW > 0) ? targetW : slaveW;
}

int Renderer::GetTargetHeight() const{
	return (targetActive && targetH > 0) ? targetH : slaveH;
}

float Renderer::GetSlaveContentScale() const{
	// Reference height: intendedSlaveH in windowed-dev mode (production
	// slave bounds the scene was authored for), else the real slave
	// window's current height.
	int refH = windowedMode
		? (intendedSlaveH > 0 ? intendedSlaveH : 0)
		: slaveH;
	int curH = targetH > 0 ? targetH : slaveH;
	if(refH <= 0 || curH <= 0) return 1.0f;
	return (float)curH / (float)refH;
}

bool Renderer::EnsureSlaveTarget(int w, int h){
	if(!slaveRenderer) return false;
	if(w <= 0 || h <= 0) return false;
	if(slaveTarget && targetW == w && targetH == h) return true;

	if(slaveTarget){
		SDL_DestroyTexture(slaveTarget);
		slaveTarget = nullptr;
	}
	// ARGB8888 is a safe, widely-supported target-capable format.
	slaveTarget = SDL_CreateTexture(slaveRenderer,
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
	if(!slaveTarget){
		fprintf(stderr, "Failed to create slave target %dx%d: %s\n",
			w, h, SDL_GetError());
		targetW = targetH = 0;
		return false;
	}
	// Preserve any alpha the content layers paint with.
	SDL_SetTextureBlendMode(slaveTarget, SDL_BLENDMODE_BLEND);
	targetW = w;
	targetH = h;
	return true;
}

void Renderer::BeginSlaveContent(){
	if(!slaveRenderer || !slaveTarget) return;
	SDL_SetRenderTarget(slaveRenderer, slaveTarget);
	targetActive = true;
}

void Renderer::EndSlaveContent(){
	if(!slaveRenderer) return;
	SDL_SetRenderTarget(slaveRenderer, nullptr);
	targetActive = false;
}

void Renderer::CompositeTargetedDisplay(const TargetedDisplay& t){
	if(!slaveRenderer || !slaveTarget) return;

	// Paint the physical slave black first — the area outside the
	// targeted rect must always be solid black on the projector.
	SetDrawColour(slaveRenderer, Colours::BLACK);
	SDL_RenderClear(slaveRenderer);

	// (1.5) Live-query dims so windowed-slave resizes compose correctly
	// without needing an SDL_WINDOWEVENT_SIZE_CHANGED pipeline.
	int liveW = GetSlaveWidth();
	int liveH = GetSlaveHeight();

	int dstW, dstH, cx, cy;
	double rot = (double)t.rotation;

	if(windowedMode && intendedSlaveW > 0 && intendedSlaveH > 0){
		// Rect coords were authored in production-slave pixel space.
		// Letterbox that virtual slave inside the window so the preview
		// matches production (aspect-correct, centered, black bars).
		float sx = (float)liveW / (float)intendedSlaveW;
		float sy = (float)liveH / (float)intendedSlaveH;
		float s  = sx < sy ? sx : sy;

		int boxW = (int)(intendedSlaveW * s);
		int boxH = (int)(intendedSlaveH * s);
		int ox   = (liveW - boxW) / 2;
		int oy   = (liveH - boxH) / 2;

		if(t.width > 0 && t.height > 0){
			dstW = (int)(t.width  * s);
			dstH = (int)(t.height * s);
			cx   = ox + (int)(t.centerX * s);
			cy   = oy + (int)(t.centerY * s);
		} else{
			// No rect authored — fill the whole virtual slave.
			dstW = boxW;
			dstH = boxH;
			cx   = ox + boxW / 2;
			cy   = oy + boxH / 2;
		}
	} else{
		dstW = t.width  > 0 ? t.width  : liveW;
		dstH = t.height > 0 ? t.height : liveH;
		cx   = t.width  > 0 ? t.centerX : liveW / 2;
		cy   = t.height > 0 ? t.centerY : liveH / 2;
	}

	SDL_Rect dst = {cx - dstW / 2, cy - dstH / 2, dstW, dstH};
	if(rot != 0.0){
		SDL_RenderCopyEx(slaveRenderer, slaveTarget, nullptr, &dst,
			rot, nullptr, SDL_FLIP_NONE);
	} else{
		SDL_RenderCopy(slaveRenderer, slaveTarget, nullptr, &dst);
	}
}

// ───────────────────────── Capture mirror ──────────────────────────
//
// Third OS window on a separate display. Receives the slave's pre-
// targeted composite — i.e. the contents of slaveTarget — untransformed,
// at slave-logical resolution. Intended to feed a capture card so the
// recording is clean of the letterbox/rotation that CompositeTargetedDisplay
// applies to the physical slave.

bool Renderer::InitCapture(int displayIndex, int logicalW, int logicalH){
	// Soft guards — Options UI also enforces these, but validate again in
	// case config was hand-edited or the display layout changed.
	if(windowedMode){
		fprintf(stderr,
			"Capture: suppressed — slave is running in windowed dev mode.\n");
		return false;
	}
	if(slaveDisplay >= 0 && displayIndex == slaveDisplay){
		fprintf(stderr,
			"Capture: refused — target display %d is the slave's display.\n",
			displayIndex);
		return false;
	}
	int numDisp = SDL_GetNumVideoDisplays();
	if(displayIndex < 0 || displayIndex >= numDisp){
		fprintf(stderr,
			"Capture: display %d out of range (have %d).\n",
			displayIndex, numDisp);
		return false;
	}
	if(logicalW <= 0 || logicalH <= 0){
		fprintf(stderr, "Capture: invalid logical size %dx%d.\n",
			logicalW, logicalH);
		return false;
	}

	CloseCapture();

	SDL_Rect bounds;
	if(SDL_GetDisplayBounds(displayIndex, &bounds) != 0){
		fprintf(stderr, "Capture: SDL_GetDisplayBounds failed: %s\n",
			SDL_GetError());
		return false;
	}

	captureWindow = SDL_CreateWindow(
		"SatyrAV Capture",
		bounds.x, bounds.y, bounds.w, bounds.h,
		SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);
	if(!captureWindow){
		fprintf(stderr, "Capture: failed to create window: %s\n",
			SDL_GetError());
		return false;
	}

	captureRenderer = SDL_CreateRenderer(captureWindow, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!captureRenderer){
		fprintf(stderr, "Capture: failed to create renderer: %s\n",
			SDL_GetError());
		SDL_DestroyWindow(captureWindow);
		captureWindow = nullptr;
		return false;
	}

	// Content is uploaded at logical size; the OS scales to the physical
	// display. Upscale is linear to avoid blocky capture on 4K monitors.
	SDL_RenderSetLogicalSize(captureRenderer, logicalW, logicalH);

	captureLogicalW   = logicalW;
	captureLogicalH   = logicalH;
	captureDisplayIdx = displayIndex;
	captureTextureReady = false;

	// Streaming texture matches the readback format. Size tracks the
	// slave's offscreen target; UpdateCaptureFromSlaveTarget recreates
	// this texture on dim changes (e.g. TargetedDisplay rect resized).
	captureTexture = SDL_CreateTexture(captureRenderer,
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		logicalW, logicalH);
	if(!captureTexture){
		fprintf(stderr, "Capture: failed to create streaming texture: %s\n",
			SDL_GetError());
	}
	capturePixels.assign((size_t)logicalW * (size_t)logicalH * 4, 0);

	// Show black immediately so the window isn't empty between open and
	// the first readback.
	SetDrawColour(captureRenderer, Colours::BLACK);
	SDL_RenderClear(captureRenderer);
	SDL_RenderPresent(captureRenderer);
	return true;
}

void Renderer::CloseCapture(){
	if(captureTexture){
		SDL_DestroyTexture(captureTexture);
		captureTexture = nullptr;
	}
	if(captureRenderer){
		SDL_DestroyRenderer(captureRenderer);
		captureRenderer = nullptr;
	}
	if(captureWindow){
		SDL_DestroyWindow(captureWindow);
		captureWindow = nullptr;
	}
	captureLogicalW = captureLogicalH = 0;
	captureDisplayIdx = -1;
}

bool Renderer::HasCapture() const{
	return captureRenderer != nullptr;
}

void Renderer::UpdateCaptureFromSlaveTarget(){
	if(!captureRenderer || !slaveRenderer || !slaveTarget) return;
	if(targetW <= 0 || targetH <= 0) return;

	// Resize capture texture + CPU buffer + logical size when the slave's
	// offscreen target dims change (e.g. user resized TargetedDisplay via
	// debug-S menu). Keeps output "at native slave-logical resolution".
	if(targetW != captureLogicalW || targetH != captureLogicalH ||
	   !captureTexture){
		if(captureTexture){
			SDL_DestroyTexture(captureTexture);
			captureTexture = nullptr;
		}
		captureTexture = SDL_CreateTexture(captureRenderer,
			SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
			targetW, targetH);
		if(!captureTexture){
			fprintf(stderr,
				"Capture: failed to recreate streaming texture %dx%d: %s\n",
				targetW, targetH, SDL_GetError());
			return;
		}
		SDL_RenderSetLogicalSize(captureRenderer, targetW, targetH);
		capturePixels.assign((size_t)targetW * (size_t)targetH * 4, 0);
		captureLogicalW = targetW;
		captureLogicalH = targetH;
		captureTextureReady = false;
	}

	// GPU→CPU readback. ReadPixels operates on the *current* render
	// target; slaveTarget is no longer active after EndSlaveContent, so
	// point the slave renderer back at it briefly and then restore.
	const int pitch = targetW * 4;
	SDL_SetRenderTarget(slaveRenderer, slaveTarget);
	int readErr = SDL_RenderReadPixels(slaveRenderer, nullptr,
		SDL_PIXELFORMAT_ARGB8888, capturePixels.data(), pitch);
	SDL_SetRenderTarget(slaveRenderer, nullptr);
	if(readErr != 0){
		fprintf(stderr, "Capture: RenderReadPixels failed: %s\n",
			SDL_GetError());
		return;
	}

	// CPU→GPU upload onto the capture renderer's streaming texture.
	if(SDL_UpdateTexture(captureTexture, nullptr,
		capturePixels.data(), pitch) == 0){
		captureTextureReady = true;
	}
}

void Renderer::PresentCapture(){
	if(!captureRenderer) return;

	SetDrawColour(captureRenderer, Colours::BLACK);
	SDL_RenderClear(captureRenderer);
	if(captureTexture && captureTextureReady){
		// Logical size matches the texture, so a null-dst copy fills the
		// logical frame. SDL handles scaling to the physical window.
		SDL_RenderCopy(captureRenderer, captureTexture, nullptr, nullptr);
	}
	SDL_RenderPresent(captureRenderer);
}

int Renderer::GetDisplayCount() const{
	return SDL_GetNumVideoDisplays();
}

std::string Renderer::GetDisplayName(int index) const{
	const char* name = SDL_GetDisplayName(index);
	if(name) return std::string(name);
	return "Display " + std::to_string(index);
}

} // namespace SatyrAV
