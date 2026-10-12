// (1.8.1) SceneParser — the .ngk scene language. This is the largest test
// surface in the core: it is the thing show operators actually author, and
// every regression here breaks a live performance rather than a dev build.
//
// Deliberately not covered:
//   - SceneParser::ParseLoop is a private stub that returns {} and is never
//     called; loop expansion really happens in ExpandLoop. Left untested on
//     purpose — see the note on the declaration in SceneParser.hpp.
//   - Exact `Command::argument` text for commands whose argument is only
//     used for debug/log display. Where `argument` is load-bearing (Show,
//     Play, Clear group names) it is asserted.

#include <doctest/doctest.h>
#include "helpers.hpp"

using namespace SatyrAV;
using SatyrTest::ParseScene;
using SatyrTest::ParseOneCommand;
using SatyrTest::TempDir;

// ── Sections and file handling ───────────────────────────────

TEST_CASE("Scene name and filename come from the path"){
	TempDir tmp("named");
	auto path = tmp.Write("Finale.ngk", "[Commands]\nclear\n");
	auto scene = SceneParser::ParseFile(path);
	CHECK(scene.name     == "Finale");
	CHECK(scene.fileName == "Finale.ngk");
}

TEST_CASE("A nonexistent scene file yields an empty scene, not a crash"){
	auto scene = SceneParser::ParseFile("does/not/exist/Nope.ngk");
	CHECK(scene.commands.empty());
	CHECK(scene.warnings.empty());
}

