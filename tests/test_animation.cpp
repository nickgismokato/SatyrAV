// (2.1.0) The [Animation] section.
//
// Pinned here: a block's keys become one `show` / `text` command with a
// pathMove() (run once per pass), plus the repeat / for / every / random
// settings; `show NAME`, `stop NAME` and `clear NAME` are marked when NAME
// is an animation, wherever the section sits in the file; and mistakes
// warn instead of throwing the block or the scene away silently.
//
// The runner itself (passes over time, soft stops, hard cuts) lives in
// PerformanceScreen and needs SDL, so it is outside this suite.

#include <doctest/doctest.h>
#include "helpers.hpp"

using namespace SatyrAV;
using SatyrTest::ParseScene;

static const char* kTumbleweed =
	"[Animation]\n"
	"Tumbleweed{\n"
	"    show     = tumbleweed.png   # rolls across\n"
	"    path     = W, E\n"
	"    duration = 3000\n"
	"    repeat   = 5\n"
	"    for      = 30\n"
	"    every    = 4000\n"
	"    randomY  = 600, 900\n"
	"}\n";

TEST_CASE("A full block becomes one animation"){
	auto s = ParseScene(std::string(kTumbleweed) + "[Commands]\nshow Tumbleweed\n");
	REQUIRE(s.animations.size() == 1);
	const auto& a = s.animations[0];
	CHECK(a.name == "Tumbleweed");
	CHECK(a.lineNumber == 2);
	CHECK(a.command.type == CommandType::Show);
	CHECK(a.command.argument == "tumbleweed.png");
	REQUIRE(a.command.mods.pathPoints.size() == 2);
	CHECK(a.command.mods.pathPoints[0].side == 4);  // W
	CHECK(a.command.mods.pathPoints[1].side == 3);  // E
	CHECK(a.command.mods.pathDurationSec == doctest::Approx(3.0));
	CHECK(a.command.mods.pathRepeat == RenderModifiers::PathRepeat::Once);
	CHECK(a.repeat == 5);
	CHECK(a.forSec == doctest::Approx(30.0));
	CHECK(a.everySec == doctest::Approx(4.0));
	CHECK(a.hasRandomY);
	CHECK_FALSE(a.hasRandomX);
	CHECK(a.randomYMin == doctest::Approx(600.0));
	CHECK(a.randomYMax == doctest::Approx(900.0));
	CHECK(s.warnings.empty());
}

TEST_CASE("Defaults: forever, back to back, no randomness"){
	auto s = ParseScene("[Animation]\nA{\nshow = x.png\npath = [0,50], [100,50]\nduration = 2000\n}\n");
	REQUIRE(s.animations.size() == 1);
	CHECK(s.animations[0].repeat == -1);
	CHECK(s.animations[0].forSec == 0.0f);
	CHECK(s.animations[0].everySec == 0.0f);
	CHECK_FALSE(s.animations[0].hasRandomX);
}

TEST_CASE("Brace on the next line, any case keys, [animations] header"){
	auto s = ParseScene("[animations]\nB\n{\nSHOW = x.png\nPath = N, S\nDuration = 1500\nRepeat = forever\nrandomX = [100, 1800]\n}\n");
	REQUIRE(s.animations.size() == 1);
	CHECK(s.animations[0].name == "B");
	CHECK(s.animations[0].repeat == -1);
	CHECK(s.animations[0].hasRandomX);
	CHECK(s.animations[0].randomXMax == doctest::Approx(1800.0));
}

TEST_CASE("Text animations, and layer / size on images"){
	auto s = ParseScene("[Animation]\n"
		"T{\ntext = \"Hej\"\npath = W, E\nduration = 2000\n}\n"
		"I{\nshow = x.png\npath = W, E\nduration = 2000\nlayer = 3\nsize = [20, 20]\n}\n");
	REQUIRE(s.animations.size() == 2);
	CHECK(s.animations[0].command.type == CommandType::Text);
	REQUIRE_FALSE(s.animations[0].command.runs.empty());
	CHECK(s.animations[0].command.runs[0].text == "Hej");
	CHECK(s.animations[0].command.mods.pathPoints.size() == 2);
	CHECK(s.animations[1].command.mods.layer == 3);
	CHECK(s.animations[1].command.argument == "x.png");
	CHECK(s.animations[1].command.mods.pathPoints.size() == 2);
}

