// (2.0.1) One name per option, one colour reader.
//
// Two promises are pinned here. First, the new rules: an option's documented
// name works in both a scene and schema.toml in any case, and every colour
// option understands the same colours. Second — the one that protects shows
// already built — every spelling and colour that worked before 2.0.1 still
// means exactly what it meant.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/OptionKeys.hpp"
#include "core/SchemaParser.hpp"
#include "core/CueEngine.hpp"
#include "core/Types.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;
using SatyrTest::ParseOneCommand;

static RevySchema ParseSchemaText(const std::string& toml){
	TempDir tmp("optnames");
	return SchemaParser::ParseFile(tmp.Write("schema.toml", toml));
}

static void CheckRGB(const Colour& c, int r, int g, int b){
	CHECK((int)c.r == r);
	CHECK((int)c.g == g);
	CHECK((int)c.b == b);
}

// ───────────────────────── the colour reader ─────────────────────────

TEST_CASE("ParseColour knows every named colour, in any case"){
	struct Case{ const char* name; uint32_t rgb; };
	const Case cases[] = {
		{"red", 0xFF0000}, {"GREEN", 0x00FF00}, {"Blue", 0x0000FF},
		{"white", 0xFFFFFF}, {"black", 0x000000}, {"yellow", 0xFFFF00},
		{"orange", 0xFFA500}, {"purple", 0x800080}, {"pink", 0xFF69B4},
		{"cyan", 0x00FFFF}, {"lime", 0x32CD32}, {"grey", 0x808080},
		{"GRAY", 0x808080},
	};
	for(const auto& c : cases){
		CAPTURE(c.name);
		Colour out{};
		REQUIRE(ParseColour(c.name, out));
		CheckRGB(out, (c.rgb >> 16) & 0xFF, (c.rgb >> 8) & 0xFF, c.rgb & 0xFF);
		CHECK((int)out.a == 255);
	}
}

TEST_CASE("ParseColour reads #RRGGBB and the short #RGB"){
	Colour c{};
	REQUIRE(ParseColour("#FF5500", c)); CheckRGB(c, 0xFF, 0x55, 0x00);
	REQUIRE(ParseColour("#ff5500", c)); CheckRGB(c, 0xFF, 0x55, 0x00);
	REQUIRE(ParseColour("  #123456  ", c)); CheckRGB(c, 0x12, 0x34, 0x56);
	SUBCASE("#RGB doubles each digit"){
		REQUIRE(ParseColour("#F80", c)); CheckRGB(c, 0xFF, 0x88, 0x00);
		REQUIRE(ParseColour("#abc", c)); CheckRGB(c, 0xAA, 0xBB, 0xCC);
	}
}

TEST_CASE("ParseColour rejects anything else and leaves the colour alone"){
	const char* bad[] = {"", "mauve", "#12", "#12345", "#1234567", "#GG0000", "FF0000", "# F80"};
	for(const char* b : bad){
		CAPTURE(b);
		Colour c = {1, 2, 3, 4};
		CHECK_FALSE(ParseColour(b, c));
		CheckRGB(c, 1, 2, 3);
	}
}

// ───────────────────────── the name table ─────────────────────────

