// (1.8.1) SchemaParser — schema.toml → RevySchema.
//
// The contract that matters here is "a missing key leaves the built-in
// default alone", because that is what keeps projects authored by older
// versions loading correctly. Every option is checked both ways: present
// and honoured, absent and defaulted.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/SchemaParser.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;

static RevySchema ParseSchema(const std::string& toml){
	TempDir tmp("schema");
	return SchemaParser::ParseFile(tmp.Write("schema.toml", toml));
}

TEST_CASE("An empty schema yields every built-in default"){
	// Equivalent to a project whose schema.toml predates every option
	// added since 1.0 — nothing may shift underneath it.
	auto s = ParseSchema("");
	CHECK(s.fontSize    == 25);
	CHECK(s.textOutline == 4);
	CHECK(s.capitalize  == false);
	CHECK(s.audioFade   == false);  // (2.0.0) opt-in, so absent means off
	CHECK(s.fontPath.empty());
	CHECK(s.fontPathItalic.empty());
	CHECK(s.fontPathBold.empty());
	CHECK(s.fontPathBoldItalic.empty());
	CHECK((int)s.backgroundColor.r == 0);
	CHECK((int)s.backgroundColor.g == 0);
	CHECK((int)s.backgroundColor.b == 0);
	CHECK(s.subtitleItalic == true);
	CHECK((int)s.subtitleColour.r == 0xAA);
	CHECK(s.subtitleTransparency == doctest::Approx(1.0f));
	CHECK(s.subtitlePosY == doctest::Approx(90.0f));
	CHECK(s.aktCount == 0);
	CHECK(s.akts.empty());
	CHECK(s.targetedDisplay.width == 0);
	CHECK(s.functionKeyAudio.empty());
	CHECK(s.particleTunings.empty());
}

TEST_CASE("Malformed TOML falls back to defaults instead of throwing"){
	// A hand-edited schema with a syntax error must not take the app down
	// mid-show; SchemaParser swallows the parse error and returns defaults.
	auto s = ParseSchema("[Options\nFontSize = = 30\n");
	CHECK(s.fontSize == 25);
	CHECK(s.aktCount == 0);
}

TEST_CASE("[Options] scalar keys are read"){
	auto s = ParseSchema(R"(
[Options]
FontSize    = 40
TextOutline = 0
Capitalize  = true
AudioFade   = true
)");
	CHECK(s.fontSize    == 40);
	CHECK(s.textOutline == 0);
	CHECK(s.capitalize  == true);
	CHECK(s.audioFade   == true);
}

TEST_CASE("[Options] font faces are read independently"){
	auto s = ParseSchema(R"(
[Options]
Font           = "Regular.ttf"
FontItalic     = "Italic.ttf"
FontBold       = "Bold.ttf"
FontBoldItalic = "BoldItalic.ttf"
)");
	CHECK(s.fontPath           == "Regular.ttf");
	CHECK(s.fontPathItalic     == "Italic.ttf");
	CHECK(s.fontPathBold       == "Bold.ttf");
	CHECK(s.fontPathBoldItalic == "BoldItalic.ttf");

	SUBCASE("setting only one face leaves the others empty"){
		auto only = ParseSchema("[Options]\nFontBold = \"B.ttf\"\n");
		CHECK(only.fontPathBold == "B.ttf");
		CHECK(only.fontPath.empty());
		CHECK(only.fontPathItalic.empty());
		CHECK(only.fontPathBoldItalic.empty());
	}
}

TEST_CASE("BackgroundColour accepts each named colour"){
	struct Case{ const char* name; int r, g, b; };
	const Case cases[] = {
		{"black", 0x00, 0x00, 0x00},
		{"blue",  0x00, 0x00, 0xFF},
		{"red",   0xFF, 0x00, 0x00},
		{"green", 0x00, 0xFF, 0x00},
		{"white", 0xFF, 0xFF, 0xFF},
	};
	for(const auto& c : cases){
		CAPTURE(c.name);
		auto s = ParseSchema(std::string("[Options]\nBackgroundColour = \"")
			+ c.name + "\"\n");
		CHECK((int)s.backgroundColor.r == c.r);
		CHECK((int)s.backgroundColor.g == c.g);
		CHECK((int)s.backgroundColor.b == c.b);
	}

	SUBCASE("an unknown name leaves the default black in place"){
		auto s = ParseSchema("[Options]\nBackgroundColour = \"chartreuse\"\n");
		CHECK((int)s.backgroundColor.r == 0);
		CHECK((int)s.backgroundColor.g == 0);
		CHECK((int)s.backgroundColor.b == 0);
	}
}

