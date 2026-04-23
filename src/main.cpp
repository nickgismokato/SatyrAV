#include "App.hpp"
#include <cstdio>

int main(int argc, char* argv[]){
	(void)argc; (void)argv;

	SatyrAV::App app;

	if(!app.Init()){
		fprintf(stderr, "Failed to initialise SatyrAV\n");
		return 1;
	}

	app.Run();
	app.Shutdown();
	return 0;
}