TEST_CASE("show / stop / clear NAME are marked, before or after the section"){
	auto s = ParseScene(std::string("[Commands]\n"
		"show Tumbleweed\n"
		"stop Tumbleweed\n"
		"clear Tumbleweed\n"
		"show other.png\n"
		"clear\n"
		"text \"x\" & show Tumbleweed\n") + kTumbleweed);
	CHECK(s.commands[0].animationName == "Tumbleweed");
	CHECK(s.commands[1].animationName == "Tumbleweed");
	CHECK(s.commands[2].animationName == "Tumbleweed");
	CHECK(s.commands[3].animationName.empty());
	CHECK(s.commands[4].animationName.empty());
	REQUIRE(s.commands[5].type == CommandType::Compound);
	CHECK(s.commands[5].subCommands[1].animationName == "Tumbleweed");
}

TEST_CASE("Mistakes warn"){
	SUBCASE("missing path or duration drops the block"){
		auto s = ParseScene("[Animation]\nA{\nshow = x.png\npath = W, E\n}\n");
		CHECK(s.animations.empty());
		CHECK(s.warnings.size() == 1);
	}
	SUBCASE("neither show nor text"){
		auto s = ParseScene("[Animation]\nA{\npath = W, E\nduration = 100\n}\n");
		CHECK(s.animations.empty());
		CHECK(s.warnings.size() == 1);
	}
	SUBCASE("one point is not a path"){
		auto s = ParseScene("[Animation]\nA{\nshow = x.png\npath = W\nduration = 100\n}\n");
		CHECK(s.animations.empty());
		CHECK(s.warnings.size() == 1);
	}
	SUBCASE("bad values and unknown keys warn but keep the block"){
		auto s = ParseScene("[Animation]\nA{\nshow = x.png\npath = W, E\nduration = 100\n"
			"repeat = lots\nfor = -1\nevery = 0\nrandomY = 50\ncolour = red\n}\n");
		CHECK(s.animations.size() == 1);
		CHECK(s.warnings.size() == 5);
		CHECK(s.animations[0].repeat == -1);
	}
	SUBCASE("a name used twice keeps the first"){
		auto s = ParseScene("[Animation]\n"
			"A{\nshow = one.png\npath = W, E\nduration = 100\n}\n"
			"A{\nshow = two.png\npath = W, E\nduration = 100\n}\n");
		REQUIRE(s.animations.size() == 1);
		CHECK(s.animations[0].command.argument == "one.png");
		CHECK(s.warnings.size() == 1);
	}
}

TEST_CASE("show = and text = take every modifier show and text take"){
	auto s = ParseScene("[Animation]\n"
		"A{\nshow = trans(50, spin(-90, rotate(10, x.png)))\npath = W, E\nduration = 2000\n}\n"
		"B{\nshow = randomImages([a.png, b.png])\npath = W, E\nduration = 2000\n}\n"
		"C{\nshow = random(x.png, [10,10], [40,40], true)\npath = W, E\nduration = 2000\n}\n"
		"D{\ntext = rotate(20, colour(RED, \"Hej\")) + bold(\" du\") + \"!\"\npath = W, E\nduration = 2000\n}\n");
	REQUIRE(s.animations.size() == 4);
	CHECK(s.warnings.empty());

	const auto& a = s.animations[0].command.mods;
	CHECK(a.rotation == doctest::Approx(10.0));
	CHECK(a.spinDegPerSec == doctest::Approx(-90.0));
	CHECK(a.transparency == doctest::Approx(0.5));
	CHECK(a.pathPoints.size() == 2);

	CHECK(s.animations[1].command.mods.randomImagePaths.size() == 2);
	CHECK(s.animations[2].command.mods.useRandomSize);

	// The whole `+` line survives, and the path moves all of it.
	const auto& d = s.animations[3].command;
	REQUIRE(d.runs.size() == 3);
	CHECK(d.runs[0].text == "Hej");
	CHECK(d.runs[0].colour.r == 255);
	CHECK(d.runs[1].bold);
	CHECK(d.runs[2].text == "!");
	CHECK(d.mods.rotation == doctest::Approx(20.0));
	CHECK(d.mods.pathPoints.size() == 2);
}

TEST_CASE("particle() in an animation warns and shows the image instead"){
	auto s = ParseScene("[Animation]\nA{\nshow = particle(RAIN, x.png)\npath = W, E\nduration = 2000\n}\n");
	REQUIRE(s.animations.size() == 1);
	CHECK(s.animations[0].command.mods.particleType == ParticleType::None);
	CHECK(s.animations[0].command.argument == "x.png");
	CHECK(s.warnings.size() == 1);
}
