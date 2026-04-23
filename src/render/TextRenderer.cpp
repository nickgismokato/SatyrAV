#include "render/TextRenderer.hpp"
#include <cstdio>

namespace SatyrAV{

bool TextRenderer::Init(const std::string& fontPath, int fontSize){
	if(TTF_Init() < 0){
		fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
		return false;
	}

	fontFilePath = fontPath;

	font = TTF_OpenFont(fontPath.c_str(), fontSize);
	if(!font){
		fprintf(stderr, "Failed to load font '%s': %s\n",
			fontPath.c_str(), TTF_GetError());
		return false;
	}
	lineHeight = TTF_FontLineSkip(font);

	// Open slave font at default size
	slaveFontSize = fontSize;
	slaveFont = TTF_OpenFont(fontPath.c_str(), slaveFontSize);
	if(slaveFont){
		slaveLineHeight = TTF_FontLineSkip(slaveFont);
	}

	return true;
}

void TextRenderer::Shutdown(){
	if(slaveFont) TTF_CloseFont(slaveFont);
	if(font) TTF_CloseFont(font);
	slaveFont = nullptr;
	font = nullptr;
	TTF_Quit();
}

void TextRenderer::DrawText(SDL_Renderer* renderer,
	const std::string& text, int x, int y, Colour colour){
	if(!font || text.empty()) return;

	SDL_Color sdlColour = {colour.r, colour.g, colour.b, colour.a};
	SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColour);
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
	const std::string& text, int x, int y, Colour colour){
	TTF_Font* f = slaveFont ? slaveFont : font;
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
	const std::string& text, int centerX, int y, Colour colour){
	TTF_Font* f = slaveFont ? slaveFont : font;
	if(!f || text.empty()) return;

	int w = 0, h = 0;
	TTF_SizeUTF8(f, text.c_str(), &w, &h);
	// Use the scaled width for centering so the text visually centres on
	// centerX after DrawTextSlave applies slaveScale.
	int scaledW = (int)(w * slaveScale);
	DrawTextSlave(renderer, text, centerX - scaledW / 2, y, colour);
}

void TextRenderer::DrawTextSlaveRotated(SDL_Renderer* renderer,
	const std::string& text, int x, int y, Colour colour, float rotationDeg){
	TTF_Font* f = slaveFont ? slaveFont : font;
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

void TextRenderer::MeasureSlave(const std::string& text, int& w, int& h) const{
	w = h = 0;
	TTF_Font* f = slaveFont ? slaveFont : font;
	if(!f || text.empty()) return;
	TTF_SizeUTF8(f, text.c_str(), &w, &h);
	// Return scaled dims so callers laying out runs use the on-screen size.
	w = (int)(w * slaveScale);
	h = (int)(h * slaveScale);
}

void TextRenderer::SetSlaveFontSize(int size){
	if(size == slaveFontSize && slaveFont) return;
	if(fontFilePath.empty()) return;

	if(slaveFont){
		TTF_CloseFont(slaveFont);
		slaveFont = nullptr;
	}

	slaveFontSize = size;
	slaveFont = TTF_OpenFont(fontFilePath.c_str(), size);
	if(slaveFont){
		slaveLineHeight = TTF_FontLineSkip(slaveFont);
	}
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
	if(!font) return 0;
	int w = 0, h = 0;
	TTF_SizeUTF8(font, text.c_str(), &w, &h);
	return w;
}

} // namespace SatyrAV
