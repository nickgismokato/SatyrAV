#include "screens/MainMenuScreen.hpp"
#include "App.hpp"
#include "core/AssetPath.hpp"
#include <SDL2/SDL_image.h>
#include <cstdio>

namespace SatyrAV{

void MainMenuScreen::OnEnter(){
	selectedOption = 0;

	if(!logoTexture){
		std::string logoPath = FindAsset("satyr-logo-body.png");
		SDL_Surface* surface = IMG_Load(logoPath.c_str());
		if(surface){
			logoW = surface->w;
			logoH = surface->h;
			auto* renderer = app->GetRenderer().GetMaster();
			logoTexture = SDL_CreateTextureFromSurface(renderer, surface);
			SDL_FreeSurface(surface);
		}
	}
}

void MainMenuScreen::OnExit(){
	// Keep texture alive — we return to this screen often
}

void MainMenuScreen::HandleInput(InputAction action){
	switch(action){
		case InputAction::NavUp:
			selectedOption = (selectedOption - 1 + OPTION_COUNT) % OPTION_COUNT;
			break;
		case InputAction::NavDown:
			selectedOption = (selectedOption + 1) % OPTION_COUNT;
			break;
		case InputAction::Execute:
			switch(selectedOption){
				case 0: app->SwitchScreen(ScreenID::NewProject); break;
				case 1: app->SwitchScreen(ScreenID::LoadProject); break;
				case 2: app->SwitchScreen(ScreenID::Options); break;
			}
			break;
		case InputAction::Quit:
			app->Quit();
			break;
		default: break;
	}
}

void MainMenuScreen::Update(float deltaTime){
	(void)deltaTime;
}

void MainMenuScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();
	int screenW = r.GetMasterWidth();
	int screenH = r.GetMasterHeight();
	int centerX = screenW / 2;

	// Draw logo just above center
	if(logoTexture){
		int maxW = (int)(screenW * 0.45f);
		int maxH = (int)(screenH * 0.35f);
		float scaleW = (float)maxW / (float)logoW;
		float scaleH = (float)maxH / (float)logoH;
		float scale = (scaleW < scaleH) ? scaleW : scaleH;

		int drawW = (int)(logoW * scale);
		int drawH = (int)(logoH * scale);
		int drawX = centerX - drawW / 2;
		int drawY = screenH / 3 - drawH / 2;

		SDL_Rect dst = {drawX, drawY, drawW, drawH};
		SDL_RenderCopy(renderer, logoTexture, nullptr, &dst);
	}

	// Menu options below logo
	int startY = (int)(screenH * 0.55f);
	for(int i = 0; i < OPTION_COUNT; i++){
		Colour c = (i == selectedOption) ? Colours::ORANGE : Colours::WHITE;
		text.DrawTextCentered(renderer, OPTIONS[i], centerX, startY + i * 50, c);
	}

}

} // namespace SatyrAV
