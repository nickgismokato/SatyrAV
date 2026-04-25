#include "render/TextRenderer.hpp"
#include <cstdio>

namespace SatyrAV{

// (1.6.1) Style-slot encoding. Index 0 is regular and is always loaded; the
// others may be nullptr (in which case PickSlaveFont falls back to regular).
//   0 = regular        (bold=false, italic=false)
//   1 = italic         (bold=false, italic=true)
//   2 = bold           (bold=true,  italic=false)
//   3 = bold-italic    (bold=true,  italic=true)
static int StyleIndex(bool bold, bool italic){
	return (bold ? 2 : 0) | (italic ? 1 : 0);
}

bool TextRenderer::Init(const std::string& fontPath, int fontSize,
	const std::string& fontPathItalic,
	const std::string& fontPathBold,
	const std::string& fontPathBoldItalic){
	if(TTF_Init() < 0){
		fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
		return false;
	}

	// Master-side faces — single fixed size, opened once.
	fontPaths[0] = fontPath;
	fontPaths[1] = fontPathItalic;
	fontPaths[2] = fontPathBold;
	fontPaths[3] = fontPathBoldItalic;

	fonts[0] = TTF_OpenFont(fontPath.c_str(), fontSize);
	if(!fonts[0]){
		fprintf(stderr, "Failed to load font '%s': %s\n",
			fontPath.c_str(), TTF_GetError());
		return false;
	}
	lineHeight = TTF_FontLineSkip(fonts[0]);

	for(int i = 1; i < 4; i++){
		if(!fontPaths[i].empty()){
			fonts[i] = TTF_OpenFont(fontPaths[i].c_str(), fontSize);
			if(!fonts[i]){
				fprintf(stderr, "Note: optional font slot %d ('%s') failed: %s — falling back to regular.\n",
					i, fontPaths[i].c_str(), TTF_GetError());
			}
		}
	}

	// Slave faces start with the same path set as master. Project override
	// happens later via SetSlaveFontPaths.
	slaveFontSize = fontSize;
	for(int i = 0; i < 4; i++) slaveFontPaths[i] = fontPaths[i];
	OpenSlaveFonts();

	return true;
}

void TextRenderer::Shutdown(){
	CloseSlaveFonts();
	for(int i = 0; i < 4; i++){
		if(fonts[i]){ TTF_CloseFont(fonts[i]); fonts[i] = nullptr; }
	}
	TTF_Quit();
}

void TextRenderer::OpenSlaveFonts(){
	CloseSlaveFonts();
	for(int i = 0; i < 4; i++){
		if(slaveFontPaths[i].empty()) continue;
		slaveFonts[i] = TTF_OpenFont(slaveFontPaths[i].c_str(), slaveFontSize);
		if(!slaveFonts[i] && i != 0){
			fprintf(stderr, "Note: slave font slot %d ('%s') failed at size %d: %s\n",
				i, slaveFontPaths[i].c_str(), slaveFontSize, TTF_GetError());
		}
	}
	if(slaveFonts[0]){
		slaveLineHeight = TTF_FontLineSkip(slaveFonts[0]);
	}
}

void TextRenderer::CloseSlaveFonts(){
	for(int i = 0; i < 4; i++){
		if(slaveFonts[i]){ TTF_CloseFont(slaveFonts[i]); slaveFonts[i] = nullptr; }
	}
}

TTF_Font* TextRenderer::PickSlaveFont(bool bold, bool italic) const{
	int idx = StyleIndex(bold, italic);
	if(slaveFonts[idx]) return slaveFonts[idx];
	// Fall through: bold-italic → bold → italic → regular. This keeps the
	// effect closest to what the author asked for when only some variants
	// are configured (e.g. a project with only a Bold path picks Bold for
	// bold-italic runs rather than collapsing all the way to regular).
	if(bold && italic){
		if(slaveFonts[StyleIndex(true, false)]) return slaveFonts[StyleIndex(true, false)];
		if(slaveFonts[StyleIndex(false, true)]) return slaveFonts[StyleIndex(false, true)];
	}
	return slaveFonts[0] ? slaveFonts[0] : fonts[0];
}

void TextRenderer::DrawText(SDL_Renderer* renderer,
	const std::string& text, int x, int y, Colour colour){
	TTF_Font* f = fonts[0];
	if(!f || text.empty()) return;

	SDL_Color sdlColour = {colour.r, colour.g, colour.b, colour.a};
	SDL_Surface* surface = TTF_RenderUTF8_Blended(f, text.c_str(), sdlColour);
	if(!surface) return;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_Rect dst = {x, y, surface->w, surface->h};
	SDL_RenderCopy(renderer, texture, nullptr, &dst);

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
}

void TextRenderer::DrawTextCentered(SDL_Renderer* renderer,
	const std::string& text, int centerX, int y, Colour colour){
	int w = MeasureWidth(text);
	DrawText(renderer, text, centerX - w / 2, y, colour);
}

void TextRenderer::DrawTextSlave(SDL_Renderer* renderer,
	const std::string& text, int x, int y, Colour colour,
	bool bold, bool italic){
	TTF_Font* f = PickSlaveFont(bold, italic);
	if(!f || text.empty()) return;

	// Canvas-relative scale: glyph bitmap is rendered at native size then
	// RenderCopy'd into a scaled dst rect. The outline offset also scales
	// so the glyph baseline stays aligned with (x, y) after scaling.
	const float s = slaveScale;

	// Draw outline first (black text slightly larger)
	if(outlineThickness > 0){
		TTF_SetFontOutline(f, outlineThickness);
		SDL_Color black = {0, 0, 0, 255};
		SDL_Surface* outSurface = TTF_RenderUTF8_Blended(f, text.c_str(), black);
		if(outSurface){
			SDL_Texture* outTex = SDL_CreateTextureFromSurface(renderer, outSurface);
			int outW = (int)(outSurface->w * s);
			int outH = (int)(outSurface->h * s);
			int offs = (int)(outlineThickness * s);
			SDL_Rect outDst = {x - offs, y - offs, outW, outH};
			SDL_RenderCopy(renderer, outTex, nullptr, &outDst);
			SDL_DestroyTexture(outTex);
			SDL_FreeSurface(outSurface);
		}
		TTF_SetFontOutline(f, 0);
	}

	// Foreground text
	SDL_Color sdlColour = {colour.r, colour.g, colour.b, colour.a};
	SDL_Surface* surface = TTF_RenderUTF8_Blended(f, text.c_str(), sdlColour);
	if(!surface) return;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_Rect dst = {x, y, (int)(surface->w * s), (int)(surface->h * s)};
	SDL_RenderCopy(renderer, texture, nullptr, &dst);

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
}

void TextRenderer::DrawTextCenteredSlave(SDL_Renderer* renderer,
	const std::string& text, int centerX, int y, Colour colour,
	bool bold, bool italic){
	TTF_Font* f = PickSlaveFont(bold, italic);
	if(!f || text.empty()) return;

	int w = 0, h = 0;
	TTF_SizeUTF8(f, text.c_str(), &w, &h);
	// Use the scaled width for centering so the text visually centres on
	// centerX after DrawTextSlave applies slaveScale.
	int scaledW = (int)(w * slaveScale);
	DrawTextSlave(renderer, text, centerX - scaledW / 2, y, colour, bold, italic);
}

void TextRenderer::DrawTextSlaveRotated(SDL_Renderer* renderer,
	const std::string& text, int x, int y, Colour colour, float rotationDeg,
	bool bold, bool italic){
	TTF_Font* f = PickSlaveFont(bold, italic);
	if(!f || text.empty()) return;

	const float s = slaveScale;

	// Outline first — rotates around its own centre, which is the same
	// world point as the text centre because the outline expands evenly.
	if(outlineThickness > 0){
		TTF_SetFontOutline(f, outlineThickness);
		SDL_Color black = {0, 0, 0, 255};
		SDL_Surface* outSurface = TTF_RenderUTF8_Blended(f, text.c_str(), black);
		if(outSurface){
			SDL_Texture* outTex = SDL_CreateTextureFromSurface(renderer, outSurface);
			int outW = (int)(outSurface->w * s);
			int outH = (int)(outSurface->h * s);
			int offs = (int)(outlineThickness * s);
			SDL_Rect outDst = {x - offs, y - offs, outW, outH};
			SDL_RenderCopyEx(renderer, outTex, nullptr, &outDst,
				(double)rotationDeg, nullptr, SDL_FLIP_NONE);
			SDL_DestroyTexture(outTex);
			SDL_FreeSurface(outSurface);
		}
		TTF_SetFontOutline(f, 0);
	}

	SDL_Color sdlColour = {colour.r, colour.g, colour.b, colour.a};
	SDL_Surface* surface = TTF_RenderUTF8_Blended(f, text.c_str(), sdlColour);
	if(!surface) return;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_Rect dst = {x, y, (int)(surface->w * s), (int)(surface->h * s)};
	SDL_RenderCopyEx(renderer, texture, nullptr, &dst,
		(double)rotationDeg, nullptr, SDL_FLIP_NONE);

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
}

void TextRenderer::MeasureSlave(const std::string& text, int& w, int& h,
	bool bold, bool italic) const{
	w = h = 0;
	TTF_Font* f = PickSlaveFont(bold, italic);
	if(!f || text.empty()) return;
	TTF_SizeUTF8(f, text.c_str(), &w, &h);
	// Return scaled dims so callers laying out runs use the on-screen size.
	w = (int)(w * slaveScale);
	h = (int)(h * slaveScale);
}

void TextRenderer::SetSlaveFontSize(int size){
	if(size == slaveFontSize && slaveFonts[0]) return;
	if(slaveFontPaths[0].empty()) return;
	slaveFontSize = size;
	OpenSlaveFonts();
}

void TextRenderer::SetSlaveFontPaths(const std::string& regular,
	const std::string& italic,
	const std::string& bold,
	const std::string& boldItalic){
	// (1.6.1 fix) Empty path on any slot = "keep existing". The cascade
	// in PerformanceScreen passes empty for any face the active project
	// doesn't explicitly override; without this guard those empty values
	// would wipe the Config-level paths that App.cpp::Init loaded, and
	// every styled run would silently fall back to regular. With the
	// guard, the global default sticks unless the project's [Options]
	// names a replacement.
	if(!regular.empty())    slaveFontPaths[0] = regular;
	if(!italic.empty())     slaveFontPaths[1] = italic;
	if(!bold.empty())       slaveFontPaths[2] = bold;
	if(!boldItalic.empty()) slaveFontPaths[3] = boldItalic;
	OpenSlaveFonts();
}

int TextRenderer::GetSlaveFontSize() const{ return slaveFontSize; }

int TextRenderer::GetSlaveLineHeight() const{
	int raw = slaveLineHeight > 0 ? slaveLineHeight : lineHeight;
	return (int)(raw * slaveScale);
}

void  TextRenderer::SetSlaveScale(float s){
	if(s <= 0.0f) s = 1.0f;
	slaveScale = s;
}
float TextRenderer::GetSlaveScale() const{ return slaveScale; }

int TextRenderer::GetLineHeight() const{ return lineHeight; }

int TextRenderer::MeasureWidth(const std::string& text) const{
	if(!fonts[0]) return 0;
	int w = 0, h = 0;
	TTF_SizeUTF8(fonts[0], text.c_str(), &w, &h);
	return w;
}

} // namespace SatyrAV
