// (1.8.1) Config — config.toml round-trip.
//
// Config is a singleton, so every test here writes its paths into a temp
// directory before calling Load. That matters for more than tidiness:
// Config::Load ends with EnsureDefaults(), which *creates* the configured
// projects and fonts directories. Leaving them empty would have the suite
// scribbling in the developer's real home directory.
//
// Deliberately not covered: Config::Save() (the no-argument overload) just
// forwards to Save(configPath); testing it would only re-test the
// forwarding, and it is the overload that writes to the user's real
// config path.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/Config.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;

// Point every path-bearing field inside the temp dir, so EnsureDefaults
// has nothing left to resolve against the real home directory.
static void UseTempPaths(Config& c, const TempDir& tmp){
	c.projectsDir = (tmp.Path() / "projects").string();
	c.fontsDir    = (tmp.Path() / "FONTS").string();
	c.configPath  = tmp.File("config.toml");
}

TEST_CASE("Save then Load round-trips every field"){
	TempDir tmp("config");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	c.fontSize             = 33;
	c.fontPath             = "Regular.ttf";
	c.fontPathItalic       = "Italic.ttf";
	c.fontPathBold         = "Bold.ttf";
	c.fontPathBoldItalic   = "BoldItalic.ttf";
	c.masterWidth          = 1600;
	c.masterHeight         = 900;
	c.fullscreen           = true;
	c.slaveDisplayIndex    = 2;
	c.slaveWindowed        = true;
	c.captureEnabled       = true;
	c.captureDisplayIndex  = 3;
	c.videoPreloadBudgetMB = 2048;
	c.videoPreloadMaxSeconds = 30;
	c.videoZeroCopy = false;        // (2.0.0) non-default, to prove the round trip
	c.audioDeviceName      = "Speakers (USB Audio)";
	c.lastProject          = (tmp.Path() / "projects" / "MyRevy").string();

	const auto projects = c.projectsDir;
	const auto fonts    = c.fontsDir;
	const auto last     = c.lastProject;

	auto path = tmp.File("saved.toml");
	c.Save(path);

	// Scribble over the live values so a failed Load can't pass by accident.
	c.fontSize = 1; c.masterWidth = 1; c.fullscreen = false;
	c.audioDeviceName = "wrong";

	c.Load(path);

	CHECK(c.fontSize             == 33);
	CHECK(c.fontPath             == "Regular.ttf");
	CHECK(c.fontPathItalic       == "Italic.ttf");
	CHECK(c.fontPathBold         == "Bold.ttf");
	CHECK(c.fontPathBoldItalic   == "BoldItalic.ttf");
	CHECK(c.masterWidth          == 1600);
	CHECK(c.masterHeight         == 900);
	CHECK(c.fullscreen           == true);
	CHECK(c.slaveDisplayIndex    == 2);
	CHECK(c.slaveWindowed        == true);
	CHECK(c.captureEnabled       == true);
	CHECK(c.captureDisplayIndex  == 3);
	CHECK(c.videoPreloadBudgetMB == 2048);
	CHECK(c.videoPreloadMaxSeconds == 30);
	CHECK(c.videoZeroCopy == false);
	CHECK(c.audioDeviceName      == "Speakers (USB Audio)");
	CHECK(c.projectsDir          == projects);
	CHECK(c.fontsDir             == fonts);
	CHECK(c.lastProject          == last);
}

TEST_CASE("Windows paths survive the round-trip (1.5.1 regression)"){
	// Paths are written as TOML *literal* strings so backslashes are not
	// read as escape sequences. Before 1.5.1 a path like 'C:\Users\…' made
	// toml::parse_file throw on the next launch, silently resetting every
	// setting to its default. `\U` is the nastiest case: in a basic string
	// it starts a Unicode escape.
	TempDir tmp("winpath");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	const std::string winProjects = "C:\\Users\\nicka\\Documents\\SatyrAV\\projects";
	const std::string winFont     = "C:\\Windows\\Fonts\\arial.ttf";
	c.projectsDir = winProjects;
	c.fontPath    = winFont;
	c.fontsDir    = "C:\\Users\\nicka\\Documents\\SatyrAV\\FONTS";
	c.lastProject = "C:\\Users\\nicka\\Documents\\SatyrAV\\projects\\Revy2026";

	auto path = tmp.File("win.toml");
	c.Save(path);

	c.projectsDir = "clobbered";
	c.fontPath    = "clobbered";

	c.Load(path);

	// The real assertion: the file parsed at all, and the values came back
	// byte-for-byte rather than falling through to defaults.
	CHECK(c.projectsDir == winProjects);
	CHECK(c.fontPath    == winFont);
	CHECK(c.lastProject == "C:\\Users\\nicka\\Documents\\SatyrAV\\projects\\Revy2026");
}

