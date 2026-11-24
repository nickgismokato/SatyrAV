#pragma once
#include "render/Renderer.hpp"
#include "render/TextRenderer.hpp"
#include "input/InputHandler.hpp"
#include "screens/Screen.hpp"
#include "ui/HelpPopup.hpp"
#include "ui/DebugPopup.hpp"
#include "ui/DisplaySizePopup.hpp"
#include "ui/ScannerPopup.hpp"
#include <unordered_map>
#include <memory>

namespace SatyrAV{

class App{
public:
	bool Init();
	void Run();
	// One frame of Run's loop. For the render tests.
	void Frame(float dt);
	bool IsRunning() const{ return running; }
	// For the render tests: the project screen.
	Screen* GetScreen(ScreenID id){ return screens[id].get(); }
	void Shutdown();

	void SwitchScreen(ScreenID id);
	void OpenProject(const ProjectData& project);
	void Quit();

	Renderer& GetRenderer();
	TextRenderer& GetTextRenderer();
	InputHandler& GetInput();
	DebugPopup& GetDebugPopup();

	bool IsInProject() const;

private:
	bool running = false;
	bool inProject = false;
	Renderer renderer;
	TextRenderer textRenderer;
	InputHandler input;

	HelpPopup helpPopup;
	DebugPopup debugPopup;
	DisplaySizePopup sizePopup;
	// Project scanner — modal, opened with C (or the `D → O` chord) while a
	// project is loaded. Replaces the 1.6.3 Overview popup.
	ScannerPopup scannerPopup;

	std::unordered_map<ScreenID, std::unique_ptr<Screen>> screens;
	Screen* currentScreen = nullptr;
	ScreenID currentScreenID = ScreenID::Splash;

	void RegisterScreens();
};

} // namespace SatyrAV
