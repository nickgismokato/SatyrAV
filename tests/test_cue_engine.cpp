// (1.8.1) CueEngine — navigation and the scene → project → default
// options cascade. The cascade is the part worth guarding: it decides what
// the audience sees, and a regression in it is invisible until a show.
//
// Deliberately not covered:
//   - StopAllMedia / ToggleVideo / ToggleMusic are empty TODO stubs with no
//     behaviour to assert yet. Tests are pointless until they do something,
//     and would have to be rewritten when they do. See the note left on
//     each in CueEngine.cpp.
//   - ExecuteCurrentCommand's dispatch *effects* stop at the callback
//     boundary; what the slave then draws is render-layer behaviour and is
//     out of scope for the SDL-independent suite (deferred to 2.1.0).

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/CueEngine.hpp"

using namespace SatyrAV;

// Build a ProjectData in memory — no disk, no schema.toml. LoadProject
// takes it whole, so the cascade can be driven directly.
static ProjectData MakeProject(int aktCount = 1, int scenesPerAkt = 2){
	ProjectData p;
	p.revyName = "TestRevy";
	p.year     = 2026;
	p.schema.revyName = "TestRevy 2026";
	p.schema.aktCount = aktCount;
	for(int a = 1; a <= aktCount; a++){
		Akt akt;
		akt.number = a;
		for(int s = 0; s < scenesPerAkt; s++){
			Scene sc;
			sc.name = "Akt" + std::to_string(a) + "Scene" + std::to_string(s);
			akt.sceneNames.push_back(sc.name);
			akt.scenes.push_back(std::move(sc));
		}
		p.schema.akts.push_back(std::move(akt));
	}
	return p;
}

static Command MakeCommand(CommandType type){
	Command c;
	c.type = type;
	return c;
}

// ── Navigation ───────────────────────────────────────────────

TEST_CASE("A fresh engine has nothing selected"){
	CueEngine e;
	CHECK(e.GetCurrentAktIndex()     == -1);
	CHECK(e.GetCurrentSceneIndex()   == -1);
	CHECK(e.GetCurrentCommandIndex() == -1);
	CHECK(e.GetAktCount()    == 0);
	CHECK(e.GetSceneCount()  == 0);
	CHECK(e.GetCurrentAkt()     == nullptr);
	CHECK(e.GetCurrentScene()   == nullptr);
	CHECK(e.GetCurrentCommand() == nullptr);
}

TEST_CASE("LoadProject resets the selection"){
	CueEngine e;
	e.LoadProject(MakeProject(2, 2));
	e.SelectAkt(1);
	e.SelectScene(1);
	CHECK(e.GetCurrentAktIndex() == 1);

	e.LoadProject(MakeProject(2, 2));
	CHECK(e.GetCurrentAktIndex()     == -1);
	CHECK(e.GetCurrentSceneIndex()   == -1);
	CHECK(e.GetCurrentCommandIndex() == -1);
}

TEST_CASE("SelectAkt and SelectScene reject out-of-range indices"){
	CueEngine e;
	e.LoadProject(MakeProject(2, 3));
	CHECK(e.GetAktCount() == 2);

	e.SelectAkt(5);
	CHECK(e.GetCurrentAktIndex() == -1); // unchanged
	e.SelectAkt(-1);
	CHECK(e.GetCurrentAktIndex() == -1);

	e.SelectAkt(1);
	CHECK(e.GetCurrentAktIndex() == 1);
	CHECK(e.GetSceneCount() == 3);

	e.SelectScene(99);
	CHECK(e.GetCurrentSceneIndex() == -1);
	e.SelectScene(2);
	CHECK(e.GetCurrentSceneIndex() == 2);
}

