#pragma once
#include "screens/Screen.hpp"
#include <SDL2/SDL.h>

namespace SatyrAV{

class MainMenuScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	int selectedOption = 0;
	static constexpr int OPTION_COUNT = 3;
	const char* OPTIONS[OPTION_COUNT] = {
		"New project",
		"Load project",
		"Options"
	};

	SDL_Texture* logoTexture = nullptr;
	int logoW = 0, logoH = 0;
};

} // namespace SatyrAV
