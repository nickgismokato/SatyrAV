#include "ui/DisplaySizePopup.hpp"
#include "render/SlaveWindow.hpp"
#include "core/AssetPath.hpp"
#include "core/Project.hpp"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace SatyrAV{

namespace{
	// Hold-to-repeat rates, all expressed per second.
	constexpr float SCALE_FACTOR_PER_SEC = 1.5f;   // X grows ×1.5/sec, Z shrinks by 1/1.5
	constexpr float ROTATE_DEG_PER_SEC   = 60.0f;
	constexpr float MOVE_PX_PER_SEC      = 500.0f;
	constexpr int   MIN_DIM              = 32;
}

void DisplaySizePopup::Open(TargetedDisplay* targeted, SlaveWindow* slaveWin,
	SDL_Renderer* slaveRenderer, int physicalW, int physicalH,
	const std::string& path){
	td         = targeted;
	slave      = slaveWin;
	physW      = physicalW;
	physH      = physicalH;
	projectPath = path;
	waitForSRelease = true;

	// Seed float mirrors from the rect's current int dims so scale
	// accumulation starts from the right place.
	if(td){
		fWidth  = (float)td->width;
		fHeight = (float)td->height;
	}

	// Load the test pattern if the slave renderer is available.
	if(slaveRenderer){
		std::string assetPath = FindAsset("SatyrAV_4k-test.png");
		testPattern = IMG_LoadTexture(slaveRenderer, assetPath.c_str());
		if(testPattern){
			SDL_QueryTexture(testPattern, nullptr, nullptr, &testW, &testH);
			// Push onto the slave image layer so DrawMedia draws it inside
			// the targeted rect. Default RenderModifiers → fit + centre.
			if(slave){
				RenderModifiers mods;
				slave->ShowImage(testPattern, testW, testH, mods);
			}
		} else{
			fprintf(stderr, "DisplaySizePopup: failed to load %s: %s\n",
				assetPath.c_str(), IMG_GetError());
		}
	}

	Show();
}

void DisplaySizePopup::Close(){
	// Persist the edited rect. Project::Save only touches the [Display]
	// section — existing Options/RevyData/Structure are preserved.
	if(!projectPath.empty() && td){
		ProjectData tmp;
		tmp.projectPath = projectPath;
		tmp.schema.targetedDisplay = *td;
		Project::Save(tmp);
	}

	// Remove the test pattern from the slave before destroying it.
	if(slave) slave->ClearImage();
	if(testPattern){
		SDL_DestroyTexture(testPattern);
		testPattern = nullptr;
	}
	testW = testH = 0;

	td = nullptr;
	slave = nullptr;
	projectPath.clear();
	waitForSRelease = false;
	moveAccumX = moveAccumY = 0.0f;

	Hide();
}

void DisplaySizePopup::ResetToDefault(){
	if(!td) return;
	td->width    = physW;
	td->height   = physH;
	td->centerX  = physW / 2;
	td->centerY  = physH / 2;
	td->rotation = 0.0f;
	// Re-seed float mirrors after a reset so subsequent scaling accumulates
	// against the new dims, not the pre-reset ones.
	fWidth  = (float)physW;
	fHeight = (float)physH;
	moveAccumX = moveAccumY = 0.0f;
}