TEST_CASE("Selecting an akt clears the scene and command selection"){
	CueEngine e;
	e.LoadProject(MakeProject(2, 2));
	e.SelectAkt(0);
	e.SelectScene(1);
	e.SetCommandIndex(0);
	CHECK(e.GetCurrentSceneIndex() == 1);

	e.SelectAkt(1);
	CHECK(e.GetCurrentSceneIndex()   == -1);
	CHECK(e.GetCurrentCommandIndex() == -1);
}

TEST_CASE("SelectScene with no akt chosen is a no-op"){
	CueEngine e;
	e.LoadProject(MakeProject(1, 2));
	e.SelectScene(0);
	CHECK(e.GetCurrentSceneIndex() == -1);
}

TEST_CASE("GetNextScene walks scenes, then akts, then stops"){
	CueEngine e;
	auto p = MakeProject(2, 2);
	e.LoadProject(p);

	e.SelectAkt(0);
	e.SelectScene(0);
	REQUIRE(e.GetNextScene() != nullptr);
	CHECK(e.GetNextScene()->name == "Akt1Scene1");
	CHECK_FALSE(e.IsLastScene());

	SUBCASE("last scene of an akt rolls into the next akt"){
		e.SelectScene(1);
		REQUIRE(e.GetNextScene() != nullptr);
		CHECK(e.GetNextScene()->name == "Akt2Scene0");
		CHECK_FALSE(e.IsLastScene());
	}
	SUBCASE("the very last scene has no next"){
		e.SelectAkt(1);
		e.SelectScene(1);
		CHECK(e.GetNextScene() == nullptr);
		CHECK(e.IsLastScene());
	}
}

TEST_CASE("Index accessors bounds-check instead of trapping"){
	CueEngine e;
	e.LoadProject(MakeProject(1, 1));
	CHECK(e.GetAkt(-1) == nullptr);
	CHECK(e.GetAkt(9)  == nullptr);
	CHECK(e.GetAkt(0)  != nullptr);
	CHECK(e.GetScene(0, -1) == nullptr);
	CHECK(e.GetScene(0, 9)  == nullptr);
	CHECK(e.GetScene(9, 0)  == nullptr);
	CHECK(e.GetScene(0, 0)  != nullptr);
	CHECK(e.GetCommand(0, 0, 0) == nullptr); // scene has no commands
	CHECK(e.GetSceneCountForAkt(9) == 0);
	CHECK(e.GetCommandCountForScene(9, 9) == 0);
}

TEST_CASE("Revy name and year come from schema and project data"){
	CueEngine e;
	e.LoadProject(MakeProject());
	CHECK(e.GetRevyName() == "TestRevy 2026");
	CHECK(e.GetRevyYear() == 2026);
}

// ── Executable-index navigation (comment rows are skipped) ───

TEST_CASE("Executable-index helpers skip comment rows"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	auto& cmds = p.schema.akts[0].scenes[0].commands;
	cmds.push_back(MakeCommand(CommandType::Comment)); // 0
	cmds.push_back(MakeCommand(CommandType::Text));    // 1
	cmds.push_back(MakeCommand(CommandType::Comment)); // 2
	cmds.push_back(MakeCommand(CommandType::Clear));   // 3
	cmds.push_back(MakeCommand(CommandType::Comment)); // 4
	e.LoadProject(p);

	CHECK(e.FirstExecutableIndex(0, 0) == 1);
	CHECK(e.LastExecutableIndex(0, 0)  == 3);
	CHECK(e.NextExecutableIndex(0, 0, 0) == 1);
	CHECK(e.NextExecutableIndex(0, 0, 2) == 3);
	CHECK(e.NextExecutableIndex(0, 0, 4) == -1); // nothing after the tail
	CHECK(e.PrevExecutableIndex(0, 0, 4) == 3);
	CHECK(e.PrevExecutableIndex(0, 0, 2) == 1);
	CHECK(e.PrevExecutableIndex(0, 0, 0) == -1);

	SUBCASE("the search is inclusive of `from`"){
		CHECK(e.NextExecutableIndex(0, 0, 1) == 1);
		CHECK(e.PrevExecutableIndex(0, 0, 3) == 3);
	}
	SUBCASE("a negative `from` clamps to the start"){
		CHECK(e.NextExecutableIndex(0, 0, -5) == 1);
	}
	SUBCASE("a `from` past the end clamps to the last row"){
		CHECK(e.PrevExecutableIndex(0, 0, 999) == 3);
	}
}

