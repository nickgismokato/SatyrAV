// (2.1.0) TransparentBackground: a see-through slave.
//
// Pinned here: the option is read in a scene (either way) and in
// schema.toml (off when missing, `false` in new projects); a scene's value
// wins over the project's; and UsesTransparentBackground — which decides
// whether the slave window is created see-through — is true exactly when
// the project or at least one scene turns it on.
//
// The drawing side (alpha-0 clears, premultiplied composite) needs SDL and
// a window, so it is outside this suite.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/CueEngine.hpp"
#include "core/OptionKeys.hpp"
#include "core/Project.hpp"
#include "core/SchemaParser.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;

TEST_CASE("TransparentBackground: names, scene and schema"){
	CHECK(LookupOptionKey("TransparentBackground") == OptionKey::TransparentBackground);
	CHECK(LookupOptionKey("seethrough") == OptionKey::TransparentBackground);

	CHECK(ParseScene("[Options]\nTransparentBackground = true\n").transparentBackground == 1);
	CHECK(ParseScene("[Options]\ntransparentbackground = false\n").transparentBackground == 0);
	CHECK(ParseScene("[Options]\nFontSize = 30\n").transparentBackground == -1);

	TempDir tmp("transparent");
	CHECK(SchemaParser::ParseFile(tmp.Write("a.toml", "[Options]\nTransparentBackground = true\n")).transparentBackground);
	CHECK_FALSE(SchemaParser::ParseFile(tmp.Write("b.toml", "[Options]\nFontSize = 30\n")).transparentBackground);

	auto data = Project::Create(RevyType::Other, 2026, "Tester",
		(tmp.Path() / "Proj").string(), "Proj");
	CHECK_FALSE(data.schema.transparentBackground);
}

static ProjectData Make(std::vector<int> sceneVals, bool projectVal){
	ProjectData p;
	Akt akt;
	akt.number = 1;
	for(size_t i = 0; i < sceneVals.size(); i++){
		Scene sc;
		sc.name = "S" + std::to_string(i);
		sc.transparentBackground = sceneVals[i];
		akt.sceneNames.push_back(sc.name);
		akt.scenes.push_back(sc);
	}
	p.schema.akts.push_back(akt);
	p.schema.aktCount = 1;
	p.schema.transparentBackground = projectVal;
	return p;
}

TEST_CASE("Effective TransparentBackground: scene either way, else the project"){
	auto effective = [](int sceneVal, bool projectVal){
		CueEngine e;
		e.LoadProject(Make({sceneVal}, projectVal));
		e.SelectAkt(0);
		e.SelectScene(0);
		return e.GetEffectiveTransparentBackground();
	};
	CHECK_FALSE(effective(-1, false));
	CHECK(effective(-1, true));
	CHECK(effective(1, false));
	CHECK_FALSE(effective(0, true));
}

TEST_CASE("The window is see-through only when something asks for it"){
	CHECK_FALSE(CueEngine::UsesTransparentBackground(Make({-1, 0, -1}, false)));
	CHECK(CueEngine::UsesTransparentBackground(Make({-1, 1, -1}, false)));
	CHECK(CueEngine::UsesTransparentBackground(Make({-1, -1}, true)));
	// Every scene switches it off again: still asked for at project level,
	// so the window is opened see-through (harmless; scenes paint opaque).
	CHECK(CueEngine::UsesTransparentBackground(Make({0, 0}, true)));
}
