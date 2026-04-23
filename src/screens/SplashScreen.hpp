#pragma once
#include "screens/Screen.hpp"
#include "core/Version.hpp"
#include <SDL2/SDL.h>

namespace SatyrAV{

class SplashScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	enum class Phase{
		Logo,
		Credits
	};

	Phase phase = Phase::Logo;
	float elapsed = 0.0f;
	bool transitioned = false;
	SDL_Texture* logoTexture = nullptr;
	int logoW = 0, logoH = 0;

	SDL_Texture* avLogoTexture = nullptr;
	int avLogoW = 0, avLogoH = 0;

	static constexpr float LOGO_DURATION    = 3.0f;
	static constexpr float CREDITS_DURATION = 3.0f;

	void NextPhase();
	void TransitionToMenu();
};

} // namespace SatyrAV