TEST_CASE("A scene of nothing but comments has no executable index"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	auto& cmds = p.schema.akts[0].scenes[0].commands;
	cmds.push_back(MakeCommand(CommandType::Comment));
	cmds.push_back(MakeCommand(CommandType::Comment));
	e.LoadProject(p);
	CHECK(e.FirstExecutableIndex(0, 0) == -1);
	CHECK(e.LastExecutableIndex(0, 0)  == -1);
}

TEST_CASE("An empty scene has no executable index"){
	CueEngine e;
	e.LoadProject(MakeProject(1, 1));
	CHECK(e.FirstExecutableIndex(0, 0) == -1);
	CHECK(e.LastExecutableIndex(0, 0)  == -1);
}

// ── Dispatch ─────────────────────────────────────────────────

TEST_CASE("ExecuteCurrentCommand fires the callback once per command"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.akts[0].scenes[0].commands.push_back(MakeCommand(CommandType::Text));
	e.LoadProject(p);

	int calls = 0;
	e.SetSlaveCallback([&](const Command&){ calls++; });
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(0);
	e.ExecuteCurrentCommand();
	CHECK(calls == 1);
}

TEST_CASE("Comment rows never reach the slave"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.akts[0].scenes[0].commands.push_back(MakeCommand(CommandType::Comment));
	e.LoadProject(p);

	int calls = 0;
	e.SetSlaveCallback([&](const Command&){ calls++; });
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(0);
	e.ExecuteCurrentCommand();
	CHECK(calls == 0);
}

TEST_CASE("A Compound fires each sub-command, skipping comments"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	Command compound = MakeCommand(CommandType::Compound);
	compound.subCommands.push_back(MakeCommand(CommandType::Clear));
	compound.subCommands.push_back(MakeCommand(CommandType::Comment));
	compound.subCommands.push_back(MakeCommand(CommandType::Text));
	p.schema.akts[0].scenes[0].commands.push_back(std::move(compound));
	e.LoadProject(p);

	std::vector<CommandType> seen;
	e.SetSlaveCallback([&](const Command& c){ seen.push_back(c.type); });
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(0);
	e.ExecuteCurrentCommand();

	REQUIRE(seen.size() == 2);
	CHECK(seen[0] == CommandType::Clear);
	CHECK(seen[1] == CommandType::Text);
}

TEST_CASE("Executing with no selection or a stale index is a safe no-op"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.akts[0].scenes[0].commands.push_back(MakeCommand(CommandType::Text));
	e.LoadProject(p);

	int calls = 0;
	e.SetSlaveCallback([&](const Command&){ calls++; });

	e.ExecuteCurrentCommand();          // nothing selected
	CHECK(calls == 0);

	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(99);              // past the end
	e.ExecuteCurrentCommand();
	CHECK(calls == 0);

	e.SetCommandIndex(-1);
	e.ExecuteCurrentCommand();
	CHECK(calls == 0);
}

TEST_CASE("Dispatch without a callback installed does not crash"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.akts[0].scenes[0].commands.push_back(MakeCommand(CommandType::Text));
	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(0);
	e.ExecuteCurrentCommand(); // no SetSlaveCallback — must be inert
	CHECK(true);
}

// ── Options cascade ──────────────────────────────────────────

