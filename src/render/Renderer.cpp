#include "render/Renderer.hpp"
#include "core/Logger.hpp"
#include <cstdio>

namespace SatyrAV{

// (2.0.0) Index ↔ SDL_DisplayID bridge. SDL_GetDisplays returns a
// SDL_free-able array in the platform's display order — the same order SDL2
// exposed as indices — so index N here means the monitor SDL2 called N.
int Renderer::DisplayCount(){
	int n = 0;
	SDL_DisplayID* ids = SDL_GetDisplays(&n);
	if(!ids) return 0;
	SDL_free(ids);
	return n;
}

SDL_DisplayID Renderer::DisplayIdForIndex(int index){
	int n = 0;
	SDL_DisplayID* ids = SDL_GetDisplays(&n);
	if(!ids) return 0;
	SDL_DisplayID id = (index >= 0 && index < n) ? ids[index] : 0;
	SDL_free(ids);
	return id;
}

// (2.0.0) Prefer D3D11, but never *require* it.
//
// SDL2 treated SDL_HINT_RENDER_DRIVER as a preference: if the named driver
// was unavailable, SDL_CreateRenderer quietly fell through to the next best
// one, which is how the same code worked on both Windows and Linux. SDL3
// treats the hint as a requirement — with it set to "direct3d11",
// SDL_CreateRenderer fails outright on a machine that has no D3D11 instead
// of falling back. That took the whole Linux build down at startup with
// "direct3d11 not available".
//
// Asking for the driver by name and retrying with SDL's own choice restores
// the original semantics explicitly, on every platform.
static SDL_Renderer* CreatePreferredRenderer(SDL_Window* w){
	if(!w) return nullptr;
	// (2.0.0) SDL creates its D3D11 device single-threaded by default. The
	// hardware video decoder shares the slave renderer's device from the
	// decode thread, which is undefined behaviour on a single-threaded
	// device — and it fails silently, not loudly. Must be set before the
	// renderer exists; it cannot be changed afterwards.
	SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
	SDL_Renderer* r = SDL_CreateRenderer(w, "direct3d11");
	if(!r) r = SDL_CreateRenderer(w, nullptr);
	return r;
}

bool Renderer::InitMaster(int mW, int mH){
	// D3D11 is preferred so YUV video textures take the GPU-shader fast
	// path. Texture filtering no longer needs a hint: SDL3 defaults every
	// texture to SDL_SCALEMODE_LINEAR, which is what the old
	// SDL_HINT_RENDER_SCALE_QUALITY=linear was buying us.
	masterWindow = SDL_CreateWindow("SatyrAV", mW, mH, SDL_WINDOW_RESIZABLE);
	if(!masterWindow){
		Logger::Instance().Errorf("Failed to create master window: %s", SDL_GetError());
		return false;
	}

	masterRenderer = CreatePreferredRenderer(masterWindow);
	if(!masterRenderer){
		Logger::Instance().Errorf("Failed to create master renderer: %s", SDL_GetError());
		return false;
	}
	// Vsync is no longer a creation flag.
	SDL_SetRenderVSync(masterRenderer, 1);

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
			"SatyrAV Output (Windowed)", slaveW, slaveH, SDL_WINDOW_RESIZABLE);
		slaveRenderer = CreatePreferredRenderer(slaveWindow);