void DisplaySizePopup::Update(float dt){
	if(!visible || !td || dt <= 0.0f) return;

	const Uint8* keys = SDL_GetKeyboardState(nullptr);
	if(!keys) return;

	// The S key that opened the popup is still held. Wait for the user to
	// let it go before polling anything, otherwise the first frames would
	// instantly drag the centre downward.
	if(waitForSRelease){
		if(!keys[SDL_SCANCODE_S]) waitForSRelease = false;
		return;
	}

	// SHIFT enables precision mode: 0.1x rate for move/rotate, 0.25x for
	// scale. Scale gets a looser multiplier because at small dims the
	// multiplicative factor × current width rounds to the same int for
	// many frames, making the rect appear to freeze between jumps. 0.25x
	// keeps individual steps visible while still being "fine".
	bool shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
	float precision      = shift ? 0.1f  : 1.0f;
	float scalePrecision = shift ? 0.25f : 1.0f;

	// Scale around centre (multiplicative). Accumulate on fWidth/fHeight
	// floats; rounding into int each frame would cancel tiny growth.
	if(keys[SDL_SCANCODE_X]){
		float factor = std::pow(SCALE_FACTOR_PER_SEC, dt * scalePrecision);
		fWidth  *= factor;
		fHeight *= factor;
	}
	if(keys[SDL_SCANCODE_Z]){
		float factor = std::pow(1.0f / SCALE_FACTOR_PER_SEC, dt * scalePrecision);
		fWidth  *= factor;
		fHeight *= factor;
	}
	if(fWidth  < (float)MIN_DIM) fWidth  = (float)MIN_DIM;
	if(fHeight < (float)MIN_DIM) fHeight = (float)MIN_DIM;
	td->width  = (int)std::round(fWidth);
	td->height = (int)std::round(fHeight);

	// Rotate.
	if(keys[SDL_SCANCODE_Q]) td->rotation -= ROTATE_DEG_PER_SEC * dt * precision;
	if(keys[SDL_SCANCODE_E]) td->rotation += ROTATE_DEG_PER_SEC * dt * precision;
	// Keep rotation in (-360, 360) just so the number doesn't blow up.
	if(td->rotation >=  360.0f) td->rotation = std::fmod(td->rotation, 360.0f);
	if(td->rotation <= -360.0f) td->rotation = std::fmod(td->rotation, 360.0f);

	// Move centre — no clamping; overhanging off the physical screen is
	// visible as black-on-black and is allowed on purpose. Accumulate
	// sub-pixel deltas so SHIFT-precision motion doesn't get rounded away
	// at high frame rates (see header comment on moveAccumX/Y).
	float step = MOVE_PX_PER_SEC * dt * precision;
	if(keys[SDL_SCANCODE_W]) moveAccumY -= step;
	if(keys[SDL_SCANCODE_S]) moveAccumY += step;
	if(keys[SDL_SCANCODE_A]) moveAccumX -= step;
	if(keys[SDL_SCANCODE_D]) moveAccumX += step;

	int dx = (int)moveAccumX;
	int dy = (int)moveAccumY;
	if(dx != 0){ td->centerX += dx; moveAccumX -= (float)dx; }
	if(dy != 0){ td->centerY += dy; moveAccumY -= (float)dy; }
}

void DisplaySizePopup::Draw(Renderer& r, TextRenderer& text){
	if(!visible) return;

	int lineH = text.GetLineHeight() + 2;
	int width = 380;
	int height = lineH * 11 + 60;
	DrawBox(r, text, "Display Size", width, height);

	auto* renderer = r.GetMaster();
	Colour hdr = Colours::ORANGE;
	Colour txt = Colours::WHITE;

	int lx = boxX + 16;
	int y  = boxY + 40;

	if(!td){
		text.DrawText(renderer, "(no project)", lx, y, txt);
		return;
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "Size:    %d x %d", td->width, td->height);
	text.DrawText(renderer, buf, lx, y, txt); y += lineH;
	snprintf(buf, sizeof(buf), "Centre:  (%d, %d)", td->centerX, td->centerY);
	text.DrawText(renderer, buf, lx, y, txt); y += lineH;
	snprintf(buf, sizeof(buf), "Rotate:  %.1f deg", td->rotation);
	text.DrawText(renderer, buf, lx, y, txt); y += lineH;
	snprintf(buf, sizeof(buf), "Display: %d x %d", physW, physH);
	text.DrawText(renderer, buf, lx, y, txt); y += lineH + 6;

	text.DrawText(renderer, "Controls", lx, y, hdr); y += lineH;
	text.DrawText(renderer, "  Z/X     shrink / grow", lx, y, txt); y += lineH;
	text.DrawText(renderer, "  Q/E     rotate",        lx, y, txt); y += lineH;
	text.DrawText(renderer, "  WASD    move centre",   lx, y, txt); y += lineH;
	text.DrawText(renderer, "  SHIFT+  precision (10x slower)", lx, y, txt); y += lineH;
	text.DrawText(renderer, "  C       reset to default", lx, y, txt); y += lineH;
	text.DrawText(renderer, "  ESC     save & close",  lx, y, hdr);
}

} // namespace SatyrAV