TEST_CASE("Font size cascades scene → project → built-in 25"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.fontSize = 40;
	p.schema.akts[0].scenes[0].fontSize = 60;
	e.LoadProject(p);

	SUBCASE("no scene selected falls back to the project value"){
		CHECK(e.GetEffectiveFontSize() == 40);
	}
	SUBCASE("a scene value wins"){
		e.SelectAkt(0);
		e.SelectScene(0);
		CHECK(e.GetEffectiveFontSize() == 60);
		CHECK(e.GetSceneFontSize()     == 60);
		CHECK(e.GetProjectFontSize()   == 40);
	}
	SUBCASE("scene 0 means 'inherit', not 'zero'"){
		auto p2 = MakeProject(1, 1);
		p2.schema.fontSize = 40;
		p2.schema.akts[0].scenes[0].fontSize = 0;
		CueEngine e2;
		e2.LoadProject(p2);
		e2.SelectAkt(0);
		e2.SelectScene(0);
		CHECK(e2.GetEffectiveFontSize() == 40);
		CHECK(e2.GetSceneFontSize()     == 0);
	}
	SUBCASE("both unset falls through to the absolute default"){
		auto p2 = MakeProject(1, 1);
		p2.schema.fontSize = 0;
		CueEngine e2;
		e2.LoadProject(p2);
		e2.SelectAkt(0);
		e2.SelectScene(0);
		CHECK(e2.GetEffectiveFontSize() == 25);
	}
}

TEST_CASE("Background colour comes from the scene once one is selected"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.backgroundColor = {0x11, 0x22, 0x33, 0xFF};
	p.schema.akts[0].scenes[0].backgroundColor = {0x44, 0x55, 0x66, 0xFF};
	e.LoadProject(p);

	CHECK((int)e.GetEffectiveBackgroundColour().r == 0x11); // project

	e.SelectAkt(0);
	e.SelectScene(0);
	CHECK((int)e.GetEffectiveBackgroundColour().r == 0x44); // scene
}

TEST_CASE("Capitalize ORs scene onto project"){
	// A scene can turn capitalisation on, but cannot turn a project-wide
	// setting back off — worth pinning so the asymmetry is deliberate.
	auto build = [](bool project, bool scene){
		auto p = MakeProject(1, 1);
		p.schema.capitalize = project;
		p.schema.akts[0].scenes[0].capitalize = scene;
		return p;
	};

	CueEngine a; a.LoadProject(build(false, false));
	a.SelectAkt(0); a.SelectScene(0);
	CHECK_FALSE(a.GetEffectiveCapitalize());

	CueEngine b; b.LoadProject(build(false, true));
	b.SelectAkt(0); b.SelectScene(0);
	CHECK(b.GetEffectiveCapitalize());

	CueEngine c; c.LoadProject(build(true, false));
	c.SelectAkt(0); c.SelectScene(0);
	CHECK(c.GetEffectiveCapitalize());
}

TEST_CASE("Text outline is project-level only"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.textOutline = 7;
	e.LoadProject(p);
	CHECK(e.GetTextOutline() == 7);
}

TEST_CASE("Font-face cascade is independent per style"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.fontPath           = "proj-regular.ttf";
	p.schema.fontPathItalic     = "proj-italic.ttf";
	p.schema.fontPathBold       = "proj-bold.ttf";
	p.schema.fontPathBoldItalic = "proj-bolditalic.ttf";
	auto& scene = p.schema.akts[0].scenes[0];
	scene.fontPathBold = "scene-bold.ttf"; // only the bold slot overridden
	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);

	CHECK(e.GetEffectiveFontPath(false, false) == "proj-regular.ttf");
	CHECK(e.GetEffectiveFontPath(false, true)  == "proj-italic.ttf");
	CHECK(e.GetEffectiveFontPath(true,  false) == "scene-bold.ttf");
	CHECK(e.GetEffectiveFontPath(true,  true)  == "proj-bolditalic.ttf");

	SUBCASE("nothing configured returns empty, meaning 'use regular'"){
		CueEngine e2;
		e2.LoadProject(MakeProject(1, 1));
		e2.SelectAkt(0);
		e2.SelectScene(0);
		CHECK(e2.GetEffectiveFontPath(true, false).empty());
		CHECK(e2.GetEffectiveFontPath(false, true).empty());
		CHECK(e2.GetEffectiveFontPath(true, true).empty());
	}
}