		// Remember the intended production slave dims so
		// CompositeTargetedDisplay can letterbox correctly. Prefer the
		// configured slave display's bounds; if that index is out of range
		// (true single-monitor box), fall back to the primary display.
		windowedMode = true;
		slaveDisplay = -1;
		int idx = slaveDisplayIndex;
		int numDisp = DisplayCount();
		if(idx < 0 || idx >= numDisp) idx = 0;
		SDL_Rect refBounds;
		SDL_DisplayID refId = DisplayIdForIndex(idx);
		// (2.0.0) SDL3 returns true on success, where SDL2 returned 0.
		// The old `== 0` test would have inverted here and silently pinned
		// the windowed preview to the 1920x1080 fallback.
		if(refId && SDL_GetDisplayBounds(refId, &refBounds) &&
		   refBounds.w > 0 && refBounds.h > 0){
			intendedSlaveW = refBounds.w;
			intendedSlaveH = refBounds.h;
		} else{
			intendedSlaveW = 1920;
			intendedSlaveH = 1080;
		}
	} else{
		int numDisplays = DisplayCount();
		SDL_DisplayID slaveId = DisplayIdForIndex(slaveDisplayIndex);
		if(slaveId != 0){
			SDL_Rect bounds;
			SDL_GetDisplayBounds(slaveId, &bounds);
			slaveW = bounds.w;
			slaveH = bounds.h;

			// (2.0.0) SDL_CreateWindow no longer takes a position, and
			// SDL_WINDOW_FULLSCREEN_DESKTOP is gone. Creating the window
			// via properties sets position + borderless + fullscreen in one
			// atomic call, which avoids a create-then-move-then-fullscreen
			// race on backends where window ops are asynchronous.
			SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetStringProperty(props,
				SDL_PROP_WINDOW_CREATE_TITLE_STRING, "SatyrAV Output");
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, bounds.x);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, bounds.y);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, slaveW);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, slaveH);
			SDL_SetBooleanProperty(props,
				SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
			SDL_SetBooleanProperty(props,
				SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
			slaveWindow = SDL_CreateWindowWithProperties(props);
			SDL_DestroyProperties(props);

			// Window geometry is applied asynchronously on some backends;
			// block until it has actually landed so the GetSlaveWidth/Height
			// live-queries below cannot read stale dimensions.
			if(slaveWindow) SDL_SyncWindow(slaveWindow);

			slaveRenderer = CreatePreferredRenderer(slaveWindow);
			slaveDisplay = slaveDisplayIndex;
		} else{
			Logger::Instance().Warnf("Slave display %d not found (%d displays available)",
				slaveDisplayIndex, numDisplays);
			// Fallback window for testing with single monitor
			slaveW = 800;
			slaveH = 600;
			slaveWindow = SDL_CreateWindow(
				"SatyrAV Output (Fallback)", slaveW, slaveH, 0);
			slaveRenderer = CreatePreferredRenderer(slaveWindow);
		}
	}

	if(!slaveRenderer){
		Logger::Instance().Errorf("Failed to create slave renderer: %s", SDL_GetError());
		return false;
	}
	SDL_SetRenderVSync(slaveRenderer, 1);

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
	// (2.0.0) SDL3's 2D render API is float-based; SDL_Rect is only used
	// for integer things like display bounds and texture-update regions.
	SDL_FRect rect = {(float)x, (float)y, (float)w, (float)h};
	if(filled) SDL_RenderFillRect(r, &rect);
	else       SDL_RenderRect(r, &rect);
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
		SDL_GetCurrentRenderOutputSize(slaveRenderer, &w, &h);
		if(w > 0) return w;
	}
	return slaveW;
}
int Renderer::GetSlaveHeight() const{
	if(slaveRenderer){
		int w = 0, h = 0;
		SDL_GetCurrentRenderOutputSize(slaveRenderer, &w, &h);
		if(h > 0) return h;
	}
	return slaveH;
}

int Renderer::GetCanvasWidth() const{
	if(windowedMode && intendedSlaveW > 0) return intendedSlaveW;
	return GetSlaveWidth();
}

int Renderer::GetCanvasHeight() const{
	if(windowedMode && intendedSlaveH > 0) return intendedSlaveH;
	return GetSlaveHeight();
}

int Renderer::GetTargetWidth() const{
	return (targetActive && targetW > 0) ? targetW : slaveW;
}

