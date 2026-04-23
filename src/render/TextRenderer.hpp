#pragma once
#include "core/Types.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

namespace SatyrAV{

class TextRenderer{
public:
	bool Init(const std::string& fontPath, int fontSize);
	void Shutdown();

	void DrawText(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour = Colours::ORANGE);

	void DrawTextCentered(SDL_Renderer* renderer,
		const std::string& text,
		int centerX, int y,
		Colour colour = Colours::ORANGE);

	// Slave display text (uses slaveFontSize)
	void DrawTextSlave(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour = Colours::WHITE);

	void DrawTextCenteredSlave(SDL_Renderer* renderer,
		const std::string& text,
		int centerX, int y,
		Colour colour = Colours::WHITE);

	// Rotated slave text. The rotation pivot is the centre of the rendered
	// glyph bounds; the outline rotates with the fill so they stay aligned.
	void DrawTextSlaveRotated(SDL_Renderer* renderer,
		const std::string& text,
		int x, int y,
		Colour colour,
		float rotationDeg);

	// Measure slave-font glyph bounds for a given string.
	void MeasureSlave(const std::string& text, int& w, int& h) const;

	void SetSlaveFontSize(int size);
	int GetSlaveFontSize() const;
	int GetSlaveLineHeight() const;

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
	TTF_Font* font      = nullptr;
	TTF_Font* slaveFont = nullptr;
	std::string fontFilePath;
	int lineHeight      = 0;
	int slaveLineHeight = 0;
	int slaveFontSize   = 25;
	float slaveScale    = 1.0f;
};

} // namespace SatyrAV
