#pragma once
#include "core/Types.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>

namespace SatyrAV{

class Renderer{
public:
	bool InitMaster(int masterW, int masterH);
	// (1.5) `windowed` = true opens the slave as a resizable, decorated
	// window on the primary display instead of fullscreen. Used for
	// single-monitor development. Production setups pass false.
	bool InitSlave(int slaveDisplayIndex, bool windowed = false);
	void CloseSlave();
	void Shutdown();

	SDL_Renderer* GetMaster() const;
	SDL_Renderer* GetSlave() const;
	SDL_Window* GetMasterWindow() const;
	SDL_Window* GetSlaveWindow() const;

	void ClearMaster(Colour c = Colours::BACKGROUND);
	void ClearSlave(Colour c = Colours::BLACK);
	void PresentMaster();
	void PresentSlave();

	// Helpers
	void SetDrawColour(SDL_Renderer* r, Colour c);
	void DrawRect(SDL_Renderer* r, int x, int y, int w, int h, Colour c, bool filled = true);

	int GetMasterWidth() const;
	int GetMasterHeight() const;
	int GetSlaveWidth() const;
	int GetSlaveHeight() const;

	// (1.6) Rect-authoring-space dims — i.e. the coordinate system that
	// `TargetedDisplay` and `pos`/`size` modifiers are interpreted against.
	// In production fullscreen this is the slave display's bounds; in
	// windowed-dev mode this is `intendedSlaveW/H` (the configured slave
	// display's bounds), NOT the live window size. Use this anywhere a
	// caller needs to seed/reset rect coords or measure rect-relative
	// quantities so windowed and fullscreen behave identically.
	int GetCanvasWidth() const;
	int GetCanvasHeight() const;

	// Targeted-display offscreen target — all slave content layers
	// (background, video, image, particles, text) render into this
	// texture, which is then composited into the physical slave via
	// CompositeTargetedDisplay(). When no target is active, drawing
	// goes straight to the slave renderer.
	bool EnsureSlaveTarget(int w, int h);
	void BeginSlaveContent();
	void EndSlaveContent();
	void CompositeTargetedDisplay(const TargetedDisplay& t);

	// Returns the current drawable dimensions for slave content:
	// target dims while a target is active, physical slave dims otherwise.
	int GetTargetWidth() const;
	int GetTargetHeight() const;

	// (1.5) Ratio of the current content-target height to the reference
	// slave height (physical fullscreen bounds, or intendedSlaveH in
	// windowed-dev mode). Used by TextRenderer to render glyphs at a size
	// proportional to the TargetedDisplay rect — which is what makes the
	// capture mirror show constant-size text across rect resizes.
	// Returns 1.0 when there's no meaningful reference.
	float GetSlaveContentScale() const;

	// (1.5) Capture-mirror window. Third OS window on a separate display
	// that shows the slave's pre-targeted-display composite at native
	// slave-logical resolution — feeds a capture card / recorder.
	//
	// InitCapture returns false (and logs) if displayIndex is the same as
	// slaveDisplayIndex used for the current slave, or if the slave is
	// running in windowed-dev mode. Silently no-ops on success-with-slave-
	// absent (caller must InitSlave first).
	bool InitCapture(int displayIndex, int logicalW, int logicalH);
	void CloseCapture();
	bool HasCapture() const;
	// Readback slaveTarget → CPU buffer → capture-side streaming texture.
	// Call once per frame from the slave-draw path, after all content
	// layers have rendered into slaveTarget (i.e. after EndSlaveContent).
	// No-op if capture isn't open or slaveTarget is missing.
	void UpdateCaptureFromSlaveTarget();
	// Present the most-recently-uploaded capture texture to its window.
	void PresentCapture();

	// Display info
	int GetDisplayCount() const;
	std::string GetDisplayName(int index) const;

private:
	SDL_Window* masterWindow     = nullptr;
	SDL_Renderer* masterRenderer = nullptr;
	SDL_Window* slaveWindow      = nullptr;
	SDL_Renderer* slaveRenderer  = nullptr;

	int slaveW = 0, slaveH = 0;

	// (1.5) Windowed-slave dev mode. When true, CompositeTargetedDisplay
	// letterboxes the virtual slave (of dims intendedSlaveW/H — the
	// configured slave display's bounds) inside the window and scales
	// the targeted rect into that letterbox. Rect values in schema.toml
	// stay in production-slave pixel space, so fullscreen/windowed look
	// identical aside from scale.
	bool windowedMode    = false;
	int  intendedSlaveW  = 0;
	int  intendedSlaveH  = 0;
	// Display the fullscreen slave lives on (for capture soft guard).
	// -1 when no slave or when in windowed-dev mode.
	int  slaveDisplay    = -1;

	// Offscreen slave target
	SDL_Texture* slaveTarget = nullptr;
	int targetW = 0, targetH = 0;
	bool targetActive = false;

	// (1.5) Capture-mirror window — see InitCapture/PresentCapture.
	SDL_Window*   captureWindow   = nullptr;
	SDL_Renderer* captureRenderer = nullptr;
	SDL_Texture*  captureTexture  = nullptr;
	int captureLogicalW = 0;
	int captureLogicalH = 0;
	int captureDisplayIdx = -1;
	// CPU staging buffer for the per-frame GPU→CPU→GPU readback hop.
	// Sized to captureLogicalW * captureLogicalH * 4 (ARGB8888).
	std::vector<unsigned char> capturePixels;
	bool captureTextureReady = false;
};

} // namespace SatyrAV