TEST_CASE("Subtitle defaults are read straight off the project schema"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.subtitleItalic       = false;
	p.schema.subtitleColour       = {0x10, 0x20, 0x30, 0xFF};
	p.schema.subtitleTransparency = 0.5f;
	p.schema.subtitlePosY         = 66.0f;
	e.LoadProject(p);

	CHECK_FALSE(e.GetSubtitleItalic());
	CHECK((int)e.GetSubtitleColour().g == 0x20);
	CHECK(e.GetSubtitleTransparency() == doctest::Approx(0.5f));
	CHECK(e.GetSubtitlePosY() == doctest::Approx(66.0f));
}

TEST_CASE("Function-key audio lookup"){
	CueEngine e;
	auto p = MakeProject(1, 1);
	p.schema.functionKeyAudio[1]  = "knock.mp3";
	p.schema.functionKeyAudio[12] = "applause.mp3";
	e.LoadProject(p);

	CHECK(e.GetFunctionKeyAudio(1)  == "knock.mp3");
	CHECK(e.GetFunctionKeyAudio(12) == "applause.mp3");
	CHECK(e.GetFunctionKeyAudio(2).empty());   // unbound
	CHECK(e.GetFunctionKeyAudio(99).empty());  // out of range
	CHECK(e.GetFunctionKeyAudio(0).empty());
}

TEST_CASE("Particle tuning merges per field, scene over project"){
	CueEngine e;
	auto p = MakeProject(1, 1);

	ParticleTuning proj;
	proj.speed = 2.0f;      proj.haveSpeed   = true;
	proj.density = 3.0f;    proj.haveDensity = true;
	proj.xDist.type = Distribution::Normal;
	proj.haveXDist = true;
	p.schema.particleTunings[(int)ParticleType::Rain] = proj;

	ParticleTuning scene;
	scene.density = 9.0f;   scene.haveDensity = true; // overrides only density
	p.schema.akts[0].scenes[0].particleTunings[(int)ParticleType::Rain] = scene;

	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);

	auto t = e.GetEffectiveParticleTuning(ParticleType::Rain);
	CHECK(t.speed   == doctest::Approx(2.0f)); // from project
	CHECK(t.density == doctest::Approx(9.0f)); // from scene
	CHECK(t.xDist.type == Distribution::Normal);

	SUBCASE("an untuned type falls through to the built-in identity values"){
		auto snow = e.GetEffectiveParticleTuning(ParticleType::Snow);
		CHECK(snow.speed   == doctest::Approx(1.0f));
		CHECK(snow.density == doctest::Approx(1.0f));
		CHECK(snow.xDist.type == Distribution::Uniform);
		CHECK_FALSE(snow.haveSpeed);
	}
	SUBCASE("with no scene selected only the project level applies"){
		CueEngine e2;
		e2.LoadProject(p);
		auto t2 = e2.GetEffectiveParticleTuning(ParticleType::Rain);
		CHECK(t2.density == doctest::Approx(3.0f));
	}
}

// ── ReloadScenes ─────────────────────────────────────────────

