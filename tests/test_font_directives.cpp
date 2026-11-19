// (2.1.0) `use FONT` and `fontSize N` in a scene's [Commands].
//
// Pinned here: both parse as Comment rows (so the navigator shows them and
// never stops on them) with a readable note; each one is stamped onto every
// text run after it in file order, including compound parts and loop
// bodies, until the next one of its kind; `default` goes back to the
// scene's font / size; bad sizes warn and change nothing; and a scene
// without either reads exactly as before 2.1.0.

#include <doctest/doctest.h>
#include "helpers.hpp"

using namespace SatyrAV;
using SatyrTest::ParseScene;

static const TextRun& FirstRun(const Command& c){
	REQUIRE_FALSE(c.runs.empty());
	return c.runs.front();
}

TEST_CASE("use and fontSize are comment rows with a note"){
	auto s = ParseScene("[Commands]\n"
		"use font2.ttf\n"
		"use default\n"
		"fontSize 90 # bigger from here\n"
		"fontSize default\n");
	REQUIRE(s.commands.size() == 4);
	for(const auto& c : s.commands) CHECK(c.type == CommandType::Comment);

	CHECK(s.commands[0].directive == FontDirective::UseFont);
	CHECK(s.commands[0].directiveFont == "font2.ttf");
	CHECK(s.commands[0].argument == "Using font font2.ttf");

	CHECK(s.commands[1].directive == FontDirective::UseFont);
	CHECK(s.commands[1].directiveFont.empty());
	CHECK(s.commands[1].argument == "Using the default font");

	CHECK(s.commands[2].directive == FontDirective::FontSize);
	CHECK(s.commands[2].directiveSize == 90);
	CHECK(s.commands[2].argument == "Font size 90");

	CHECK(s.commands[3].directive == FontDirective::FontSize);
	CHECK(s.commands[3].directiveSize == 0);
	CHECK(s.warnings.empty());
}

TEST_CASE("use takes a quoted name with spaces"){
	auto s = ParseScene("[Commands]\nuse \"My Font.ttf\"\n");
	CHECK(s.commands[0].directiveFont == "My Font.ttf");
}

TEST_CASE("The TODO example: two fonts on one line"){
	auto s = ParseScene("[Preload]\nfont1.ttf\nfont2.ttf\n[Commands]\n"
		"use font1.ttf\n"
		"text \"Hello, \"\n"
		"use font2.ttf\n"
		"textCont \"World!\"\n");
	REQUIRE(s.commands.size() == 4);
	CHECK(FirstRun(s.commands[1]).fontPath == "font1.ttf");
	CHECK(FirstRun(s.commands[3]).fontPath == "font2.ttf");
	CHECK(FirstRun(s.commands[1]).fontSize == 0);
}

TEST_CASE("Directives apply in file order until the next one"){
	auto s = ParseScene("[Commands]\n"
		"text \"a\"\n"
		"fontSize 90\n"
		"text \"b\"\n"
		"use x.ttf\n"
		"text \"c\"\n"
		"fontSize default\n"
		"text \"d\"\n"
		"use default\n"
		"text \"e\"\n");
	auto run = [&](int i){ return FirstRun(s.commands[i]); };
	CHECK(run(0).fontPath.empty()); CHECK(run(0).fontSize == 0);
	CHECK(run(2).fontPath.empty()); CHECK(run(2).fontSize == 90);
	CHECK(run(4).fontPath == "x.ttf"); CHECK(run(4).fontSize == 90);
	CHECK(run(6).fontPath == "x.ttf"); CHECK(run(6).fontSize == 0);
	CHECK(run(8).fontPath.empty()); CHECK(run(8).fontSize == 0);
}

TEST_CASE("Every run of a cue is stamped, also in compounds and typewrite"){
	auto s = ParseScene("[Commands]\n"
		"fontSize 70\n"
		"text \"a\" + bold(\"b\")\n"
		"clear & text \"c\"\n"
		"typewrite(50, \"d\")\n");
	for(const auto& r : s.commands[1].runs) CHECK(r.fontSize == 70);
	REQUIRE(s.commands[2].type == CommandType::Compound);
	CHECK(FirstRun(s.commands[2].subCommands[1]).fontSize == 70);
	CHECK(FirstRun(s.commands[3]).fontSize == 70);
}

TEST_CASE("A directive inside a compound affects the parts after it"){
	auto s = ParseScene("[Commands]\n"
		"text \"a\" & use y.ttf & textCont \"b\"\n"
		"text \"c\"\n");
	REQUIRE(s.commands[0].type == CommandType::Compound);
	const auto& subs = s.commands[0].subCommands;
	REQUIRE(subs.size() == 3);
	CHECK(FirstRun(subs[0]).fontPath.empty());
	CHECK(FirstRun(subs[2]).fontPath == "y.ttf");
	CHECK(FirstRun(s.commands[1]).fontPath == "y.ttf");
}

TEST_CASE("Bad fontSize values warn and change nothing"){
	auto s = ParseScene("[Commands]\n"
		"fontSize 80\n"
		"fontSize big\n"
		"fontSize 0\n"
		"fontSize 12px\n"
		"text \"a\"\n");
	CHECK(s.commands[1].directive == FontDirective::None);
	CHECK(s.commands[1].type == CommandType::Comment);
	CHECK(FirstRun(s.commands[4]).fontSize == 80);
	CHECK(s.warnings.size() == 3);
	CHECK(s.warnings[0].lineNumber == 3);
}

TEST_CASE("use with nothing after it warns and means default"){
	auto s = ParseScene("[Commands]\nuse x.ttf\nuse\ntext \"a\"\n");
	CHECK(FirstRun(s.commands[2]).fontPath.empty());
	REQUIRE(s.warnings.size() == 1);
}

TEST_CASE("Scenes without directives keep unstamped runs"){
	auto s = ParseScene("[Commands]\ntext \"a\"\ntextbf \"b\"\n");
	for(const auto& c : s.commands){
		CHECK(FirstRun(c).fontPath.empty());
		CHECK(FirstRun(c).fontSize == 0);
	}
}
