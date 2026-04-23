#pragma once
#include "input/InputHandler.hpp"
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"

namespace SatyrAV{

class App; // Forward declaration

enum class ScreenID{
	Splash,
	MainMenu,
	NewProject,
	LoadProject,
	Options,
	Performance
};

class Screen{
public:
	virtual ~Screen() = default;

	virtual void OnEnter(){}
	virtual void OnExit(){}
	virtual void HandleInput(InputAction action) = 0;
	virtual void Update(float deltaTime) = 0;
	virtual void Draw(Renderer& r, TextRenderer& text) = 0;

	App* app = nullptr;
};

} // namespace SatyrAV