TEST_CASE("Subtitle defaults for textD"){
	SUBCASE("named colours"){
		auto s = ParseSchema("[Options]\nSubtitleColour = \"white\"\n");
		CHECK((int)s.subtitleColour.r == 0xFF);
		CHECK((int)s.subtitleColour.g == 0xFF);
		CHECK((int)s.subtitleColour.b == 0xFF);
	}
	SUBCASE("grey and gray are both accepted"){
		auto grey = ParseSchema("[Options]\nSubtitleColour = \"grey\"\n");
		auto gray = ParseSchema("[Options]\nSubtitleColour = \"gray\"\n");
		CHECK((int)grey.subtitleColour.r == (int)gray.subtitleColour.r);
		CHECK((int)grey.subtitleColour.r == 0xAA);
	}
	SUBCASE("hex form"){
		auto s = ParseSchema("[Options]\nSubtitleColour = \"#FF8800\"\n");
		CHECK((int)s.subtitleColour.r == 0xFF);
		CHECK((int)s.subtitleColour.g == 0x88);
		CHECK((int)s.subtitleColour.b == 0x00);
	}
	SUBCASE("transparency is authored as a percent, stored as 0-1 opacity"){
		// 0 % transparent == fully opaque == 1.0 internally.
		CHECK(ParseSchema("[Options]\nSubtitleTransparency = 0.0\n")
			.subtitleTransparency == doctest::Approx(1.0f));
		CHECK(ParseSchema("[Options]\nSubtitleTransparency = 100.0\n")
			.subtitleTransparency == doctest::Approx(0.0f));
		CHECK(ParseSchema("[Options]\nSubtitleTransparency = 25.0\n")
			.subtitleTransparency == doctest::Approx(0.75f));
	}
	SUBCASE("out-of-range transparency is clamped, not wrapped"){
		CHECK(ParseSchema("[Options]\nSubtitleTransparency = 500.0\n")
			.subtitleTransparency == doctest::Approx(0.0f));
		CHECK(ParseSchema("[Options]\nSubtitleTransparency = -50.0\n")
			.subtitleTransparency == doctest::Approx(1.0f));
	}
	SUBCASE("italic flag and Y position"){
		auto s = ParseSchema(R"(
[Options]
SubtitleItalic = false
SubtitlePosY   = 75.0
)");
		CHECK(s.subtitleItalic == false);
		CHECK(s.subtitlePosY == doctest::Approx(75.0f));
	}
}

TEST_CASE("[RevyData] name, creator and derived year"){
	auto s = ParseSchema(R"(
[RevyData]
Revy    = "MatRevyJubi 2026"
Creator = "Nick"
)");
	CHECK(s.revyName    == "MatRevyJubi 2026");
	CHECK(s.creatorName == "Nick");
	CHECK(s.year        == 2026);

	SUBCASE("a missing Revy key gets a placeholder, not an empty string"){
		auto missing = ParseSchema("[RevyData]\nCreator = \"X\"\n");
		CHECK(missing.revyName == "Unnamed Revy");
	}
	SUBCASE("creator is optional — pre-1.6.2 projects load with it empty"){
		auto s2 = ParseSchema("[RevyData]\nRevy = \"KemiRevy 2020\"\n");
		CHECK(s2.creatorName.empty());
		CHECK(s2.year == 2020);
	}
	SUBCASE("a name with no trailing year leaves year at 0 (= unknown)"){
		// The Load Project sort treats 0 as unknown and buckets it last.
		CHECK(ParseSchema("[RevyData]\nRevy = \"MatRevy\"\n").year == 0);
		CHECK(ParseSchema("[RevyData]\nRevy = \"Revy 20x6\"\n").year == 0);
	}
	SUBCASE("only the trailing four digits are read"){
		CHECK(ParseSchema("[RevyData]\nRevy = \"Revy 1999 2026\"\n").year == 2026);
	}
}

