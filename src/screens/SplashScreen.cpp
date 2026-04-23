#include "screens/SplashScreen.hpp"
#include "App.hpp"
#include "core/Types.hpp"
#include "core/AssetPath.hpp"
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <string>

namespace SatyrAV{

static SDL_Texture* LoadTexture(const std::string& path, SDL_Renderer* r, int& w, int& h){
	SDL_Surface* surface = IMG_Load(path.c_str());
	if(!surface){
		fprintf(stderr, "SplashScreen: cannot load '%s': %s\n", path.c_str(), IMG_GetError());
		return nullptr;
	}
	w = surface->w;
	h = surface->h;
	SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surface);
	SDL_FreeSurface(surface);
	return tex;
}

void SplashScreen::OnEnter(){
	elapsed = 0.0f;
	phase = Phase::Logo;
	transitioned = false;

	auto* renderer = app->GetRenderer().GetMaster();

	if(logoTexture){ SDL_DestroyTexture(logoTexture); logoTexture = nullptr; }
	if(avLogoTexture){ SDL_DestroyTexture(avLogoTexture); avLogoTexture = nullptr; }

	logoTexture = LoadTexture(FindAsset("satyr-logo-body.png"), renderer, logoW, logoH);
	avLogoTexture = LoadTexture(FindAsset("satyr-av-logo.png"), renderer, avLogoW, avLogoH);
}

void SplashScreen::OnExit(){
	if(logoTexture){ SDL_DestroyTexture(logoTexture); logoTexture = nullptr; }
	if(avLogoTexture){ SDL_DestroyTexture(avLogoTexture); avLogoTexture = nullptr; }
}

void SplashScreen::HandleInput(InputAction action){
	if(action != InputAction::None){
		NextPhase();
	}
}

void SplashScreen::Update(float deltaTime){
	elapsed += deltaTime;
	float duration = (phase == Phase::Logo) ? LOGO_DURATION : CREDITS_DURATION;
	if(elapsed >= duration){
		NextPhase();
	}
}

void SplashScreen::NextPhase(){
	if(phase == Phase::Logo){
		phase = Phase::Credits;
		elapsed = 0.0f;
	} else{
		TransitionToMenu();
	}
}

static void DrawTextureCentered(SDL_Renderer* r, SDL_Texture* tex,
	int texW, int texH, int screenW, int centerY,
	float maxWidthFrac, float maxHeightFrac){
	if(!tex) return;
	int maxW = (int)(screenW * maxWidthFrac);
	int maxH = (int)((float)texH / (float)texW * maxW); // preserve aspect
	if(maxH > (int)(screenW * maxHeightFrac)){
		maxH = (int)(screenW * maxHeightFrac);
		maxW = (int)((float)texW / (float)texH * maxH);
	}
	float scaleW = (float)maxW / (float)texW;
	float scaleH = (float)maxH / (float)texH;
	float scale = (scaleW < scaleH) ? scaleW : scaleH;
	int drawW = (int)(texW * scale);
	int drawH = (int)(texH * scale);
	int drawX = screenW / 2 - drawW / 2;
	int drawY = centerY - drawH / 2;
	SDL_Rect dst = {drawX, drawY, drawW, drawH};
	SDL_RenderCopy(r, tex, nullptr, &dst);
}

void SplashScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();
	int screenW = r.GetMasterWidth();
	int screenH = r.GetMasterHeight();
	int centerX = screenW / 2;

	if(phase == Phase::Logo){
		// Phase 1: SaTyR logo centered
		DrawTextureCentered(renderer, logoTexture, logoW, logoH,
			screenW, screenH / 2, 0.6f, 0.6f);
	} else{
		// Phase 2: SatyrAV logo at top, credits below
		int logoAreaH = (int)(screenH * 0.4f);
		DrawTextureCentered(renderer, avLogoTexture, avLogoW, avLogoH,
			screenW, logoAreaH / 2 + 20, 0.55f, 0.35f);

		int y = logoAreaH + 20;
		std::string versionStr = "Version " + std::string(SATYR_AV_VERSION);
		text.DrawTextCentered(renderer, versionStr, centerX, y, Colours::WHITE);

		y += 50;
		text.DrawTextCentered(renderer, "Developed and maintained by",
			centerX, y, Colours::WHITE);

		y += 30;
		text.DrawTextCentered(renderer, "Nick Alexander Villum Laursen",
			centerX, y, Colours::ORANGE);

		y += 30;
		text.DrawTextCentered(renderer, "Nickgismokato",
			centerX, y, Colours::WHITE);
	}
}

void SplashScreen::TransitionToMenu(){
	if(transitioned) return;
	transitioned = true;
	app->SwitchScreen(ScreenID::MainMenu);
}

} // namespace SatyrAV