TEST_CASE("Option names: documented name in any case, plus the old spellings"){
	CHECK(LookupOptionKey("BackgroundColour") == OptionKey::BackgroundColour);
	CHECK(LookupOptionKey("backgroundcolour") == OptionKey::BackgroundColour);
	CHECK(LookupOptionKey("BackgroundColor")  == OptionKey::BackgroundColour);
	CHECK(LookupOptionKey("bc")               == OptionKey::BackgroundColour);
	CHECK(LookupOptionKey("BC")               == OptionKey::BackgroundColour);
	CHECK(LookupOptionKey("font")             == OptionKey::Font);
	CHECK(LookupOptionKey("FONT")             == OptionKey::Font);
	CHECK(LookupOptionKey("fontSize")         == OptionKey::FontSize);
	CHECK(LookupOptionKey("cap")              == OptionKey::Capitalize);
	CHECK(LookupOptionKey("textColor")        == OptionKey::TextColour);
	CHECK(LookupOptionKey("SubtitleColor")    == OptionKey::SubtitleColour);
	CHECK(LookupOptionKey("RAIN.SPEED")       == OptionKey::Unknown);
	CHECK(LookupOptionKey("fontsizes")        == OptionKey::Unknown);

	CHECK(IsDocumentedOptionName("backgroundcolour"));
	CHECK_FALSE(IsDocumentedOptionName("bc"));
	CHECK_FALSE(IsDocumentedOptionName("BackgroundColor"));
	CHECK(std::string(DocumentedOptionName(OptionKey::Capitalize)) == "Capitalize");

	CHECK(ParseOptionBool("TRUE"));
	CHECK(ParseOptionBool(" 1 "));
	CHECK_FALSE(ParseOptionBool("yes"));
	CHECK_FALSE(ParseOptionBool("false"));
}

// ───────────────────────── scene [Options] ─────────────────────────

TEST_CASE("Scene options accept the documented names, in any case"){
	auto s = ParseScene(
		"[Options]\n"
		"BackgroundColour = blue\n"
		"FONTSIZE = 42\n"
		"Font = Inter.ttf\n"
		"FontBold = Inter-Bold.ttf\n"
		"Capitalize = TRUE\n"
		"audiofade = true\n");
	CheckRGB(s.backgroundColor, 0x00, 0x00, 0xFF);
	CHECK(s.fontSize == 42);
	CHECK(s.fontPath == "Inter.ttf");
	CHECK(s.fontPathBold == "Inter-Bold.ttf");
	CHECK(s.capitalize);
	CHECK(s.audioFade);
}

TEST_CASE("Scene options: every pre-2.0.1 spelling still works the same"){
	auto s = ParseScene(
		"[Options]\n"
		"bc = red\n"
		"FontSize = 30\n"
		"font = a.ttf\n"
		"fontItalic = b.ttf\n"
		"fontBoldItalic = c.ttf\n"
		"cap = 1\n"
		"audioFade = true\n");
	CheckRGB(s.backgroundColor, 0xFF, 0x00, 0x00);
	CHECK(s.fontSize == 30);
	CHECK(s.fontPath == "a.ttf");
	CHECK(s.fontPathItalic == "b.ttf");
	CHECK(s.fontPathBoldItalic == "c.ttf");
	CHECK(s.capitalize);
	CHECK(s.audioFade);
}

TEST_CASE("Scene background colour now takes any colour, including hex"){
	CheckRGB(ParseScene("[Options]\nbc = orange\n").backgroundColor, 0xFF, 0xA5, 0x00);
	CheckRGB(ParseScene("[Options]\nbc = #102030\n").backgroundColor, 0x10, 0x20, 0x30);
	CheckRGB(ParseScene("[Options]\nbc = #FFF\n").backgroundColor, 0xFF, 0xFF, 0xFF);
	SUBCASE("a hex value is not mistaken for a comment"){
		CheckRGB(ParseScene("[Options]\nbc = #102030   # warm blue\n").backgroundColor, 0x10, 0x20, 0x30);
		CheckRGB(ParseScene("[Options]\nbc = \"#102030\"\n").backgroundColor, 0x10, 0x20, 0x30);
		CheckRGB(ParseScene("[Options]\nbc = blue  # was #FF0000\n").backgroundColor, 0x00, 0x00, 0xFF);
		// Five digits is not a colour, so it stays a comment, as before 2.0.1.
		CheckRGB(ParseScene("[Options]\nbc = #12345\n").backgroundColor, 0, 0, 0);
		// A whole-line comment is still a comment.
		CheckRGB(ParseScene("[Options]\n# bc = #FFFFFF\n").backgroundColor, 0, 0, 0);
	}
	SUBCASE("quoted values: font paths with spaces"){
		CHECK(ParseScene("[Options]\nfont = \"My Font.ttf\"\n").fontPath == "My Font.ttf");
	}
	SUBCASE("an unrecognised colour still leaves the default black"){
		CheckRGB(ParseScene("[Options]\nbc = mauve\n").backgroundColor, 0, 0, 0);
	}
}

