#include "App.hpp"
#include "core/Config.hpp"
#include "core/Platform.hpp"
#include "core/AssetPath.hpp"
#include "screens/SplashScreen.hpp"
#include "screens/MainMenuScreen.hpp"
#include "screens/NewProjectScreen.hpp"
#include "screens/LoadProjectScreen.hpp"
#include "screens/OptionsScreen.hpp"
#include "screens/PerformanceScreen.hpp"
#include "input/KeyBindings.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>

namespace SatyrAV{

bool App::Init(){
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0){
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	auto& config = Config::Instance();
	config.Load(Platform::GetDefaultConfigPath());

	if(!renderer.InitMaster(config.masterWidth, config.masterHeight)){
		return false;
	}

	// Set window icon
	std::string iconPath = FindAsset("satyr-av-logo.png");
	SDL_Surface* iconSurface = IMG_Load(iconPath.c_str());
	if(iconSurface){
		SDL_SetWindowIcon(renderer.GetMasterWindow(), iconSurface);
		SDL_FreeSurface(iconSurface);
	}

	std::string fontPath = config.fontPath;
	if(fontPath.empty()){
		const char* FALLBACK_FONTS[] = {
			"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
			"C:\\Windows\\Fonts\\arial.ttf",
			"assets/font.ttf",
			nullptr
		};
		for(int i = 0; FALLBACK_FONTS[i]; i++){
			FILE* f = fopen(FALLBACK_FONTS[i], "r");
			if(f){ fclose(f); fontPath = FALLBACK_FONTS[i]; break; }
		}
	}

	if(!textRenderer.Init(fontPath, config.fontSize)){
		fprintf(stderr, "Warning: Text rendering unavailable\n");
	}

	RegisterScreens();
	SwitchScreen(ScreenID::Splash);
	running = true;
	return true;
}

void App::RegisterScreens(){
	auto Register = [&](ScreenID id, std::unique_ptr<Screen> screen){
		screen->app = this;
		screens[id] = std::move(screen);
	};

	Register(ScreenID::Splash,      std::make_unique<SplashScreen>());
	Register(ScreenID::MainMenu,    std::make_unique<MainMenuScreen>());
	Register(ScreenID::NewProject,  std::make_unique<NewProjectScreen>());
	Register(ScreenID::LoadProject, std::make_unique<LoadProjectScreen>());
	Register(ScreenID::Options,     std::make_unique<OptionsScreen>());
	Register(ScreenID::Performance, std::make_unique<PerformanceScreen>());
}

void App::SwitchScreen(ScreenID id){
	// Close the size popup *before* OnExit so its SDL texture is destroyed
	// while the slave renderer still exists, and so any pending rect edits
	// are persisted even if the user leaves the screen another way.
	if(sizePopup.IsVisible()) sizePopup.Close();

	if(currentScreen) currentScreen->OnExit();

	// (1.5) Capture mirror tears down when leaving the project — the
	// window is only meaningful while a scene is playing. The saved
	// config values are untouched, so reopening a project spins it back
	// up automatically.
	if(currentScreenID == ScreenID::Performance && id != ScreenID::Performance){
		renderer.CloseCapture();
	}

	currentScreenID = id;
	inProject = (id == ScreenID::Performance);
	currentScreen = screens[id].get();
	currentScreen->OnEnter();

	helpPopup.Hide();
	debugPopup.Hide();
}

void App::OpenProject(const ProjectData& project){
	auto& config = Config::Instance();
	if(!renderer.GetSlave()){
		renderer.InitSlave(config.slaveDisplayIndex, config.slaveWindowed);
	}

	// (1.5) Capture mirror. Force-off in windowed-slave dev mode — the
	// saved config value is preserved so flipping back to fullscreen
	// auto-restores the last choice (see TODO Phase D Q4).
	if(!renderer.HasCapture() &&
	   config.captureEnabled &&
	   !config.slaveWindowed){
		// Logical size = slave's logical output = the offscreen content
		// surface size. That surface is (re)sized per-frame to the
		// TargetedDisplay rect; for D.2 we use a reasonable default and
		// let D.3 keep capture in sync with slaveTarget dims.
		int logicalW = renderer.GetSlaveWidth();
		int logicalH = renderer.GetSlaveHeight();
		renderer.InitCapture(config.captureDisplayIndex, logicalW, logicalH);
	}

	config.lastProject = project.projectPath;
	config.Save();

	auto* perfScreen = static_cast<PerformanceScreen*>(screens[ScreenID::Performance].get());
	perfScreen->SetProject(project);
	SwitchScreen(ScreenID::Performance);
}

void App::Run(){
	uint64_t lastTick = SDL_GetPerformanceCounter();
	uint64_t freq = SDL_GetPerformanceFrequency();

	while(running){
		uint64_t now = SDL_GetPerformanceCounter();
		float dt = (float)(now - lastTick) / (float)freq;
		lastTick = now;

		SDL_Event event;
		while(SDL_PollEvent(&event)){
			if(event.type == SDL_QUIT){
				running = false;
				break;
			}

			// Mouse events for popup dragging
			if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT){
				helpPopup.HandleMouseDown(event.button.x, event.button.y);
				debugPopup.HandleMouseDown(event.button.x, event.button.y);
				sizePopup.HandleMouseDown(event.button.x, event.button.y);
			}
			if(event.type == SDL_MOUSEBUTTONUP){
				helpPopup.HandleMouseUp();
				debugPopup.HandleMouseUp();
				sizePopup.HandleMouseUp();
			}
			if(event.type == SDL_MOUSEMOTION){
				helpPopup.HandleMouseMove(event.motion.x, event.motion.y);
				debugPopup.HandleMouseMove(event.motion.x, event.motion.y);
				sizePopup.HandleMouseMove(event.motion.x, event.motion.y);
			}

			// ── Size popup has absolute input priority while visible. ──
			// ESC closes+saves, C resets to default; everything else is
			// swallowed (Z/X/Q/E/WASD hold-keys are polled via SDL keyboard
			// state in Update, optionally with SHIFT for precision).
			if(sizePopup.IsVisible()){
				if(event.type == SDL_KEYDOWN){
					if(event.key.keysym.sym == Keys::QUIT){
						sizePopup.Close();
					} else if(event.key.keysym.sym == SDLK_c){
						sizePopup.ResetToDefault();
					}
				}
				continue; // Do not forward any event to Poll/screen.
			}

			// ── Open size popup: S while debug popup is open, in project. ──
			if(event.type == SDL_KEYDOWN &&
				event.key.keysym.sym == SDLK_s &&
				debugPopup.IsVisible() &&
				inProject){
				auto* perfScreen = static_cast<PerformanceScreen*>(
					screens[ScreenID::Performance].get());
				if(perfScreen){
					sizePopup.Open(
						&perfScreen->GetTargetedDisplay(),
						&perfScreen->GetSlaveWindow(),
						renderer.GetSlave(),
						renderer.GetCanvasWidth(),
						renderer.GetCanvasHeight(),
						perfScreen->GetProjectPath());
					debugPopup.Hide();
				}
				continue;
			}

			InputAction action = input.Poll(event);

			// Global popup toggles — H and D toggle popups
			if(action == InputAction::Help){
				helpPopup.inProject = inProject;
				helpPopup.Toggle();
				continue;
			}
			if(action == InputAction::Debug){
				debugPopup.Toggle();
				continue;
			}

			// Program works normally even with popups open
			if(currentScreen) currentScreen->HandleInput(action);
		}

		if(currentScreen){
			// Size popup consumes dt for hold-to-repeat modifiers.
			if(sizePopup.IsVisible()) sizePopup.Update(dt);

			currentScreen->Update(dt);
			currentScreen->Draw(renderer, textRenderer);

			// Draw popups on top of the screen content
			helpPopup.Draw(renderer, textRenderer);
			debugPopup.Draw(renderer, textRenderer);
			sizePopup.Draw(renderer, textRenderer);

			// Single present for the entire frame
			renderer.PresentMaster();

			// (1.5) Capture mirror presents every frame App::Run draws,
			// independent of which screen is up. While outside a project
			// the slaveTarget isn't refreshed, so the capture window
			// simply repeats its last frame (or the debug placeholder).
			renderer.PresentCapture();
		}
	}
}

void App::Quit(){ running = false; }

void App::Shutdown(){
	textRenderer.Shutdown();
	renderer.Shutdown();
	SDL_Quit();
}

Renderer& App::GetRenderer(){ return renderer; }
TextRenderer& App::GetTextRenderer(){ return textRenderer; }
InputHandler& App::GetInput(){ return input; }
DebugPopup& App::GetDebugPopup(){ return debugPopup; }
bool App::IsInProject() const{ return inProject; }

} // namespace SatyrAV
