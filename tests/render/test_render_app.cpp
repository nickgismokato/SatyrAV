// The whole app, off-screen: a small project is opened and run with key
// presses, exactly as an operator would. Covers what lives in
// PerformanceScreen and nowhere else: the [Animation] runner, the show
// log and the per-act subtitles, and the keys App handles itself.

#include "render_helpers.hpp"
#include "helpers.hpp"
#include "App.hpp"
#include "core/Config.hpp"
#include "core/Project.hpp"
#include "screens/PerformanceScreen.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace SatyrRender;
using SatyrTest::TempDir;
namespace fs = std::filesystem;

namespace{

void SaveBmp(const std::string& path, uint8_t r, uint8_t g, uint8_t b){
	fs::create_directories(fs::path(path).parent_path());
	SDL_Surface* s = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
	SDL_FillSurfaceRect(s, nullptr, SDL_MapSurfaceRGBA(s, r, g, b, 255));
	SDL_SaveBMP(s, path.c_str());
	SDL_DestroySurface(s);
}

std::string ReadAll(const fs::path& p){
	std::ifstream f(p, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// The one file in `dir` whose name ends with `suffix`.
fs::path FindFile(const fs::path& dir, const std::string& suffix){
	std::error_code ec;
	for(auto& e : fs::directory_iterator(dir, ec)){
		const std::string n = e.path().filename().string();
		if(n.size() >= suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
			return e.path();
	}
	return {};
}

// Writes the test project: two akter, one scene each.
std::string MakeProject(const TempDir& tmp){
	const std::string p = tmp.File("TestRevy_2026");
	tmp.Write("TestRevy_2026/schema.toml",
		"[RevyData]\nRevy = \"TestRevy 2026\"\nCreator = \"Test\"\n"
		"[Structure]\nakter = 2\n"
		"[Structure.Akt1]\nschema = [\"A\"]\n"
		"[Structure.Akt2]\nschema = [\"B\"]\n");
	tmp.Write("TestRevy_2026/scenes/A.ngk",
		"[Animation]\n"
		"Ball{\n"
		"    show     = ball.bmp\n"
		"    path     = W, E\n"
		"    duration = 300\n"
		"    repeat   = 3\n"
		"    every    = 100\n"
		"}\n"
		"[Commands]\n"
		"text \"Hello\"\n"
		"show bg.bmp\n"
		"show Ball\n"
		"clear\n");
	tmp.Write("TestRevy_2026/scenes/B.ngk",
		"[Commands]\n"
		"text bold(\"Bye\")\n");
	SaveBmp(p + "/pictures/bg.bmp", 0, 0, 255);
	SaveBmp(p + "/pictures/ball.bmp", 255, 0, 0);
	return p;
}

class AppRig{
public:
	App app;
	bool ok = false;

	AppRig(bool showLog){
		ok = app.Init();
		auto& cfg = Config::Instance();
		cfg.slaveWindowed = true;
		cfg.captureEnabled = false;
		cfg.showLogEnabled = showLog;
	}
	~AppRig(){ app.Shutdown(); }

	PerformanceScreen& Perf(){
		return *static_cast<PerformanceScreen*>(app.GetScreen(ScreenID::Performance));
	}

	void Frames(int n){
		for(int i = 0; i < n; i++){ app.Frame(0.016f); SleepMs(16); }
	}

	void Key(SDL_Keycode key, SDL_Scancode sc, SDL_Keymod mod = SDL_KMOD_NONE){
		SDL_Event e{};
		e.type = SDL_EVENT_KEY_DOWN;
		e.key.key = key;
		e.key.scancode = sc;
		e.key.mod = mod;
		e.key.down = true;
		SDL_PushEvent(&e);
		Frames(2);
	}
	void Enter(){ Key(SDLK_RETURN, SDL_SCANCODE_RETURN); }

	// Opens the project and walks into the first scene's commands.
	void OpenAndEnter(const std::string& path){
		app.OpenProject(Project::Load(path));
		Frames(2);
		Key(SDLK_DOWN, SDL_SCANCODE_DOWN);
		Key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
		Key(SDLK_DOWN, SDL_SCANCODE_DOWN);
		Key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
	}

	bool HasText(const std::string& s){
		for(const auto& e : Perf().GetSlaveWindow().GetTextEntries())
			for(const auto& r : e.runs)
				if(r.text == s) return true;
		return false;
	}
	int AnimationImages(){
		int n = 0;
		for(const auto& im : Perf().GetSlaveWindow().GetImageEntries())
			if(im.groupName.rfind("\x01" "anim:", 0) == 0) n++;
		return n;
	}
	bool HasImage(const std::string& fileName){
		for(const auto& im : Perf().GetSlaveWindow().GetImageEntries()){
			const std::string& s = im.source;
			if(s.size() >= fileName.size() &&
			   s.compare(s.size() - fileName.size(), fileName.size(), fileName) == 0) return true;
		}
		return false;
	}
};

} // namespace

TEST_CASE("App: a show run with keys - cues, animation, show log and per-act subtitles"){
	TempDir tmp("render-app");
	const std::string project = MakeProject(tmp);
	AppRig rig(true);
	REQUIRE(rig.ok);
	rig.OpenAndEnter(project);

	rig.Enter();                                   // text "Hello"
	REQUIRE(rig.HasText("Hello"));
	rig.Enter();                                   // show bg.bmp
	CHECK(rig.HasImage("bg.bmp"));

	// Keys App and the screen handle with no video playing: nothing breaks.
	rig.Key(SDLK_COMMA, SDL_SCANCODE_COMMA);
	rig.Key(SDLK_LESS, SDL_SCANCODE_COMMA, SDL_KMOD_LSHIFT);
	rig.Key(SDLK_C, SDL_SCANCODE_C);               // scanner opens...
	rig.Key(SDLK_C, SDL_SCANCODE_C);               // ...and closes
	CHECK(rig.HasText("Hello"));

	rig.Enter();                                   // show Ball: the animation
	CHECK(WaitFor(500, [&]{ rig.Frames(1); }, [&]{ return rig.AnimationImages() > 0; }));
	// Three passes of 300 ms, 100 ms apart: all gone within a second.
	CHECK(WaitFor(1500, [&]{ rig.Frames(1); }, [&]{ return rig.AnimationImages() == 0; }));

	rig.Enter();                                   // clear
	CHECK_FALSE(rig.HasText("Hello"));
	CHECK_FALSE(rig.HasImage("bg.bmp"));

	rig.Enter();                                   // to the next scene (Akt 2)
	rig.Enter();                                   // primes its first cue
	rig.Enter();                                   // text bold("Bye")
	REQUIRE(rig.HasText("Bye"));
	rig.Frames(10);

	rig.Key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);     // leaving ends the show
	rig.Frames(2);

	const fs::path logDir = fs::path(project) / "showlog";
	const std::string log = ReadAll(FindFile(logDir, "-1.log"));
	INFO(log);
	CHECK(log.find("Started TestRevy2026") != std::string::npos);
	CHECK(log.find("Akt 1 | Scene A | started act") != std::string::npos);
	CHECK(log.find("text \"Hello\"") != std::string::npos);
	CHECK(log.find("on screen: [PICTURE] bg.bmp") != std::string::npos);
	CHECK(log.find("off screen: [PICTURE] bg.bmp") != std::string::npos);
	CHECK(log.find(",: ") == std::string::npos);   // no video: seek keys log nothing
	CHECK(log.find("Akt 1 | Scene A | ended act") != std::string::npos);
	CHECK(log.find("Akt 2 | Scene B | started act") != std::string::npos);
	CHECK(log.find("Ended show") != std::string::npos);

	const std::string srt1 = ReadAll(FindFile(logDir, "-Akt1.srt"));
	INFO(srt1);
	CHECK(srt1.find("Hello") != std::string::npos);
	CHECK(srt1.find("[PICTURE] bg.bmp") != std::string::npos);
	// Animation passes are left out of the subtitles.
	CHECK(srt1.find("ball.bmp") == std::string::npos);

	const std::string srt2 = ReadAll(FindFile(logDir, "-Akt2.srt"));
	CHECK(srt2.find("<b>Bye</b>") != std::string::npos);
	CHECK(srt2.find("Hello") == std::string::npos);
	const std::string ass2 = ReadAll(FindFile(logDir, "-Akt2.ass"));
	INFO(ass2);
	CHECK(ass2.find("Style: Default,DejaVu Sans,") != std::string::npos);
	CHECK(ass2.find("{\\b1}Bye") != std::string::npos);
}

TEST_CASE("App: with the show log off, a show writes no files"){
	TempDir tmp("render-app");
	const std::string project = MakeProject(tmp);
	AppRig rig(false);
	REQUIRE(rig.ok);
	rig.OpenAndEnter(project);
	rig.Enter();
	REQUIRE(rig.HasText("Hello"));
	rig.Key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
	CHECK_FALSE(fs::exists(fs::path(project) / "showlog"));
}