TEST_CASE("Particle tunables are untouched by the option-name table"){
	auto s = ParseScene("[Options]\nRAIN.SPEED = 1.5\n");
	const auto& t = s.particleTunings[(int)ParticleType::Rain];
	CHECK(t.haveSpeed);
	CHECK(t.speed == doctest::Approx(1.5f));
}

// ───────────────────────── schema.toml [Options] ─────────────────────────

TEST_CASE("Schema options accept any case and the scene spellings"){
	auto s = ParseSchemaText(R"(
[Options]
fontsize   = 40
font       = "Inter.ttf"
cap        = true
bc         = "#336699"
audiofade  = true
)");
	CHECK(s.fontSize == 40);
	CHECK(s.fontPath == "Inter.ttf");
	CHECK(s.capitalize);
	CheckRGB(s.backgroundColor, 0x33, 0x66, 0x99);
	CHECK(s.audioFade);
}

TEST_CASE("Schema: the documented name wins over an alias, whatever the order"){
	auto a = ParseSchemaText("[Options]\nbc = \"red\"\nBackgroundColour = \"blue\"\n");
	CheckRGB(a.backgroundColor, 0x00, 0x00, 0xFF);
	auto b = ParseSchemaText("[Options]\nBackgroundColour = \"blue\"\nbc = \"red\"\n");
	CheckRGB(b.backgroundColor, 0x00, 0x00, 0xFF);
}

TEST_CASE("Schema background colour takes any colour; unknown keeps the default"){
	CheckRGB(ParseSchemaText("[Options]\nBackgroundColour = \"purple\"\n").backgroundColor, 0x80, 0x00, 0x80);
	const RevySchema builtin;
	auto s = ParseSchemaText("[Options]\nBackgroundColour = \"mauve\"\n");
	CheckRGB(s.backgroundColor, builtin.backgroundColor.r, builtin.backgroundColor.g, builtin.backgroundColor.b);
}

TEST_CASE("SubtitleColour: grey keeps its old lighter shade, everything else is shared"){
	// The one deliberate exception: shows built before 2.0.1 must not get
	// darker subtitles.
	CheckRGB(ParseSchemaText("[Options]\nSubtitleColour = \"grey\"\n").subtitleColour, 0xAA, 0xAA, 0xAA);
	CheckRGB(ParseSchemaText("[Options]\nSubtitleColour = \"GRAY\"\n").subtitleColour, 0xAA, 0xAA, 0xAA);
	CheckRGB(ParseSchemaText("[Options]\nSubtitleColour = \"yellow\"\n").subtitleColour, 0xFF, 0xFF, 0x00);
	CheckRGB(ParseSchemaText("[Options]\nSubtitleColour = \"#0F0\"\n").subtitleColour, 0x00, 0xFF, 0x00);
	CheckRGB(ParseSchemaText("[Options]\nsubtitlecolor = \"white\"\n").subtitleColour, 0xFF, 0xFF, 0xFF);
}

// ───────────────────────── colour(...) / color(...) ─────────────────────────

TEST_CASE("colour(...) is the documented modifier; color(...) still works"){
	auto uk = ParseOneCommand("text colour(RED, \"x\")");
	REQUIRE(uk.runs.size() == 1);
	CHECK(uk.runs[0].text == "x");
	CheckRGB(uk.runs[0].colour, 0xFF, 0x00, 0x00);

	auto us = ParseOneCommand("text color(RED, \"x\")");
	REQUIRE(us.runs.size() == 1);
	CheckRGB(us.runs[0].colour, 0xFF, 0x00, 0x00);

	auto mixed = ParseOneCommand("text colour(#00F, \"a\") + color(lime, \"b\")");
	REQUIRE(mixed.runs.size() == 2);
	CheckRGB(mixed.runs[0].colour, 0x00, 0x00, 0xFF);
	CheckRGB(mixed.runs[1].colour, 0x32, 0xCD, 0x32);

	SUBCASE("an unknown colour has always meant white"){
		auto unk = ParseOneCommand("text colour(mauve, \"x\")");
		CheckRGB(unk.runs[0].colour, 0xFF, 0xFF, 0xFF);
	}
	SUBCASE("no unknown-keyword warning for either spelling"){
		auto s = ParseScene("[Commands]\ntext colour(RED, \"x\")\ntext color(RED, \"y\")\n");
		CHECK(s.warnings.empty());
	}
}