TEST_CASE("Lines outside any section are ignored"){
	// Everything before the first [Section] header is preamble/comments.
	auto scene = ParseScene(
		"this is not a command\n"
		"neither is this\n"
		"[Commands]\n"
		"clear\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Clear);
}

TEST_CASE("Blank lines and full-line comments produce no commands"){
	auto scene = ParseScene(
		"[Commands]\n"
		"\n"
		"# a comment line\n"
		"   \n"
		"clear\n"
		"\t# indented comment\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Clear);
}

TEST_CASE("A trailing comment is captured and stripped from the command"){
	auto scene = ParseScene("[Commands]\nclear   #wipe the stage\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Clear);
	CHECK(scene.commands[0].comment == "wipe the stage");
	CHECK(scene.commands[0].argument.empty());
}

TEST_CASE("Line numbers are 1-based and survive blank lines"){
	auto scene = ParseScene(
		"[Commands]\n"   // line 1
		"\n"             // line 2
		"clear\n"        // line 3
		"clearAll\n");   // line 4
	REQUIRE(scene.commands.size() == 2);
	CHECK(scene.commands[0].lineNumber == 3);
	CHECK(scene.commands[1].lineNumber == 4);
}

TEST_CASE("[Preload] collects one path per line"){
	auto scene = ParseScene(
		"[Preload]\n"
		"pictures/logo.png\n"
		"sound/intro.mp3\n"
		"\n"
		"[Commands]\n"
		"clear\n");
	REQUIRE(scene.preloadPaths.size() == 2);
	CHECK(scene.preloadPaths[0] == "pictures/logo.png");
	CHECK(scene.preloadPaths[1] == "sound/intro.mp3");
}

TEST_CASE("Sections can appear in any order and repeat"){
	auto scene = ParseScene(
		"[Commands]\n"
		"clear\n"
		"[Options]\n"
		"FontSize = 40\n"
		"[Commands]\n"
		"clearAll\n");
	CHECK(scene.fontSize == 40);
	REQUIRE(scene.commands.size() == 2);
	CHECK(scene.commands[0].type == CommandType::Clear);
	CHECK(scene.commands[1].type == CommandType::ClearAll);
}

// ── [Options] ────────────────────────────────────────────────

TEST_CASE("[Options] scene overrides"){
	auto scene = ParseScene(R"(
[Options]
bc = blue
FontSize = 30
font = Regular.ttf
fontItalic = Italic.ttf
fontBold = Bold.ttf
fontBoldItalic = BoldItalic.ttf
cap = true

[Commands]
clear
)");
	CHECK((int)scene.backgroundColor.b == 0xFF);
	CHECK(scene.fontSize == 30);
	CHECK(scene.fontPath           == "Regular.ttf");
	CHECK(scene.fontPathItalic     == "Italic.ttf");
	CHECK(scene.fontPathBold       == "Bold.ttf");
	CHECK(scene.fontPathBoldItalic == "BoldItalic.ttf");
	CHECK(scene.capitalize == true);
}

TEST_CASE("[Options] defaults when nothing is set"){
	// fontSize 0 is the "inherit from project" sentinel — not 25.
	auto scene = ParseScene("[Commands]\nclear\n");
	CHECK(scene.fontSize == 0);
	CHECK(scene.fontPath.empty());
	CHECK(scene.capitalize == false);
	CHECK((int)scene.backgroundColor.r == 0); // black
}

TEST_CASE("[Options] bc accepts each named colour, ignores unknown"){
	CHECK((int)ParseScene("[Options]\nbc = red\n").backgroundColor.r   == 0xFF);
	CHECK((int)ParseScene("[Options]\nbc = green\n").backgroundColor.g == 0xFF);
	CHECK((int)ParseScene("[Options]\nbc = white\n").backgroundColor.b == 0xFF);
	// Unknown leaves the default black.
	auto unknown = ParseScene("[Options]\nbc = puce\n");
	CHECK((int)unknown.backgroundColor.r == 0);
	CHECK((int)unknown.backgroundColor.g == 0);
	CHECK((int)unknown.backgroundColor.b == 0);
}

TEST_CASE("[Options] cap accepts true and 1"){
	CHECK(ParseScene("[Options]\ncap = true\n").capitalize);
	CHECK(ParseScene("[Options]\ncap = 1\n").capitalize);
	CHECK_FALSE(ParseScene("[Options]\ncap = false\n").capitalize);
	CHECK_FALSE(ParseScene("[Options]\ncap = yes\n").capitalize);

	// (2.0.0) audioFade accepts the same truthy spellings, and defaults off
	// when the key is absent entirely.
	CHECK(ParseScene("[Options]\naudioFade = true\n").audioFade);
	CHECK(ParseScene("[Options]\naudioFade = 1\n").audioFade);
	CHECK_FALSE(ParseScene("[Options]\naudioFade = false\n").audioFade);
	CHECK_FALSE(ParseScene("[Options]\naudioFade = yes\n").audioFade);
	CHECK_FALSE(ParseScene("[Options]\ncap = true\n").audioFade);
}

TEST_CASE("[Options] particle tunables are per-field with have-flags"){
	auto scene = ParseScene(R"(
[Options]
RAIN.SPEED   = 1.5
RAIN.DENS    = 2.0
SNOW.X.DIST  = NORMAL(0.5, 0.1)
SNOW.VY.DIST = GAMMA(2.0, 0.3)

[Commands]
clear
)");
	const auto& rain = scene.particleTunings.at((int)ParticleType::Rain);
	CHECK(rain.haveSpeed);
	CHECK(rain.speed == doctest::Approx(1.5f));
	CHECK(rain.haveDensity);
	CHECK(rain.density == doctest::Approx(2.0f));
	CHECK_FALSE(rain.haveXDist);

	const auto& snow = scene.particleTunings.at((int)ParticleType::Snow);
	CHECK(snow.haveXDist);
	CHECK(snow.xDist.type == Distribution::Normal);
	CHECK(snow.xDist.p1 == doctest::Approx(0.5f));
	CHECK(snow.haveVyDist);
	CHECK(snow.vyDist.type == Distribution::Gamma);
	CHECK_FALSE(snow.haveVxDist);

	SUBCASE("type names are case-insensitive"){
		auto s = ParseScene("[Options]\nrain.SPEED = 2.0\n");
		CHECK(s.particleTunings.at((int)ParticleType::Rain).speed
			== doctest::Approx(2.0f));
	}
	SUBCASE("DENSITY is accepted as a synonym for DENS"){
		auto s = ParseScene("[Options]\nRAIN.DENSITY = 3.0\n");
		CHECK(s.particleTunings.at((int)ParticleType::Rain).density
			== doctest::Approx(3.0f));
	}
	SUBCASE("an unknown type produces no tuning entry"){
		CHECK(ParseScene("[Options]\nSPARKLE.SPEED = 2.0\n")
			.particleTunings.empty());
	}
}

// ── Commands: the keyword table ──────────────────────────────

TEST_CASE("Every command keyword maps to its CommandType"){
	CHECK(ParseOneCommand("text \"a\"").type          == CommandType::Text);
	CHECK(ParseOneCommand("textCont \"a\"").type      == CommandType::TextCont);
	CHECK(ParseOneCommand("textbf \"a\"").type        == CommandType::TextBf);
	CHECK(ParseOneCommand("textit \"a\"").type        == CommandType::TextIt);
	CHECK(ParseOneCommand("textbfCont \"a\"").type    == CommandType::TextBfCont);
	CHECK(ParseOneCommand("textitCont \"a\"").type    == CommandType::TextItCont);
	CHECK(ParseOneCommand("textD \"a\",\"b\"").type   == CommandType::TextD);
	CHECK(ParseOneCommand("clear").type               == CommandType::Clear);
	CHECK(ParseOneCommand("clearText").type           == CommandType::ClearText);
	CHECK(ParseOneCommand("clearImages").type         == CommandType::ClearImages);
	CHECK(ParseOneCommand("clearAll").type            == CommandType::ClearAll);
	CHECK(ParseOneCommand("clearCountdown").type      == CommandType::ClearCountdown);
	CHECK(ParseOneCommand("play a.mp3").type          == CommandType::Play);
	CHECK(ParseOneCommand("stop").type                == CommandType::Stop);
	CHECK(ParseOneCommand("stopParticleCont").type    == CommandType::StopParticleCont);
	CHECK(ParseOneCommand("show a.png").type          == CommandType::Show);
	CHECK(ParseOneCommand("comment \"a\"").type       == CommandType::Comment);
	CHECK(ParseOneCommand("countdown(30)").type       == CommandType::Countdown);
}

TEST_CASE("Keywords are case-sensitive"){
	// `Clear` is not `clear` — it falls through to Text and is flagged.
	auto cmd = ParseOneCommand("Clear");
	CHECK(cmd.type == CommandType::Text);
	CHECK(cmd.unknownKeyword == "Clear");
}

TEST_CASE("An unknown bareword parses as text and raises a warning"){
	auto scene = ParseScene("[Commands]\ntxet \"Hello\"\n");
	REQUIRE(scene.commands.size() == 1);
	// Behaviour fallback: the cue still runs rather than killing the scene.
	CHECK(scene.commands[0].type == CommandType::Text);
	CHECK(scene.commands[0].unknownKeyword == "txet");
	REQUIRE(scene.warnings.size() == 1);
	CHECK(scene.warnings[0].lineNumber == 2);
	CHECK(scene.warnings[0].message.find("txet") != std::string::npos);
}

TEST_CASE("Genuine text is not mistaken for a typo"){
	// A line that starts with a quote, `+`, or the acute delimiter is text
	// the author meant, so no warning is raised.
	CHECK(ParseOneCommand("\"just text\"").unknownKeyword.empty());
	CHECK(ParseOneCommand("'just text'").unknownKeyword.empty());
	CHECK(ParseOneCommand("\xC2\xB4just text\xC2\xB4").unknownKeyword.empty());
	auto scene = ParseScene("[Commands]\n\"quoted line\"\n");
	CHECK(scene.warnings.empty());
}

TEST_CASE("Warnings are lifted out of nested bodies"){
	auto scene = ParseScene(
		"[Commands]\n"
		"loop(2){\n"
		"\tbogus \"x\"\n"
		"}\n");
	// Two loop iterations, each carrying the flagged command.
	CHECK(scene.warnings.size() >= 1);
	bool found = false;
	for(const auto& w : scene.warnings){
		if(w.message.find("bogus") != std::string::npos) found = true;
	}
	CHECK(found);
}

// ── Text commands and runs ───────────────────────────────────

TEST_CASE("text produces one run and a concatenated argument"){
	auto cmd = ParseOneCommand("text \"Hello, world!\"");
	REQUIRE(cmd.runs.size() == 1);
	CHECK(cmd.runs[0].text == "Hello, world!");
	CHECK(cmd.argument     == "Hello, world!");
	CHECK(cmd.runs[0].bold   == false);
	CHECK(cmd.runs[0].italic == false);
	CHECK((int)cmd.runs[0].colour.a == 0); // no colour override
}

TEST_CASE("textbf and textit set the style on every run"){
	auto bf = ParseOneCommand("textbf \"loud\"");
	REQUIRE(bf.runs.size() == 1);
	CHECK(bf.runs[0].bold);
	CHECK_FALSE(bf.runs[0].italic);

	auto it = ParseOneCommand("textit \"soft\"");
	REQUIRE(it.runs.size() == 1);
	CHECK(it.runs[0].italic);
	CHECK_FALSE(it.runs[0].bold);
}

TEST_CASE("Line-level style ORs with inline style"){
	// Documented in scene-format.md: `textbf "A " + it("B")` renders A bold
	// and B bold-italic.
	auto cmd = ParseOneCommand("textbf \"A \" + it(\"B\")");
	REQUIRE(cmd.runs.size() == 2);
	CHECK(cmd.runs[0].bold);
	CHECK_FALSE(cmd.runs[0].italic);
	CHECK(cmd.runs[1].bold);
	CHECK(cmd.runs[1].italic);
}

TEST_CASE("bold() and it() nest in either order"){
	auto a = ParseOneCommand("text bold(it(\"x\"))");
	REQUIRE(a.runs.size() == 1);
	CHECK(a.runs[0].bold);
	CHECK(a.runs[0].italic);

	auto b = ParseOneCommand("text it(bold(\"x\"))");
	REQUIRE(b.runs.size() == 1);
	CHECK(b.runs[0].bold);
	CHECK(b.runs[0].italic);
}

TEST_CASE("The + operator splits into styled runs"){
	auto cmd = ParseOneCommand("text color(RED, \"Hello, \") + color(BLUE, \"World!\")");
	REQUIRE(cmd.runs.size() == 2);
	CHECK(cmd.runs[0].text == "Hello, ");
	CHECK((int)cmd.runs[0].colour.r == 0xFF);
	CHECK((int)cmd.runs[0].colour.g == 0x00);
	CHECK(cmd.runs[1].text == "World!");
	CHECK((int)cmd.runs[1].colour.b == 0xFF);
	CHECK(cmd.argument == "Hello, World!");
}

TEST_CASE("Line-level modifiers use leftmost-wins across + segments"){
	auto cmd = ParseOneCommand(
		"text pos([20, 80], \"left\") + pos([90, 90], \"right\")");
	REQUIRE(cmd.runs.size() == 2);
	CHECK(cmd.mods.posX == doctest::Approx(20.0f));
	CHECK(cmd.mods.posY == doctest::Approx(80.0f));
}

TEST_CASE("color() accepts named colours"){
	struct Case{ const char* name; int r, g, b; };
	const Case cases[] = {
		{"RED",   0xFF, 0x00, 0x00},
		{"GREEN", 0x00, 0x80, 0x00},
		{"BLUE",  0x00, 0x00, 0xFF},
		{"WHITE", 0xFF, 0xFF, 0xFF},
		{"BLACK", 0x00, 0x00, 0x00},
	};
	for(const auto& c : cases){
		CAPTURE(c.name);
		auto cmd = ParseOneCommand(
			std::string("text color(") + c.name + ", \"x\")");
		REQUIRE(cmd.runs.size() == 1);
		// Alpha > 0 marks "a colour was explicitly set", including black.
		CHECK((int)cmd.runs[0].colour.a == 255);
	}

	SUBCASE("colour names are case-insensitive"){
		auto lower = ParseOneCommand("text color(red, \"x\")");
		CHECK((int)lower.runs[0].colour.r == 0xFF);
	}
}

TEST_CASE("trans() sets per-run transparency"){
	// The modifier takes a percentage where 0 = opaque; internally it is
	// stored as a 0-1 opacity multiplier.
	auto opaque = ParseOneCommand("text trans(0, \"x\")");
	CHECK(opaque.runs[0].transparency == doctest::Approx(1.0f));

	auto half = ParseOneCommand("text trans(50, \"x\")");
	CHECK(half.runs[0].transparency == doctest::Approx(0.5f));
}

TEST_CASE("textD splits primary and secondary runs"){
	auto cmd = ParseOneCommand("textD \"Good evening!\", \"Godaften!\"");
	CHECK(cmd.type == CommandType::TextD);
	REQUIRE(cmd.runs.size() == 1);
	CHECK(cmd.runs[0].text == "Good evening!");
	REQUIRE(cmd.subtitleRuns.size() == 1);
	CHECK(cmd.subtitleRuns[0].text == "Godaften!");
	// `argument` mirrors only the primary side.
	CHECK(cmd.argument == "Good evening!");
}

TEST_CASE("textD with one argument leaves the subtitle side empty"){
	// Dispatch checks subtitleRuns.empty() and falls back to a plain text
	// render, so a missing translation must not fabricate an empty line.
	auto cmd = ParseOneCommand("textD \"Only primary\"");
	CHECK(cmd.type == CommandType::TextD);
	REQUIRE(cmd.runs.size() == 1);
	CHECK(cmd.runs[0].text == "Only primary");
	CHECK(cmd.subtitleRuns.empty());
}

TEST_CASE("textD supports + and modifiers on both sides"){
	auto cmd = ParseOneCommand(
		"textD color(RED, \"A\") + \"B\", it(\"sub1\") + \"sub2\"");
	REQUIRE(cmd.runs.size() == 2);
	CHECK((int)cmd.runs[0].colour.r == 0xFF);
	REQUIRE(cmd.subtitleRuns.size() == 2);
	CHECK(cmd.subtitleRuns[0].italic);
	CHECK_FALSE(cmd.subtitleRuns[1].italic);
}

// ── Quoting (1.7) ────────────────────────────────────────────

TEST_CASE("All three quote delimiters are accepted"){
	CHECK(ParseOneCommand("text \"double\"").runs[0].text == "double");
	CHECK(ParseOneCommand("text 'single'").runs[0].text   == "single");
	CHECK(ParseOneCommand("text \xC2\xB4""acute\xC2\xB4").runs[0].text == "acute");
}

TEST_CASE("A non-matching delimiter inside a string is literal"){
	// The whole point of 1.7: text can contain quotation marks.
	CHECK(ParseOneCommand("text 'He said \"hi\"'").runs[0].text
		== "He said \"hi\"");
	CHECK(ParseOneCommand("text \"don't worry\"").runs[0].text
		== "don't worry");
	CHECK(ParseOneCommand("text \xC2\xB4mixed \"a\" and 'b'\xC2\xB4").runs[0].text
		== "mixed \"a\" and 'b'");
}

TEST_CASE("# inside quoted text is not a comment (1.6.6 regression)"){
	auto scene = ParseScene("[Commands]\ntext \"Item #1 and #2\"\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].runs[0].text == "Item #1 and #2");
	CHECK(scene.commands[0].comment.empty());
}

TEST_CASE("#RRGGBB hex colours survive the comment stripper (1.8.2)"){
	// Before 1.8.2 the `#` was read as a line comment, truncating the cue
	// to `text color(` and losing the colour entirely.
	auto cmd = ParseOneCommand("text color(#FF5500, \"Custom colour\")");
	REQUIRE(cmd.runs.size() == 1);
	CHECK(cmd.runs[0].text == "Custom colour");
	CHECK((int)cmd.runs[0].colour.r == 0xFF);
	CHECK((int)cmd.runs[0].colour.g == 0x55);
	CHECK((int)cmd.runs[0].colour.b == 0x00);
	CHECK((int)cmd.runs[0].colour.a == 255);
	CHECK(cmd.comment.empty());

	SUBCASE("a spaced-out argument list works too"){
		auto c = ParseOneCommand("text color( #00FF80 , \"x\")");
		REQUIRE(c.runs.size() == 1);
		CHECK((int)c.runs[0].colour.g == 0xFF);
		CHECK((int)c.runs[0].colour.b == 0x80);
	}
	SUBCASE("lower-case hex digits"){
		auto c = ParseOneCommand("text color(#ff5500, \"x\")");
		CHECK((int)c.runs[0].colour.r == 0xFF);
		CHECK((int)c.runs[0].colour.g == 0x55);
	}
	SUBCASE("hex inside a + concatenation"){
		auto c = ParseOneCommand(
			"text color(#FF0000, \"A\") + color(#0000FF, \"B\")");
		REQUIRE(c.runs.size() == 2);
		CHECK((int)c.runs[0].colour.r == 0xFF);
		CHECK((int)c.runs[1].colour.b == 0xFF);
		CHECK(c.argument == "AB");
	}
	SUBCASE("hex inside nested modifiers"){
		auto c = ParseOneCommand(
			"text pos([10, 20], trans(50, color(#123456, \"deep\")))");
		REQUIRE(c.runs.size() == 1);
		CHECK(c.runs[0].text == "deep");
		CHECK((int)c.runs[0].colour.r == 0x12);
		CHECK((int)c.runs[0].colour.g == 0x34);
		CHECK((int)c.runs[0].colour.b == 0x56);
		CHECK(c.mods.posX == doctest::Approx(10.0f));
	}
	SUBCASE("a trailing comment still works on the same line"){
		auto scene = ParseScene(
			"[Commands]\ntext color(#FF5500, \"x\")   #pick a warmer orange\n");
		REQUIRE(scene.commands.size() == 1);
		CHECK((int)scene.commands[0].runs[0].colour.r == 0xFF);
		CHECK(scene.commands[0].comment == "pick a warmer orange");
	}
	SUBCASE("hex works on show and its modifiers too"){
		auto c = ParseOneCommand("show pos([50, 50], a.png)");
		CHECK(c.argument == "a.png");
	}
}

TEST_CASE("A comment that looks like a hex colour is still a comment (1.8.2)"){
	// The reason paren depth was chosen over "6 hex digits means colour":
	// this line must stay a comment.
	auto scene = ParseScene(
		"[Commands]\n"
		"#FFFFFF looks nice for the backdrop\n"
		"clear\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Clear);
}

TEST_CASE("Paren tracking cannot be unbalanced by quoted text (1.8.2)"){
	SUBCASE("a closing paren inside a string does not open a comment hole"){
		auto scene = ParseScene("[Commands]\ntext \"smile :)\"  #note\n");
		REQUIRE(scene.commands.size() == 1);
		CHECK(scene.commands[0].runs[0].text == "smile :)");
		CHECK(scene.commands[0].comment == "note");
	}
	SUBCASE("an opening paren inside a string does not swallow the comment"){
		auto scene = ParseScene("[Commands]\ntext \"a (b\"  #note\n");
		REQUIRE(scene.commands.size() == 1);
		CHECK(scene.commands[0].runs[0].text == "a (b");
		CHECK(scene.commands[0].comment == "note");
	}
	SUBCASE("a stray closing paren clamps at depth zero"){
		// Depth must not go negative — otherwise every later '#' on the
		// line would stop being a comment.
		auto scene = ParseScene("[Commands]\nclear )  #note\n");
		REQUIRE(scene.commands.size() == 1);
		CHECK(scene.commands[0].comment == "note");
	}
}

TEST_CASE("# outside quotes still starts a comment on every block form"){
	// The 1.6.6 fix had to be applied at five call sites; this walks them.
	SUBCASE("top level"){
		auto s = ParseScene("[Commands]\ntext \"a\" #note\n");
		CHECK(s.commands[0].comment == "note");
	}
	SUBCASE("run body"){
		auto s = ParseScene("[Commands]\nrun{\n\ttext \"a\" #note\n}\n");
		REQUIRE(s.commands.size() == 1);
		CHECK(s.commands[0].type == CommandType::Compound);
	}
	SUBCASE("loop body"){
		auto s = ParseScene("[Commands]\nloop(1){\n\ttext \"a\" #note\n}\n");
		REQUIRE(s.commands.size() == 1);
		CHECK(s.commands[0].runs[0].text == "a");
	}
	SUBCASE("macro body"){
		auto s = ParseScene(
			"[Macro]\nM{\n\ttext \"a\" #note\n}\n[Commands]\nmacro M\n");
		REQUIRE(s.commands.size() == 1);
		CHECK(s.commands[0].runs[0].text == "a");
	}
	SUBCASE("group body"){
		auto s = ParseScene("[Commands]\ngroup G{\n\ttext \"a\" #note\n}\n");
		REQUIRE(s.commands.size() == 1);
		CHECK(s.commands[0].runs[0].text == "a");
		CHECK(s.commands[0].groupName == "G");
	}
}

// ── show / play arguments ────────────────────────────────────

TEST_CASE("show resolves down to the innermost filename"){
	CHECK(ParseOneCommand("show logo.png").argument == "logo.png");
	CHECK(ParseOneCommand("show pos([50,50], logo.png)").argument == "logo.png");
	CHECK(ParseOneCommand("show pos([10,10], trans(80, rotate(45, logo.png)))")
		.argument == "logo.png");
}

TEST_CASE("Image modifiers land on the command's mods"){
	SUBCASE("pos"){
		auto c = ParseOneCommand("show pos([25, 75], a.png)");
		CHECK(c.mods.posX == doctest::Approx(25.0f));
		CHECK(c.mods.posY == doctest::Approx(75.0f));
		CHECK(c.mods.HasPosition());
	}
	SUBCASE("size and its scale() alias"){
		auto s = ParseOneCommand("show size([200, 150], a.png)");
		CHECK(s.mods.width  == 200);
		CHECK(s.mods.height == 150);
		auto sc = ParseOneCommand("show scale(a.png, [400, 300])");
		CHECK(sc.mods.width  == 400);
		CHECK(sc.mods.height == 300);
		CHECK(sc.argument == "a.png");
	}
	SUBCASE("rotate and spin compose"){
		auto c = ParseOneCommand("show rotate(45, spin(90, a.png))");
		CHECK(c.mods.rotation      == doctest::Approx(45.0f));
		CHECK(c.mods.spinDegPerSec == doctest::Approx(90.0f));
		CHECK(c.argument == "a.png");
	}
	SUBCASE("trans"){
		auto c = ParseOneCommand("show trans(60, a.png)");
		CHECK(c.mods.transparency == doctest::Approx(0.4f));
	}
	SUBCASE("random size"){
		auto c = ParseOneCommand("show random(a.png, [50, 60], [200, 210], true)");
		CHECK(c.mods.useRandomSize);
		CHECK(c.mods.minWidth  == 50);
		CHECK(c.mods.minHeight == 60);
		CHECK(c.mods.maxWidth  == 200);
		CHECK(c.mods.maxHeight == 210);
		CHECK(c.mods.keepAspectRatio);
		CHECK(c.argument == "a.png");

		auto noAspect = ParseOneCommand(
			"show random(a.png, [1,1], [2,2], false)");
		CHECK_FALSE(noAspect.mods.keepAspectRatio);
	}
}

TEST_CASE("move() accepts coordinates and side sentinels"){
	SUBCASE("coordinate pairs"){
		auto c = ParseOneCommand("show move([0, 10], [100, 90], 2000, a.png)");
		CHECK(c.mods.hasMove);
		CHECK(c.mods.moveFromX == doctest::Approx(0.0f));
		CHECK(c.mods.moveFromY == doctest::Approx(10.0f));
		CHECK(c.mods.moveToX   == doctest::Approx(100.0f));
		CHECK(c.mods.moveToY   == doctest::Approx(90.0f));
		// Authored in milliseconds, stored in seconds.
		CHECK(c.mods.moveDurationSec == doctest::Approx(2.0f));
		CHECK(c.mods.moveFromSide == 0);
		CHECK(c.mods.moveToSide   == 0);
	}
	SUBCASE("side sentinels N/S/E/W"){
		CHECK(ParseOneCommand("show move(N, [50,50], 800, a.png)").mods.moveFromSide == 1);
		CHECK(ParseOneCommand("show move(S, [50,50], 800, a.png)").mods.moveFromSide == 2);
		CHECK(ParseOneCommand("show move(E, [50,50], 800, a.png)").mods.moveFromSide == 3);
		CHECK(ParseOneCommand("show move(W, [50,50], 800, a.png)").mods.moveFromSide == 4);
		auto toSide = ParseOneCommand("show move([50,50], N, 800, a.png)");
		CHECK(toSide.mods.moveToSide == 1);
	}
}

TEST_CASE("layer() sets image draw order (1.8.3)"){
	auto c = ParseOneCommand("show layer(5, a.png)");
	CHECK(c.mods.layer == 5);
	CHECK(c.argument == "a.png");

	SUBCASE("the default is 0, which reproduces push-order behaviour"){
		CHECK(ParseOneCommand("show a.png").mods.layer == 0);
		CHECK(ParseOneCommand("show pos([50,50], a.png)").mods.layer == 0);
	}
	SUBCASE("negative layers are legal — behind the unlayered ones"){
		CHECK(ParseOneCommand("show layer(-3, a.png)").mods.layer == -3);
	}
	SUBCASE("it nests with other modifiers in either order"){
		auto inner = ParseOneCommand("show layer(2, pos([10, 20], a.png))");
		CHECK(inner.mods.layer == 2);
		CHECK(inner.mods.posX == doctest::Approx(10.0f));
		CHECK(inner.argument == "a.png");

		auto outer = ParseOneCommand("show pos([10, 20], layer(2, a.png))");
		CHECK(outer.mods.layer == 2);
		CHECK(outer.mods.posY == doctest::Approx(20.0f));
		CHECK(outer.argument == "a.png");
	}
	SUBCASE("a non-numeric layer leaves the default rather than failing"){
		auto bad = ParseOneCommand("show layer(front, a.png)");
		CHECK(bad.mods.layer == 0);
		CHECK(bad.argument == "a.png");
	}
}

TEST_CASE("pathMove() waypoints, timing and repeat (1.8.3)"){
	auto c = ParseOneCommand(
		"show pathMove([0, 50], [50, 90], [100, 50], 3000, a.png)");
	CHECK(c.argument == "a.png");
	REQUIRE(c.mods.pathPoints.size() == 3);
	CHECK(c.mods.pathPoints[0].x == doctest::Approx(0.0f));
	CHECK(c.mods.pathPoints[1].y == doctest::Approx(90.0f));
	CHECK(c.mods.pathPoints[2].x == doctest::Approx(100.0f));
	// Authored in milliseconds, stored in seconds — as move() does.
	CHECK(c.mods.pathDurationSec == doctest::Approx(3.0f));
	CHECK(c.mods.pathRepeat == RenderModifiers::PathRepeat::Once);
	// A path also flags hasMove so every existing "animated?" check works.
	CHECK(c.mods.hasMove);

	SUBCASE("two waypoints is the minimum and is accepted"){
		auto two = ParseOneCommand("show pathMove([0,0], [100,100], 1000, a.png)");
		REQUIRE(two.mods.pathPoints.size() == 2);
		CHECK(two.mods.hasMove);
	}
	SUBCASE("repeat modes"){
		CHECK(ParseOneCommand("show pathMove([0,0],[1,1], 500, loop, a.png)")
			.mods.pathRepeat == RenderModifiers::PathRepeat::Loop);
		CHECK(ParseOneCommand("show pathMove([0,0],[1,1], 500, pingpong, a.png)")
			.mods.pathRepeat == RenderModifiers::PathRepeat::PingPong);
		CHECK(ParseOneCommand("show pathMove([0,0],[1,1], 500, once, a.png)")
			.mods.pathRepeat == RenderModifiers::PathRepeat::Once);
	}
	SUBCASE("repeat mode is case-insensitive"){
		CHECK(ParseOneCommand("show pathMove([0,0],[1,1], 500, LOOP, a.png)")
			.mods.pathRepeat == RenderModifiers::PathRepeat::Loop);
		CHECK(ParseOneCommand("show pathMove([0,0],[1,1], 500, PingPong, a.png)")
			.mods.pathRepeat == RenderModifiers::PathRepeat::PingPong);
	}
	SUBCASE("side sentinels work as waypoints, like move()'s endpoints"){
		auto s = ParseOneCommand(
			"show pathMove(W, [50, 50], E, 2000, a.png)");
		REQUIRE(s.mods.pathPoints.size() == 3);
		CHECK(s.mods.pathPoints[0].side == 4); // W
		CHECK(s.mods.pathPoints[1].side == 0); // coordinates
		CHECK(s.mods.pathPoints[2].side == 3); // E
	}
	SUBCASE("many waypoints"){
		auto many = ParseOneCommand(
			"show pathMove([0,0],[25,25],[50,0],[75,25],[100,0], 4000, loop, a.png)");
		CHECK(many.mods.pathPoints.size() == 5);
		CHECK(many.mods.pathRepeat == RenderModifiers::PathRepeat::Loop);
	}
	SUBCASE("it nests with other modifiers and keeps the content"){
		auto n = ParseOneCommand(
			"show pathMove([0,0],[100,100], 1000, loop, size([64, 64], a.png))");
		CHECK(n.argument == "a.png");
		CHECK(n.mods.width == 64);
		CHECK(n.mods.pathPoints.size() == 2);
	}
	SUBCASE("it works on text as well as images"){
		auto t = ParseOneCommand("text pathMove([0,0],[100,100], 1000, \"hi\")");
		REQUIRE(t.runs.size() == 1);
		CHECK(t.runs[0].text == "hi");
		CHECK(t.mods.pathPoints.size() == 2);
		CHECK(t.mods.hasMove);
	}
}

TEST_CASE("A malformed pathMove still shows its content (1.8.3)"){
	// Same graceful degradation as move(): a bad spec must not swallow the
	// cue, it just renders unanimated so the operator can see something is
	// off rather than staring at a blank slave.
	auto check = [](const std::string& line){
		auto c = ParseOneCommand(line);
		CHECK(c.argument == "a.png");
		CHECK(c.mods.pathPoints.empty());
		CHECK_FALSE(c.mods.hasMove);
	};
	SUBCASE("only one waypoint"){ check("show pathMove([0,0], 1000, a.png)"); }
	SUBCASE("zero duration"){     check("show pathMove([0,0],[1,1], 0, a.png)"); }
	SUBCASE("negative duration"){ check("show pathMove([0,0],[1,1], -5, a.png)"); }
	SUBCASE("no duration"){       check("show pathMove([0,0],[1,1],[2,2], a.png)"); }
	SUBCASE("unrecognised repeat mode is a typo, not a silent Once"){
		check("show pathMove([0,0],[1,1], 500, bounce, a.png)");
	}
}

TEST_CASE("move() is unchanged by the pathMove addition (1.8.3)"){
	// The 1.8.3 refactor routed move() and pathMove() through one evaluator;
	// this pins that plain move() still parses exactly as before.
	auto c = ParseOneCommand("show move([0, 10], [100, 90], 2000, a.png)");
	CHECK(c.mods.hasMove);
	CHECK(c.mods.pathPoints.empty()); // two-point path, not a waypoint list
	CHECK(c.mods.moveFromX == doctest::Approx(0.0f));
	CHECK(c.mods.moveToY   == doctest::Approx(90.0f));
	CHECK(c.mods.moveDurationSec == doctest::Approx(2.0f));
	CHECK(c.mods.pathDurationSec == doctest::Approx(0.0f));
}

TEST_CASE("particle() sets type and normalises intensity to 0-1"){
	auto plain = ParseOneCommand("show particle(RAIN, a.png)");
	CHECK(plain.mods.particleType == ParticleType::Rain);
	CHECK(plain.mods.particleIntensity == doctest::Approx(1.0f));

	auto scaled = ParseOneCommand("show particle(SNOW, 40, a.png)");
	CHECK(scaled.mods.particleType == ParticleType::Snow);
	CHECK(scaled.mods.particleIntensity == doctest::Approx(0.4f));

	SUBCASE("intensity is clamped into 0.01-1.0"){
		CHECK(ParseOneCommand("show particle(RAIN, 0, a.png)")
			.mods.particleIntensity == doctest::Approx(0.01f));
		CHECK(ParseOneCommand("show particle(RAIN, 500, a.png)")
			.mods.particleIntensity == doctest::Approx(1.0f));
	}
	SUBCASE("every documented type parses"){
		struct Case{ const char* name; ParticleType type; };
		const Case cases[] = {
			{"RAIN", ParticleType::Rain}, {"SNOW", ParticleType::Snow},
			{"CONFETTI", ParticleType::Confetti}, {"RISE", ParticleType::Rise},
			{"SCATTER", ParticleType::Scatter}, {"BROWNIAN", ParticleType::Brownian},
			{"OSCILLATION", ParticleType::Oscillation}, {"ORBIT", ParticleType::Orbit},
			{"NOISE", ParticleType::Noise}, {"LIFECURVE", ParticleType::LifeCurve},
		};
		for(const auto& c : cases){
			CAPTURE(c.name);
			auto cmd = ParseOneCommand(
				std::string("show particle(") + c.name + ", a.png)");
			CHECK(cmd.mods.particleType == c.type);
		}
	}
}

TEST_CASE("trail(), trailGlow() and bounce()"){
	SUBCASE("default trail mode is Ghost"){
		auto c = ParseOneCommand("show particle(ORBIT, trail(16, a.png))");
		CHECK(c.mods.trailLength == 16);
		CHECK(c.mods.trailMode == RenderModifiers::TrailMode::Ghost);
	}
	SUBCASE("explicit modes"){
		CHECK(ParseOneCommand("show particle(NOISE, trail(12, STRETCH, a.png))")
			.mods.trailMode == RenderModifiers::TrailMode::Stretch);
		CHECK(ParseOneCommand("show particle(NOISE, trail(12, GLOW, a.png))")
			.mods.trailMode == RenderModifiers::TrailMode::Glow);
	}
	SUBCASE("trailGlow is sugar for trail(N, GLOW, …)"){
		auto c = ParseOneCommand("show particle(NOISE, trailGlow(8, a.png))");
		CHECK(c.mods.trailLength == 8);
		CHECK(c.mods.trailMode == RenderModifiers::TrailMode::Glow);
	}
	SUBCASE("trail length is clamped to 64"){
		CHECK(ParseOneCommand("show particle(RAIN, trail(999, a.png))")
			.mods.trailLength == 64);
	}
	SUBCASE("bounce"){
		CHECK(ParseOneCommand("show particle(BROWNIAN, bounce(true, a.png))")
			.mods.bounceEdges);
		CHECK_FALSE(ParseOneCommand("show particle(BROWNIAN, bounce(false, a.png))")
			.mods.bounceEdges);
	}
}

TEST_CASE("randomImages() collects the list form"){
	auto c = ParseOneCommand(
		"show particle(RAIN, 50, randomImages([a.png, b.png, c.png]))");
	REQUIRE(c.mods.randomImagePaths.size() == 3);
	CHECK(c.mods.randomImagePaths[0] == "a.png");
	CHECK(c.mods.randomImagePaths[2] == "c.png");
}

TEST_CASE("play fade spec (1.6.6)"){
	SUBCASE("bare play has no fades"){
		auto c = ParseOneCommand("play movie.mkv");
		CHECK(c.argument == "movie.mkv");
		CHECK(c.fadeInMs  == 0);
		CHECK(c.fadeOutMs == 0);
	}
	SUBCASE("both sides"){
		auto c = ParseOneCommand("play movie.mkv 1000, 2000");
		CHECK(c.argument  == "movie.mkv");
		CHECK(c.fadeInMs  == 1000);
		CHECK(c.fadeOutMs == 2000);
	}
	SUBCASE("a filename ending in digits is not eaten"){
		// The regression the whitespace-gap rule exists for.
		auto c = ParseOneCommand("play movie2.mp4 1000, 2000");
		CHECK(c.argument  == "movie2.mp4");
		CHECK(c.fadeInMs  == 1000);
		CHECK(c.fadeOutMs == 2000);
	}
	SUBCASE("a comma with no digit gap is left in the filename"){
		auto c = ParseOneCommand("play my,file.mp3");
		CHECK(c.fadeInMs  == 0);
		CHECK(c.fadeOutMs == 0);
	}
}

TEST_CASE("countdown (1.7)"){
	CHECK(ParseOneCommand("countdown(90)").countdownSeconds == 90);
	CHECK(ParseOneCommand("countdown(90)").countdownLabel.empty());

	auto en = ParseOneCommand("countdown(120, \"en\")");
	CHECK(en.countdownSeconds == 120);
	CHECK(en.countdownLabel == "en");

	auto da = ParseOneCommand("countdown(120, \"da\")");
	CHECK(da.countdownLabel == "da");

	SUBCASE("language tags are case-insensitive"){
		CHECK(ParseOneCommand("countdown(60, \"EN\")").countdownLabel == "en");
	}
	SUBCASE("an unknown tag falls back to no label"){
		CHECK(ParseOneCommand("countdown(60, \"fr\")").countdownLabel.empty());
	}
	SUBCASE("seconds clamp to the MM:SS range"){
		CHECK(ParseOneCommand("countdown(99999)").countdownSeconds == 3599);
		CHECK(ParseOneCommand("countdown(-5)").countdownSeconds == 0);
	}
}

TEST_CASE("typewrite (1.8.2)"){
	auto c = ParseOneCommand("typewrite(50, \"Hello\")");
	CHECK(c.type == CommandType::Typewrite);
	CHECK(c.typeMsPerChar == 50);
	REQUIRE(c.runs.size() == 1);
	CHECK(c.runs[0].text == "Hello");
	CHECK(c.argument == "Hello");

	SUBCASE("the split is on the first structural comma"){
		// A comma inside color(...) belongs to the text expression, not to
		// the argument list.
		auto t = ParseOneCommand("typewrite(40, color(RED, \"A, B\"))");
		CHECK(t.typeMsPerChar == 40);
		REQUIRE(t.runs.size() == 1);
		CHECK(t.runs[0].text == "A, B");
		CHECK((int)t.runs[0].colour.r == 0xFF);
	}
	SUBCASE("the full text machinery works"){
		// Line-level modifiers sit on a segment, exactly as in `text` —
		// leftmost wins for the line, per-segment styles stay per-segment.
		auto t = ParseOneCommand(
			"typewrite(30, pos([50, 20], color(BLUE, \"a\")) + bold(\"b\"))");
		REQUIRE(t.runs.size() == 2);
		CHECK((int)t.runs[0].colour.b == 0xFF);
		CHECK(t.runs[1].bold);
		CHECK(t.mods.posX == doctest::Approx(50.0f));
		CHECK(t.mods.posY == doctest::Approx(20.0f));
		CHECK(t.argument == "ab");
	}
	SUBCASE("a + nested inside a line-level modifier is not a split"){
		// SplitPlus only splits at paren depth 0, so wrapping the whole
		// concatenation in pos(...) yields a single run. This is not a
		// typewrite quirk — plain `text` behaves identically, asserted
		// alongside so the two can never drift.
		auto tw = ParseOneCommand(
			"typewrite(30, pos([50, 20], color(BLUE, \"a\") + bold(\"b\")))");
		auto tx = ParseOneCommand(
			"text pos([50, 20], color(BLUE, \"a\") + bold(\"b\"))");
		CHECK(tw.runs.size() == 1);
		CHECK(tx.runs.size() == 1);
		CHECK(tw.runs.size() == tx.runs.size());
		CHECK(tw.runs[0].text == tx.runs[0].text);
	}
	SUBCASE("LaTeX symbols expand inside it"){
		auto t = ParseOneCommand("typewrite(20, \"\\alpha\")");
		REQUIRE(t.runs.size() == 1);
		CHECK(t.runs[0].text == "\xCE\xB1"); // U+03B1
	}
	SUBCASE("rate is clamped to 0-5000"){
		CHECK(ParseOneCommand("typewrite(-10, \"x\")").typeMsPerChar == 0);
		CHECK(ParseOneCommand("typewrite(99999, \"x\")").typeMsPerChar == 5000);
	}
	SUBCASE("a non-numeric rate degrades to instant rather than failing"){
		auto t = ParseOneCommand("typewrite(fast, \"x\")");
		CHECK(t.typeMsPerChar == 0);
		CHECK(t.runs[0].text == "x");
	}
	SUBCASE("a missing text argument is not a crash"){
		auto t = ParseOneCommand("typewrite(50)");
		CHECK(t.type == CommandType::Typewrite);
		CHECK(t.typeMsPerChar == 50);
		CHECK(t.runs.empty());
	}
	SUBCASE("it composes with & and with trailing comments"){
		auto scene = ParseScene(
			"[Commands]\nclear & typewrite(25, \"hi\")   #types in\n");
		REQUIRE(scene.commands.size() == 1);
		REQUIRE(scene.commands[0].type == CommandType::Compound);
		REQUIRE(scene.commands[0].subCommands.size() == 2);
		CHECK(scene.commands[0].subCommands[1].type == CommandType::Typewrite);
		CHECK(scene.commands[0].subCommands[1].typeMsPerChar == 25);
		CHECK(scene.commands[0].comment == "types in");
	}
	SUBCASE("it is executable, so it is not skipped like a comment"){
		auto scene = ParseScene("[Commands]\ntypewrite(30, \"a\")\n");
		REQUIRE(scene.commands.size() == 1);
		CHECK(scene.commands[0].type != CommandType::Comment);
		CHECK(scene.warnings.empty());
	}
}

TEST_CASE("clear takes an optional group name; the wipe variants take none"){
	CHECK(ParseOneCommand("clear").argument.empty());
	CHECK(ParseOneCommand("clear Backdrop").argument == "Backdrop");
	CHECK(ParseOneCommand("clearText").argument.empty());
	CHECK(ParseOneCommand("clearImages").argument.empty());
	CHECK(ParseOneCommand("clearAll").argument.empty());
	CHECK(ParseOneCommand("stopParticleCont Hearts").argument == "Hearts");
}

TEST_CASE("comment stores its text verbatim and stays non-executable"){
	auto c = ParseOneCommand("comment \"--- Akt 1 ---\"");
	CHECK(c.type == CommandType::Comment);
	CHECK(c.argument == "--- Akt 1 ---");
}

// ── Compound, blocks, macros, groups ─────────────────────────

TEST_CASE("& joins commands into one Compound cue"){
	auto c = ParseOneCommand("clear & text \"Next slide\"");
	CHECK(c.type == CommandType::Compound);
	REQUIRE(c.subCommands.size() == 2);
	CHECK(c.subCommands[0].type == CommandType::Clear);
	CHECK(c.subCommands[1].type == CommandType::Text);
	CHECK(c.subCommands[1].runs[0].text == "Next slide");
}

TEST_CASE("A single-part & line collapses to that command"){
	auto c = ParseOneCommand("clear &");
	CHECK(c.type == CommandType::Clear);
}

TEST_CASE("& is only a separator when structural (1.8.2)"){
	SUBCASE("inside a double-quoted string"){
		auto c = ParseOneCommand("text \"Tom & Jerry\"");
		CHECK(c.type == CommandType::Text);
		REQUIRE(c.runs.size() == 1);
		CHECK(c.runs[0].text == "Tom & Jerry");
		CHECK(c.subCommands.empty());
	}
	SUBCASE("inside the other two delimiters"){
		CHECK(ParseOneCommand("text 'Salt & Pepper'").runs[0].text
			== "Salt & Pepper");
		CHECK(ParseOneCommand("text \xC2\xB4R&D\xC2\xB4").runs[0].text == "R&D");
	}
	SUBCASE("inside a modifier argument list"){
		auto c = ParseOneCommand("text color(RED, \"Tom & Jerry\")");
		CHECK(c.type == CommandType::Text);
		REQUIRE(c.runs.size() == 1);
		CHECK(c.runs[0].text == "Tom & Jerry");
		CHECK((int)c.runs[0].colour.r == 0xFF);
	}
	SUBCASE("a structural & still joins two commands"){
		auto c = ParseOneCommand("clear & text \"Tom & Jerry\"");
		REQUIRE(c.type == CommandType::Compound);
		REQUIRE(c.subCommands.size() == 2);
		CHECK(c.subCommands[0].type == CommandType::Clear);
		CHECK(c.subCommands[1].runs[0].text == "Tom & Jerry");
	}
	SUBCASE("three-way split still works"){
		auto c = ParseOneCommand(
			"clear & show a.png & text \"done\"");
		REQUIRE(c.type == CommandType::Compound);
		REQUIRE(c.subCommands.size() == 3);
		CHECK(c.subCommands[0].type == CommandType::Clear);
		CHECK(c.subCommands[1].argument == "a.png");
		CHECK(c.subCommands[2].runs[0].text == "done");
	}
	SUBCASE("& in a quoted string on either side of a real separator"){
		auto c = ParseOneCommand(
			"text \"A & B\" & text \"C & D\"");
		REQUIRE(c.type == CommandType::Compound);
		REQUIRE(c.subCommands.size() == 2);
		CHECK(c.subCommands[0].runs[0].text == "A & B");
		CHECK(c.subCommands[1].runs[0].text == "C & D");
	}
	SUBCASE("both fixed separators coexist on one line"){
		auto scene = ParseScene(
			"[Commands]\n"
			"clear & text color(#FF5500, \"Tom & Jerry\")   #both at once\n");
		REQUIRE(scene.commands.size() == 1);
		auto& c = scene.commands[0];
		REQUIRE(c.type == CommandType::Compound);
		REQUIRE(c.subCommands.size() == 2);
		CHECK(c.subCommands[1].runs[0].text == "Tom & Jerry");
		CHECK((int)c.subCommands[1].runs[0].colour.r == 0xFF);
		CHECK(c.comment == "both at once");
	}
}

TEST_CASE("run{} produces a single Compound cue"){
	auto scene = ParseScene(
		"[Commands]\n"
		"run{\n"
		"\tclear\n"
		"\tshow bg.png\n"
		"\ttext \"Welcome\"\n"
		"}\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Compound);
	REQUIRE(scene.commands[0].subCommands.size() == 3);
	CHECK(scene.commands[0].subCommands[1].argument == "bg.png");
}

TEST_CASE("loop() expands inline at parse time"){
	auto scene = ParseScene(
		"[Commands]\n"
		"loop(3){\n"
		"\ttext \"a\"\n"
		"}\n");
	// Each iteration becomes its own navigator row.
	CHECK(scene.commands.size() == 3);
	for(const auto& c : scene.commands) CHECK(c.type == CommandType::Text);
}

TEST_CASE("loop() clearEvery injects a clear after every N iterations"){
	auto scene = ParseScene(
		"[Commands]\n"
		"loop(2, 1){\n"
		"\ttext \"a\"\n"
		"}\n");
	// a, clear, a, clear
	REQUIRE(scene.commands.size() == 4);
	CHECK(scene.commands[0].type == CommandType::Text);
	CHECK(scene.commands[1].type == CommandType::Clear);
	CHECK(scene.commands[2].type == CommandType::Text);
	CHECK(scene.commands[3].type == CommandType::Clear);
}

TEST_CASE("[Macro] bodies are inlined by name"){
	auto scene = ParseScene(
		"[Macro]\n"
		"Title{\n"
		"\ttext \"SatyrAV\"\n"
		"\ttextCont \" rocks\"\n"
		"}\n"
		"[Commands]\n"
		"macro Title\n");
	REQUIRE(scene.commands.size() == 2);
	CHECK(scene.commands[0].type == CommandType::Text);
	CHECK(scene.commands[1].type == CommandType::TextCont);
}

TEST_CASE("An unknown macro name is silently skipped"){
	auto scene = ParseScene("[Commands]\nmacro NoSuchThing\n");
	CHECK(scene.commands.empty());
}

TEST_CASE("A macro can be referenced more than once"){
	auto scene = ParseScene(
		"[Macro]\n"
		"M{\n"
		"\tclear\n"
		"}\n"
		"[Commands]\n"
		"macro M\n"
		"macro M\n");
	CHECK(scene.commands.size() == 2);
}

TEST_CASE("A macro brace on the following line is accepted"){
	auto scene = ParseScene(
		"[Macro]\n"
		"M\n"
		"{\n"
		"\tclear\n"
		"}\n"
		"[Commands]\n"
		"macro M\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].type == CommandType::Clear);
}

TEST_CASE("group{} tags every command it produces"){
	auto scene = ParseScene(
		"[Commands]\n"
		"group Hearts{\n"
		"\tshow a.png\n"
		"\tshow b.png\n"
		"}\n"
		"text \"ungrouped\"\n");
	REQUIRE(scene.commands.size() == 3);
	CHECK(scene.commands[0].groupName == "Hearts");
	CHECK(scene.commands[1].groupName == "Hearts");
	CHECK(scene.commands[2].groupName.empty());
}

TEST_CASE("group{} supports every block form in its body"){
	auto scene = ParseScene(
		"[Macro]\n"
		"M{\n"
		"\tclear\n"
		"}\n"
		"[Commands]\n"
		"group G{\n"
		"\trun{\n"
		"\t\tshow a.png\n"
		"\t\tshow b.png\n"
		"\t}\n"
		"\tloop(2){\n"
		"\t\ttext \"x\"\n"
		"\t}\n"
		"\tmacro M\n"
		"}\n");
	// run{} -> 1 compound, loop(2) -> 2 rows, macro -> 1 row.
	REQUIRE(scene.commands.size() == 4);
	for(const auto& c : scene.commands){
		CAPTURE(c.lineNumber);
		CHECK(c.groupName == "G");
	}
	// Tagging recurses into a Compound's sub-commands.
	CHECK(scene.commands[0].type == CommandType::Compound);
	for(const auto& sub : scene.commands[0].subCommands){
		CHECK(sub.groupName == "G");
	}
}

TEST_CASE("A group brace on the following line is accepted"){
	auto scene = ParseScene(
		"[Commands]\n"
		"group G\n"
		"{\n"
		"\tclear\n"
		"}\n");
	REQUIRE(scene.commands.size() == 1);
	CHECK(scene.commands[0].groupName == "G");
}

// ── Line endings ─────────────────────────────────────────────

TEST_CASE("CRLF scene files"){
	// Trim() strips only spaces and tabs, so a stray '\r' would end up
	// glued to the last token on every line. On Windows that never
	// happens because ParseFile opens the stream in text mode and the CRT
	// translates CRLF to LF on read. On POSIX there is no translation, so
	// a scene authored on Windows and parsed on Linux keeps the '\r' and
	// section headers stop matching.
	//
	// The assertions below therefore differ per platform *on purpose* —
	// they document the real behaviour rather than an aspiration. See the
	// note in docs/scene-format.md.
	auto scene = ParseScene("[Commands]\r\nclear\r\ntext \"a\"\r\n");
#ifdef _WIN32
	REQUIRE(scene.commands.size() == 2);
	CHECK(scene.commands[0].type == CommandType::Clear);
#else
	// '[Commands]\r' never matches the section header, so nothing is read.
	CHECK(scene.commands.empty());
#endif
}

TEST_CASE("LF scene files parse identically everywhere"){
	auto scene = ParseScene("[Commands]\nclear\ntext \"a\"\n");
	REQUIRE(scene.commands.size() == 2);
	CHECK(scene.commands[0].type == CommandType::Clear);
	CHECK(scene.commands[1].runs[0].text == "a");
}
