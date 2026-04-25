#pragma once
#include "core/Types.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

namespace SatyrAV{

class TextRenderer{
public:
	// (1.6.1) Init takes the four style face paths. `fontPath` (regular) is
	// required. The italic/bold/bold-italic paths are optional — leave empty
	// to disable that face; the renderer falls back to regular when a run
	// asks for an unloaded style. Master and slave share the same set of
	// face paths but each has its own size; see SetSlaveFontPaths for runtime
	// switching when a project overrides the global config.
	bool Init(const std::string& fontPath, int fontSize,
		const std::string& fontPathItalic = "",
		const std::string& fontPathBold = "",
		const std::string& fontPathBoldItalic = "");
	void Shutdown();

	void DrawText(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour = Colours::ORANGE);

	void DrawTextCentered(SDL_Renderer* renderer,
		const std::string& text,
		int centerX, int y,
		Colour colour = Colours::ORANGE);

	// Slave display text (uses slaveFontSize). The optional bold/italic
	// flags pick the matching loaded face — regular / italic / bold /
	// bold-italic; falls back to regular when the requested variant is
	// not configured for this project.
	void DrawTextSlave(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour = Colours::WHITE,
		bool bold = false, bool italic = false);

	void DrawTextCenteredSlave(SDL_Renderer* renderer,
		const std::string& text,
		int centerX, int y,
		Colour colour = Colours::WHITE,
		bool bold = false, bool italic = false);

	// Rotated slave text. The rotation pivot is the centre of the rendered
	// glyph bounds; the outline rotates with the fill so they stay aligned.
	void DrawTextSlaveRotated(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour,
		float rotationDeg,
		bool bold = false, bool italic = false);

	// Measure slave-font glyph bounds for a given string.
	void MeasureSlave(const std::string& text, int& w, int& h,
		bool bold = false, bool italic = false) const;

	void SetSlaveFontSize(int size);
	int GetSlaveFontSize() const;
	int GetSlaveLineHeight() const;

	// (1.6.1) Re-load the slave-side italic/bold/bold-italic faces. Called
	// by PerformanceScreen when a project's effective font cascade resolves
	// to different paths than the Config-level defaults. Empty path leaves
	// that face unloaded (i.e. that style falls back to regular).
	void SetSlaveFontPaths(const std::string& regular,
		const std::string& italic,
		const std::string& bold,
		const std::string& boldItalic);

	// (1.5) Canvas-relative text sizing. slaveScale multiplies the
	// rendered glyph dimensions. PerformanceScreen sets this each frame
	// to (currentTargetHeight / physicalSlaveHeight) so that text shrinks
	// with a shrinking TargetedDisplay rect — which makes the capture
	// mirror show a constant text size regardless of rect scale (the
	// capture's SDL_RenderSetLogicalSize stretch factor cancels out the
	// per-frame scale). At scale 1.0 the render path is identical to the
	// pre-1.5 pixel-absolute behaviour.
	void  SetSlaveScale(float s);
	float GetSlaveScale() const;

	int GetLineHeight() const;
	int MeasureWidth(const std::string& text) const;

	// Outline thickness for slave text (0 = disabled)
	int outlineThickness = 4;

private:
	// (1.6.1) Style-indexed faces. Index = (bold ? 2 : 0) | (italic ? 1 : 0).
	// 0 = regular, 1 = italic, 2 = bold, 3 = bold-italic. Slot 0 must always
	// be loaded; the others may be nullptr → caller falls back to regular.
	TTF_Font* fonts[4]      = {nullptr, nullptr, nullptr, nullptr};
	TTF_Font* slaveFonts[4] = {nullptr, nullptr, nullptr, nullptr};
	std::string fontPaths[4];      // master-side (size-independent)
	std::string slaveFontPaths[4]; // slave-side (may differ via project override)
	int lineHeight      = 0;
	int slaveLineHeight = 0;
	int slaveFontSize   = 25;
	float slaveScale    = 1.0f;

	// Pick the best-fit slave face for the requested style, falling back
	// to regular when the variant isn't loaded.
	TTF_Font* PickSlaveFont(bool bold, bool italic) const;
	// Open all four slave faces from `slaveFontPaths` at the given size.
	// Empty paths leave the slot null. Called when the size changes or when
	// SetSlaveFontPaths swaps the path set.
	void OpenSlaveFonts();
	void CloseSlaveFonts();
};

} // namespace SatyrAV
