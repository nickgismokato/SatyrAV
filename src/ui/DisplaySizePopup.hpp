#pragma once
#include "ui/Popup.hpp"
#include "core/Types.hpp"
#include <SDL2/SDL.h>
#include <string>

namespace SatyrAV{

class SlaveWindow;

// Hidden debug menu that lets the user interactively place the targeted-
// display rect inside the physical secondary screen. Opened from the debug
// popup with S; closed with ESC which persists the rect to schema.toml.
//
// While visible:
//   Z / X      shrink / grow the rect (around its centre)
//   Q / E      rotate the rect counter-/clockwise
//   W A S D    move the rect centre
//   ESC        save and close
class DisplaySizePopup : public Popup{
public:
	// Bind the popup to the currently-open project. Loads the test pattern
	// and pushes it onto the slave image layer so the user can see the rect.
	// physW/physH are the physical slave display dimensions.
	void Open(TargetedDisplay* td, SlaveWindow* slave,
		SDL_Renderer* slaveRenderer, int physW, int physH,
		const std::string& projectPath);

	// Persist rect to schema.toml, clear the test pattern, hide.
	void Close();

	// Reset the rect to fill the physical display (no rotation).
	void ResetToDefault();

	// Hold-to-repeat scale/rotate/move from SDL keyboard state.
	void Update(float dt);

	void Draw(Renderer& r, TextRenderer& text) override;

private:
	TargetedDisplay* td = nullptr;
	SlaveWindow* slave = nullptr;
	SDL_Texture* testPattern = nullptr;
	int testW = 0, testH = 0;
	int physW = 0, physH = 0;
	std::string projectPath;

	// The S key that opened the popup is still physically held down when
	// Open() returns. We must not treat that as a "move centre down" input.
	// Gate all polling until S is released at least once.
	bool waitForSRelease = false;

	// Sub-pixel movement accumulator. centerX/centerY are ints, so at
	// high frame rates (120+ Hz) + SHIFT precision (0.1x) the per-frame
	// delta rounds to zero and motion stalls. Accumulate fractional
	// pixels here, drain whole pixels into centerX/centerY each frame.
	float moveAccumX = 0.0f;
	float moveAccumY = 0.0f;

	// Float mirrors of td->width/height for the same reason: the scale
	// factor per frame under SHIFT precision is ~1.0003, so rounding
	// back to int each frame cancels the growth. Keep precise floats
	// while the popup is open, round into td->width/height each frame.
	float fWidth  = 0.0f;
	float fHeight = 0.0f;
};

} // namespace SatyrAV
