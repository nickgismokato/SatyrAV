// Entry point for the render tests. Before anything touches SDL or the
// user's folders: off-screen video and dummy audio, and HOME / USERPROFILE
// pointed at a scratch folder, so config.toml, the logs and the projects
// folder the app would use are all throwaway.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv){
	namespace fs = std::filesystem;
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path home = fs::temp_directory_path() / ("satyrav-render-home-" + std::to_string(stamp));
	fs::create_directories(home);

	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
	SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
#ifdef _WIN32
	_putenv_s("USERPROFILE", home.string().c_str());
	_putenv_s("SDL_VIDEO_DRIVER", "offscreen");
	_putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
	setenv("HOME", home.string().c_str(), 1);
	setenv("SDL_VIDEO_DRIVER", "offscreen", 1);
	setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif

	// FindAsset looks for assets/ (the bundled fonts) from here.
	std::error_code ec;
	fs::current_path(SATYR_SOURCE_ROOT, ec);

	doctest::Context ctx(argc, argv);
	const int result = ctx.run();

	fs::remove_all(home, ec);
	return result;
}