int Renderer::GetTargetHeight() const{
	return (targetActive && targetH > 0) ? targetH : slaveH;
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
		Logger::Instance().Errorf("Failed to create slave target %dx%d: %s",
			w, h, SDL_GetError());
		targetW = targetH = 0;
		return false;
	}
	// Preserve any alpha the content layers paint with.
	SDL_SetTextureBlendMode(slaveTarget, SDL_BLENDMODE_BLEND);
	// (2.0.0) Replaces the global SDL_HINT_RENDER_SCALE_QUALITY. This
	// texture is scaled into the physical slave by CompositeTargetedDisplay,
	// so without linear filtering the projector output turns blocky.
	SDL_SetTextureScaleMode(slaveTarget, SDL_SCALEMODE_LINEAR);
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

	SDL_FRect dst = {(float)(cx - dstW / 2), (float)(cy - dstH / 2),
	                 (float)dstW, (float)dstH};
	if(rot != 0.0){
		SDL_RenderTextureRotated(slaveRenderer, slaveTarget, nullptr, &dst,
			rot, nullptr, SDL_FLIP_NONE);
	} else{
		SDL_RenderTexture(slaveRenderer, slaveTarget, nullptr, &dst);
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
		Logger::Instance().Warnf(
			"Capture: suppressed — slave is running in windowed dev mode.");
		return false;
	}
	if(slaveDisplay >= 0 && displayIndex == slaveDisplay){
		Logger::Instance().Warnf(
			"Capture: refused — target display %d is the slave's display.",
			displayIndex);
		return false;
	}
	int numDisp = DisplayCount();
	if(displayIndex < 0 || displayIndex >= numDisp){
		Logger::Instance().Warnf(
			"Capture: display %d out of range (have %d).",
			displayIndex, numDisp);
		return false;
	}
	if(logicalW <= 0 || logicalH <= 0){
		Logger::Instance().Warnf("Capture: invalid logical size %dx%d.",
			logicalW, logicalH);
		return false;
	}

	CloseCapture();

	SDL_Rect bounds;
	SDL_DisplayID capId = DisplayIdForIndex(displayIndex);
	// (2.0.0) Was `!= 0` — under SDL3's true-on-success convention that
	// inverted, and the capture window would have refused to open exactly
	// when the bounds query succeeded.
	if(!capId || !SDL_GetDisplayBounds(capId, &bounds)){
		Logger::Instance().Errorf("Capture: SDL_GetDisplayBounds failed: %s",
			SDL_GetError());
		return false;
	}

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props,
		SDL_PROP_WINDOW_CREATE_TITLE_STRING, "SatyrAV Capture");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, bounds.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, bounds.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, bounds.w);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, bounds.h);
	SDL_SetBooleanProperty(props,
		SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
	SDL_SetBooleanProperty(props,
		SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	captureWindow = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
	if(!captureWindow){
		Logger::Instance().Errorf("Capture: failed to create window: %s",
			SDL_GetError());
		return false;
	}
	SDL_SyncWindow(captureWindow);

	captureRenderer = CreatePreferredRenderer(captureWindow);
	if(!captureRenderer){
		Logger::Instance().Errorf("Capture: failed to create renderer: %s",
			SDL_GetError());
		SDL_DestroyWindow(captureWindow);
		captureWindow = nullptr;
		return false;
	}
	SDL_SetRenderVSync(captureRenderer, 1);

	// Content is uploaded at logical size; the OS scales to the physical
	// display. Upscale is linear to avoid blocky capture on 4K monitors.
	// (2.0.0) SDL_RenderSetLogicalSize became SDL_SetRenderLogicalPresentation
	// and now takes an explicit scaling mode. LETTERBOX matches SDL2's
	// default behaviour of preserving aspect ratio with black bars.
	SDL_SetRenderLogicalPresentation(captureRenderer, logicalW, logicalH,
		SDL_LOGICAL_PRESENTATION_LETTERBOX);

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
		Logger::Instance().Errorf("Capture: failed to create streaming texture: %s",
			SDL_GetError());
	} else{
		SDL_SetTextureScaleMode(captureTexture, SDL_SCALEMODE_LINEAR);
	}

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
			Logger::Instance().Errorf(
				"Capture: failed to recreate streaming texture %dx%d: %s",
				targetW, targetH, SDL_GetError());
			return;
		}
		SDL_SetTextureScaleMode(captureTexture, SDL_SCALEMODE_LINEAR);
		SDL_SetRenderLogicalPresentation(captureRenderer, targetW, targetH,
			SDL_LOGICAL_PRESENTATION_LETTERBOX);
		captureLogicalW = targetW;
		captureLogicalH = targetH;
		captureTextureReady = false;
	}

	// GPU→CPU readback. ReadPixels operates on the *current* render
	// target; slaveTarget is no longer active after EndSlaveContent, so
	// point the slave renderer back at it briefly and then restore.
	//
	// (2.0.0) SDL3's SDL_RenderReadPixels allocates and returns a surface
	// instead of filling a caller-supplied buffer, so the CPU staging
	// vector this used to need is gone.
	SDL_SetRenderTarget(slaveRenderer, slaveTarget);
	SDL_Surface* shot = SDL_RenderReadPixels(slaveRenderer, nullptr);
	SDL_SetRenderTarget(slaveRenderer, nullptr);
	if(!shot){
		Logger::Instance().Errorf("Capture: RenderReadPixels failed: %s",
			SDL_GetError());
		return;
	}

	// The surface comes back in whatever format the renderer read; convert
	// only when it isn't already the texture's ARGB8888.
	SDL_Surface* src       = shot;
	SDL_Surface* converted = nullptr;
	if(shot->format != SDL_PIXELFORMAT_ARGB8888){
		converted = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_ARGB8888);
		if(!converted){
			Logger::Instance().Errorf("Capture: surface convert failed: %s",
				SDL_GetError());
			SDL_DestroySurface(shot);
			return;
		}
		src = converted;
	}

	// CPU→GPU upload onto the capture renderer's streaming texture.
	if(SDL_UpdateTexture(captureTexture, nullptr, src->pixels, src->pitch)){
		captureTextureReady = true;
	}

	if(converted) SDL_DestroySurface(converted);
	SDL_DestroySurface(shot);
}

void Renderer::PresentCapture(){
	if(!captureRenderer) return;

	SetDrawColour(captureRenderer, Colours::BLACK);
	SDL_RenderClear(captureRenderer);
	if(captureTexture && captureTextureReady){
		// Logical size matches the texture, so a null-dst copy fills the
		// logical frame. SDL handles scaling to the physical window.
		SDL_RenderTexture(captureRenderer, captureTexture, nullptr, nullptr);
	}
	SDL_RenderPresent(captureRenderer);
}

int Renderer::GetDisplayCount() const{
	return DisplayCount();
}

std::string Renderer::GetDisplayName(int index) const{
	SDL_DisplayID id = DisplayIdForIndex(index);
	const char* name = id ? SDL_GetDisplayName(id) : nullptr;
	if(name) return std::string(name);
	return "Display " + std::to_string(index);
}

} // namespace SatyrAV