TEST_CASE("ReloadScenes re-reads from disk and clamps a stale index"){
	SatyrTest::TempDir tmp("reload");
	std::filesystem::create_directories(tmp.Path() / "scenes");

	// Four commands to start with.
	tmp.Write("scenes/S.ngk",
		"[Commands]\ntext \"a\"\ntext \"b\"\ntext \"c\"\ntext \"d\"\n");

	ProjectData p;
	p.projectPath = tmp.Str();
	p.schema.aktCount = 1;
	Akt akt;
	akt.number = 1;
	akt.sceneNames.push_back("S");
	akt.scenes.push_back(SceneParser::ParseFile(tmp.File("scenes/S.ngk")));
	p.schema.akts.push_back(std::move(akt));

	CueEngine e;
	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(3);
	REQUIRE(e.GetCommandCount() == 4);

	// Shrink the file behind the engine's back, then reload.
	tmp.Write("scenes/S.ngk", "[Commands]\ntext \"a\"\n");
	e.ReloadScenes();

	CHECK(e.GetCommandCount() == 1);
	CHECK(e.GetCurrentCommandIndex() == 0);  // clamped, not left at 3
	CHECK(e.GetCurrentAktIndex()   == 0);    // selection otherwise preserved
	CHECK(e.GetCurrentSceneIndex() == 0);
}

TEST_CASE("ReloadScenes snaps off a comment row"){
	SatyrTest::TempDir tmp("reload2");
	std::filesystem::create_directories(tmp.Path() / "scenes");
	tmp.Write("scenes/S.ngk", "[Commands]\ntext \"a\"\ntext \"b\"\n");

	ProjectData p;
	p.projectPath = tmp.Str();
	p.schema.aktCount = 1;
	Akt akt;
	akt.number = 1;
	akt.sceneNames.push_back("S");
	akt.scenes.push_back(SceneParser::ParseFile(tmp.File("scenes/S.ngk")));
	p.schema.akts.push_back(std::move(akt));

	CueEngine e;
	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(1);

	// Row 1 becomes a comment; the engine must not sit on a dead row.
	tmp.Write("scenes/S.ngk", "[Commands]\ntext \"a\"\ncomment \"note\"\n");
	e.ReloadScenes();

	REQUIRE(e.GetCommandCount() == 2);
	auto* cur = e.GetCurrentCommand();
	REQUIRE(cur != nullptr);
	CHECK(cur->type != CommandType::Comment);
	CHECK(e.GetCurrentCommandIndex() == 0);
}

TEST_CASE("ReloadScenes on an emptied scene drops the command selection"){
	SatyrTest::TempDir tmp("reload3");
	std::filesystem::create_directories(tmp.Path() / "scenes");
	tmp.Write("scenes/S.ngk", "[Commands]\ntext \"a\"\n");

	ProjectData p;
	p.projectPath = tmp.Str();
	p.schema.aktCount = 1;
	Akt akt;
	akt.number = 1;
	akt.sceneNames.push_back("S");
	akt.scenes.push_back(SceneParser::ParseFile(tmp.File("scenes/S.ngk")));
	p.schema.akts.push_back(std::move(akt));

	CueEngine e;
	e.LoadProject(p);
	e.SelectAkt(0);
	e.SelectScene(0);
	e.SetCommandIndex(0);

	tmp.Write("scenes/S.ngk", "[Commands]\n");
	e.ReloadScenes();

	CHECK(e.GetCommandCount() == 0);
	CHECK(e.GetCurrentCommandIndex() == -1);
	CHECK(e.GetCurrentCommand() == nullptr);
}

TEST_CASE("ReloadScenes keeps the scene name when the file has none"){
	SatyrTest::TempDir tmp("reload4");
	std::filesystem::create_directories(tmp.Path() / "scenes");
	tmp.Write("scenes/Named.ngk", "[Commands]\nclear\n");

	ProjectData p;
	p.projectPath = tmp.Str();
	p.schema.aktCount = 1;
	Akt akt;
	akt.number = 1;
	akt.sceneNames.push_back("Named");
	akt.scenes.push_back(Scene{});
	p.schema.akts.push_back(std::move(akt));

	CueEngine e;
	e.LoadProject(p);
	e.ReloadScenes();
	auto* s = e.GetScene(0, 0);
	REQUIRE(s != nullptr);
	CHECK(s->name == "Named");
}