TEST_CASE("[Structure] builds one Akt per declared akt"){
	auto s = ParseSchema(R"(
[Structure]
akter = 2

[Structure.Akt1]
schema = [ "Intro", "Scene1" ]

[Structure.Akt2]
schema = [ "Finale" ]
)");
	REQUIRE(s.aktCount == 2);
	REQUIRE(s.akts.size() == 2);

	CHECK(s.akts[0].number == 1);
	REQUIRE(s.akts[0].sceneNames.size() == 2);
	CHECK(s.akts[0].sceneNames[0] == "Intro");
	CHECK(s.akts[0].sceneNames[1] == "Scene1");

	CHECK(s.akts[1].number == 2);
	REQUIRE(s.akts[1].sceneNames.size() == 1);
	CHECK(s.akts[1].sceneNames[0] == "Finale");

	// The parser only fills sceneNames; the Scene objects themselves are
	// loaded by Project::Load, which is covered in test_project.cpp.
	CHECK(s.akts[0].scenes.empty());

	SUBCASE("an akt declared but with no schema table yields an empty akt"){
		auto s2 = ParseSchema("[Structure]\nakter = 2\n[Structure.Akt1]\nschema = [\"A\"]\n");
		REQUIRE(s2.akts.size() == 2);
		CHECK(s2.akts[1].sceneNames.empty());
	}
	SUBCASE("akter = 0 with tables present produces no akts"){
		// The loop is driven by `akter`, not by which tables exist, so a
		// mistyped count silently hides scenes — worth pinning down.
		auto s2 = ParseSchema("[Structure]\nakter = 0\n[Structure.Akt1]\nschema = [\"A\"]\n");
		CHECK(s2.akts.empty());
	}
}

TEST_CASE("[Display] rect is read verbatim"){
	auto s = ParseSchema(R"(
[Display]
Width    = 1920
Height   = 1080
CenterX  = 960
CenterY  = 540
Rotation = 12.5
)");
	CHECK(s.targetedDisplay.width    == 1920);
	CHECK(s.targetedDisplay.height   == 1080);
	CHECK(s.targetedDisplay.centerX  == 960);
	CHECK(s.targetedDisplay.centerY  == 540);
	CHECK(s.targetedDisplay.rotation == doctest::Approx(12.5f));

	SUBCASE("a missing table leaves the fill-the-display sentinel"){
		auto s2 = ParseSchema("[Options]\nFontSize = 30\n");
		CHECK(s2.targetedDisplay.width  == 0);
		CHECK(s2.targetedDisplay.height == 0);
	}
}

TEST_CASE("[Hotkeys] binds F1-F12 and drops everything else"){
	auto s = ParseSchema(R"(
[Hotkeys]
F1  = "knock.mp3"
F7  = "applause.wav"
F12 = "gunshot.mp3"
)");
	CHECK(s.functionKeyAudio.size() == 3);
	CHECK(s.functionKeyAudio.at(1)  == "knock.mp3");
	CHECK(s.functionKeyAudio.at(7)  == "applause.wav");
	CHECK(s.functionKeyAudio.at(12) == "gunshot.mp3");

	SUBCASE("empty values are dropped rather than bound to an empty path"){
		// This is what lets the 1.8.0 generated schema list all twelve
		// slots as `F# = ""` without binding anything.
		auto s2 = ParseSchema(R"(
[Hotkeys]
F1  = ""
F2  = ""
F3  = "real.mp3"
)");
		CHECK(s2.functionKeyAudio.size() == 1);
		CHECK(s2.functionKeyAudio.count(1) == 0);
		CHECK(s2.functionKeyAudio.at(3) == "real.mp3");
	}
	SUBCASE("keys outside F1-F12 are ignored"){
		auto s2 = ParseSchema("[Hotkeys]\nF13 = \"x.mp3\"\nF0 = \"y.mp3\"\n");
		CHECK(s2.functionKeyAudio.empty());
	}
}