TEST_CASE("Loading a malformed config falls back to defaults"){
	TempDir tmp("badconfig");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	auto path = tmp.Write("broken.toml", "[font\nsize = = 12\n");
	c.fontSize = 99;
	c.Load(path);
	// The parse threw and was swallowed; whatever was in memory stays, and
	// EnsureDefaults still runs so the path fields remain usable.
	CHECK_FALSE(c.configPath.empty());
	CHECK_FALSE(c.projectsDir.empty());
	CHECK_FALSE(c.fontsDir.empty());
}

TEST_CASE("Loading a nonexistent config leaves usable defaults"){
	TempDir tmp("missing");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	c.Load(tmp.File("not-here.toml"));
	CHECK(c.configPath == tmp.File("not-here.toml"));
	CHECK_FALSE(c.projectsDir.empty());
	CHECK_FALSE(c.fontsDir.empty());
}

TEST_CASE("An empty config file gives the documented default values"){
	TempDir tmp("defaults");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	// Everything absent → value_or defaults in Config::Load.
	auto path = tmp.Write("empty.toml", "");
	c.Load(path);

	CHECK(c.fontSize             == 25);
	CHECK(c.masterWidth          == 1280);
	CHECK(c.masterHeight         == 720);
	CHECK(c.fullscreen           == false);
	CHECK(c.slaveDisplayIndex    == 1);
	CHECK(c.slaveWindowed        == false);
	CHECK(c.captureEnabled       == false);
	CHECK(c.captureDisplayIndex  == 0);
	CHECK(c.videoPreloadBudgetMB == 4096);
	CHECK(c.videoPreloadMaxSeconds == 60);
	CHECK(c.videoZeroCopy == true);  // (2.0.0) on unless switched off
	CHECK(c.audioDeviceName.empty());
	CHECK(c.fontPath.empty());
	// projectsDir / fontsDir were empty in the file, so EnsureDefaults
	// resolved them against the platform defaults rather than leaving them
	// blank — the app must always have somewhere to put projects.
	CHECK_FALSE(c.projectsDir.empty());
	CHECK_FALSE(c.fontsDir.empty());
}

TEST_CASE("EnsureDefaults creates the directories it names"){
	TempDir tmp("ensure");
	auto& c = Config::Instance();
	UseTempPaths(c, tmp);

	std::filesystem::remove_all(tmp.Path() / "projects");
	std::filesystem::remove_all(tmp.Path() / "FONTS");
	REQUIRE_FALSE(std::filesystem::exists(tmp.Path() / "projects"));

	c.EnsureDefaults();

	CHECK(std::filesystem::exists(tmp.Path() / "projects"));
	CHECK(std::filesystem::exists(tmp.Path() / "FONTS"));
}

TEST_CASE("ResolveFontPath resolves bare names against fontsDir"){
	auto& c = Config::Instance();
	const auto saved = c.fontsDir;
	c.fontsDir = "/shared/FONTS";

	auto resolved = c.ResolveFontPath("Inter.ttf");
	CHECK(resolved.find("Inter.ttf") != std::string::npos);
	CHECK(resolved.find("FONTS") != std::string::npos);
	CHECK(resolved.size() > std::string("Inter.ttf").size());

	c.fontsDir = saved;
}

TEST_CASE("ResolveFontPath leaves already-resolved paths alone"){
	auto& c = Config::Instance();
	const auto saved = c.fontsDir;
	c.fontsDir = "/shared/FONTS";

	CHECK(c.ResolveFontPath("").empty());
	// Contains a separator → treated as a real path.
	CHECK(c.ResolveFontPath("/abs/path/f.ttf")   == "/abs/path/f.ttf");
	CHECK(c.ResolveFontPath("sub/dir/f.ttf")     == "sub/dir/f.ttf");
	CHECK(c.ResolveFontPath("C:\\Windows\\f.ttf") == "C:\\Windows\\f.ttf");
	// Windows drive letter with no separator still counts as absolute.
	CHECK(c.ResolveFontPath("C:f.ttf") == "C:f.ttf");

	SUBCASE("with no fontsDir set, a bare name is returned unchanged"){
		c.fontsDir = "";
		CHECK(c.ResolveFontPath("Inter.ttf") == "Inter.ttf");
	}

	c.fontsDir = saved;
}

TEST_CASE("ResolveFontPath does not double up the separator"){
	auto& c = Config::Instance();
	const auto saved = c.fontsDir;

	c.fontsDir = "/shared/FONTS/"; // already ends in a separator
	auto resolved = c.ResolveFontPath("Inter.ttf");
	CHECK(resolved.find("//") == std::string::npos);
	CHECK(resolved.find("\\\\") == std::string::npos);

	c.fontsDir = saved;
}
