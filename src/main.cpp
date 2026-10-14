#include "App.hpp"
#include "core/Logger.hpp"
#include <cstdio>

// (2.0.0) SDL3 replaces SDL2main. Including this header renames main() to
// SDL_main() and lets SDL3::SDL3main supply the real platform entry point,
// which also guarantees SDL_SetMainReady() has run before SDL_Init().
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]){
	(void)argc; (void)argv;

	SatyrAV::App app;

	if(!app.Init()){
		// Namespace-qualified: main() sits outside namespace SatyrAV.
		// App::Init has already logged the specific cause; this is the
		// summary line, and it still reaches the file because Logger falls
		// back to stderr-only when Init never got far enough to open it.
		SatyrAV::Logger::Instance().Errorf("Failed to initialise SatyrAV");
		return 1;
	}

	app.Run();
	app.Shutdown();
	return 0;
}