TEST_CASE("[Particle.<TYPE>] tuning is per-field, with have-flags"){
	auto s = ParseSchema(R"(
[Particle.RAIN]
Speed   = 1.5
Density = 2.0

[Particle.SNOW]
XDist   = "NORMAL"
XDistP1 = 0.5
XDistP2 = 0.1
)");
	REQUIRE(s.particleTunings.count((int)ParticleType::Rain) == 1);
	const auto& rain = s.particleTunings.at((int)ParticleType::Rain);
	CHECK(rain.haveSpeed);
	CHECK(rain.speed == doctest::Approx(1.5f));
	CHECK(rain.haveDensity);
	CHECK(rain.density == doctest::Approx(2.0f));
	// Untouched fields must stay unflagged so the cascade can fall through.
	CHECK_FALSE(rain.haveXDist);
	CHECK_FALSE(rain.haveVxDist);
	CHECK_FALSE(rain.haveVyDist);

	REQUIRE(s.particleTunings.count((int)ParticleType::Snow) == 1);
	const auto& snow = s.particleTunings.at((int)ParticleType::Snow);
	CHECK(snow.haveXDist);
	CHECK(snow.xDist.type == Distribution::Normal);
	CHECK(snow.xDist.p1 == doctest::Approx(0.5f));
	CHECK(snow.xDist.p2 == doctest::Approx(0.1f));
	CHECK_FALSE(snow.haveSpeed);

	SUBCASE("velocity distributions"){
		auto s2 = ParseSchema(R"(
[Particle.CONFETTI]
VXDist   = "GAMMA"
VXDistP1 = 2.0
VXDistP2 = 0.3
VYDist   = "BERNOULLI"
VYDistP1 = 0.7
)");
		const auto& c = s2.particleTunings.at((int)ParticleType::Confetti);
		CHECK(c.haveVxDist);
		CHECK(c.vxDist.type == Distribution::Gamma);
		CHECK(c.vxDist.p1 == doctest::Approx(2.0f));
		CHECK(c.haveVyDist);
		CHECK(c.vyDist.type == Distribution::Bernoulli);
		CHECK(c.vyDist.p1 == doctest::Approx(0.7f));
	}
	SUBCASE("distribution names are case-insensitive"){
		auto s2 = ParseSchema("[Particle.RAIN]\nXDist = \"lognormal\"\n");
		CHECK(s2.particleTunings.at((int)ParticleType::Rain).xDist.type
			== Distribution::LogNormal);
	}
	SUBCASE("an unrecognised distribution name is ignored entirely"){
		auto s2 = ParseSchema("[Particle.RAIN]\nXDist = \"poisson\"\nXDistP1 = 9.0\n");
		// No recognised field was set, so no tuning entry is stored at all
		// and the cascade keeps falling through to the built-in default.
		CHECK(s2.particleTunings.count((int)ParticleType::Rain) == 0);
	}
	SUBCASE("an unknown particle type is ignored"){
		auto s2 = ParseSchema("[Particle.SPARKLE]\nSpeed = 2.0\n");
		CHECK(s2.particleTunings.empty());
	}
	SUBCASE("every documented type name is recognised"){
		const char* names[] = {"RAIN","SNOW","CONFETTI","RISE","SCATTER",
			"BROWNIAN","OSCILLATION","ORBIT","NOISE","LIFECURVE"};
		for(const char* n : names){
			CAPTURE(n);
			auto s2 = ParseSchema(std::string("[Particle.") + n + "]\nSpeed = 2.0\n");
			CHECK(s2.particleTunings.size() == 1);
		}
	}
}

TEST_CASE("A fully populated schema reads every section together"){
	// Guards against one section's parsing clobbering another's.
	auto s = ParseSchema(R"(
[RevyData]
Revy    = "FysikRevy 2027"
Creator = "Ada"

[Options]
FontSize         = 32
BackgroundColour = "blue"
Capitalize       = true

[Structure]
akter = 1

[Structure.Akt1]
schema = [ "Only" ]

[Display]
Width  = 800
Height = 600

[Hotkeys]
F5 = "cue.mp3"

[Particle.ORBIT]
Speed = 0.5
)");
	CHECK(s.revyName == "FysikRevy 2027");
	CHECK(s.year == 2027);
	CHECK(s.creatorName == "Ada");
	CHECK(s.fontSize == 32);
	CHECK((int)s.backgroundColor.b == 0xFF);
	CHECK(s.capitalize);
	REQUIRE(s.akts.size() == 1);
	CHECK(s.akts[0].sceneNames[0] == "Only");
	CHECK(s.targetedDisplay.width == 800);
	CHECK(s.functionKeyAudio.at(5) == "cue.mp3");
	CHECK(s.particleTunings.at((int)ParticleType::Orbit).speed
		== doctest::Approx(0.5f));
}

TEST_CASE("A nonexistent schema path yields defaults, not a crash"){
	auto s = SchemaParser::ParseFile("does/not/exist/schema.toml");
	CHECK(s.fontSize == 25);
	CHECK(s.akts.empty());
}