// ---------------------------------------------------------------------------
// (2.0.1) TextColour
// ---------------------------------------------------------------------------

TEST_CASE("Scene TextColour: names and hex; unset or unknown stays unset"){
	auto named = ParseScene("[Options]\ntextColour = yellow\n").textColour;
	CheckRGB(named, 0xFF, 0xFF, 0x00);
	CHECK((int)named.a == 0xFF);
	CheckRGB(ParseScene("[Options]\nTextColour = #FF8800\n").textColour, 0xFF, 0x88, 0x00);
	CheckRGB(ParseScene("[Options]\nTEXTCOLOUR = #F80   # short form\n").textColour, 0xFF, 0x88, 0x00);
	CheckRGB(ParseScene("[Options]\ntextColor = \"#102030\"\n").textColour, 0x10, 0x20, 0x30);
	CHECK((int)ParseScene("[Options]\nbc = red\n").textColour.a == 0);
	CHECK((int)ParseScene("[Options]\ntextColour = mauve\n").textColour.a == 0);
}

TEST_CASE("Schema TextColour: names and hex; unknown stays unset"){
	CheckRGB(ParseSchemaText("[Options]\nTextColour = \"#00FF80\"\n").textColour, 0x00, 0xFF, 0x80);
	CheckRGB(ParseSchemaText("[Options]\ntextcolor = \"cyan\"\n").textColour, 0x00, 0xFF, 0xFF);
	CHECK((int)ParseSchemaText("[Options]\nTextColour = \"mauve\"\n").textColour.a == 0);
	CHECK((int)ParseSchemaText("[Options]\nFontSize = 30\n").textColour.a == 0);
}

TEST_CASE("Effective text colour: scene, then project, then white"){
	auto make = [](Colour sceneCol, Colour projectCol){
		ProjectData p;
		Akt akt;
		akt.number = 1;
		akt.sceneNames.push_back("S");
		Scene sc;
		sc.name = "S";
		sc.textColour = sceneCol;
		akt.scenes.push_back(std::move(sc));
		p.schema.akts.push_back(std::move(akt));
		p.schema.aktCount = 1;
		p.schema.textColour = projectCol;
		return p;
	};
	const Colour unset = {0, 0, 0, 0};
	const Colour red   = {0xFF, 0, 0, 0xFF};
	const Colour blue  = {0, 0, 0xFF, 0xFF};

	CueEngine e;
	e.LoadProject(make(unset, unset));
	CheckRGB(e.GetEffectiveTextColour(), 0xFF, 0xFF, 0xFF);

	CueEngine e2;
	e2.LoadProject(make(unset, blue));
	CheckRGB(e2.GetEffectiveTextColour(), 0, 0, 0xFF);

	CueEngine e3;
	e3.LoadProject(make(red, blue));
	CheckRGB(e3.GetEffectiveTextColour(), 0, 0, 0xFF); // no scene entered yet
	e3.SelectAkt(0);
	e3.SelectScene(0);
	CheckRGB(e3.GetEffectiveTextColour(), 0xFF, 0, 0);
}

TEST_CASE("(2.0.1) A scene without BackgroundColour leaves it unset"){
	CHECK((int)ParseScene("[Options]\nFontSize = 30\n").backgroundColor.a == 0);
	CHECK((int)ParseScene("[Options]\nbc = mauve\n").backgroundColor.a == 0);
	CHECK((int)ParseScene("[Options]\nbc = black\n").backgroundColor.a == 0xFF);
}

