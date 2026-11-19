// (2.1.0) Per-scene TextOutline and the DisableTextOutline option.
//
// Pinned here: a scene can set its own outline thickness or switch the
// outline off; DisableTextOutline wins over a thickness in the same scene;
// an explicit `DisableTextOutline = false` in a scene brings the outline
// back when the project disables it; and a scene that says nothing follows
// the project, so existing projects look exactly as before.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/CueEngine.hpp"
#include "core/OptionKeys.hpp"
#include "core/Project.hpp"
#include "core/SchemaParser.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;

TEST_CASE("DisableTextOutline is a known option name, in any case"){
	CHECK(LookupOptionKey("DisableTextOutline") == OptionKey::DisableTextOutline);
	CHECK(LookupOptionKey("disabletextoutline") == OptionKey::DisableTextOutline);
	CHECK(IsDocumentedOptionName("DISABLETEXTOUTLINE"));
}

TEST_CASE("Scene TextOutline and DisableTextOutline are read"){
	CHECK(ParseScene("[Options]\nTextOutline = 8\n").textOutline == 8);
	CHECK(ParseScene("[Options]\ntextoutline = 0\n").textOutline == 0);
	CHECK(ParseScene("[Options]\nTextOutline = -3\n").textOutline == -1);   // ignored
	CHECK(ParseScene("[Options]\nTextOutline = thick\n").textOutline == -1); // ignored
	CHECK(ParseScene("[Options]\nFontSize = 30\n").textOutline == -1);

	CHECK(ParseScene("[Options]\nDisableTextOutline = true\n").disableTextOutline == 1);
	CHECK(ParseScene("[Options]\nDisableTextOutline = false\n").disableTextOutline == 0);
	CHECK(ParseScene("[Options]\nFontSize = 30\n").disableTextOutline == -1);
}

TEST_CASE("Schema DisableTextOutline: off when missing, new projects say false"){
	TempDir tmp("outline");
	CHECK(SchemaParser::ParseFile(tmp.Write("a.toml", "[Options]\nDisableTextOutline = true\n")).disableTextOutline);
	CHECK_FALSE(SchemaParser::ParseFile(tmp.Write("b.toml", "[Options]\nTextOutline = 4\n")).disableTextOutline);
	CHECK(SchemaParser::ParseFile(tmp.Write("c.toml", "[Options]\nTextOutline = -2\n")).textOutline == 0);

	auto data = Project::Create(RevyType::Other, 2026, "Tester",
		(tmp.Path() / "Proj").string(), "Proj");
	CHECK_FALSE(data.schema.disableTextOutline);
	CHECK(data.schema.textOutline == 4);
}

TEST_CASE("Effective text outline: scene, then project"){
	auto effective = [](int sceneOutline, int sceneDisable,
	                    int projectOutline, bool projectDisable){
		ProjectData p;
		Akt akt;
		akt.number = 1;
		Scene sc;
		sc.name = "S";
		sc.textOutline = sceneOutline;
		sc.disableTextOutline = sceneDisable;
		akt.sceneNames.push_back("S");
		akt.scenes.push_back(sc);
		p.schema.akts.push_back(akt);
		p.schema.aktCount = 1;
		p.schema.textOutline = projectOutline;
		p.schema.disableTextOutline = projectDisable;
		CueEngine e;
		e.LoadProject(p);
		e.SelectAkt(0);
		e.SelectScene(0);
		return e.GetEffectiveTextOutline();
	};

	// A scene that says nothing follows the project.
	CHECK(effective(-1, -1, 4, false) == 4);
	CHECK(effective(-1, -1, 4, true)  == 0);
	// A scene thickness overrides the project's, either way.
	CHECK(effective(9, -1, 4, false) == 9);
	CHECK(effective(9, -1, 4, true)  == 9);
	CHECK(effective(0, -1, 4, false) == 0);
	// DisableTextOutline = true wins, even over a thickness in the scene.
	CHECK(effective(-1, 1, 4, false) == 0);
	CHECK(effective(9,  1, 4, false) == 0);
	// DisableTextOutline = false brings the project's thickness back.
	CHECK(effective(-1, 0, 4, true)  == 4);
	CHECK(effective(9,  0, 4, true)  == 9);

	// The project-only getter is unchanged.
	CueEngine e;
	ProjectData p;
	p.schema.textOutline = 7;
	e.LoadProject(p);
	CHECK(e.GetTextOutline() == 7);
	CHECK(e.GetEffectiveTextOutline() == 7);   // no scene selected
}
