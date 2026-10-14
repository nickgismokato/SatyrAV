// (2.0.2) Fixed text centre: which lines a block of centred text will hold,
// and the FixedTextCentre option.
//
// Pinned here: a block runs from the cue that starts it to the next
// `clear` / `clearText` / `clearAll` (or the scene's end); positioned and
// moving text is not part of it; compounds count in firing order; and the
// option is off for existing projects, on for new ones, and a scene can
// switch it either way.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/TextPlan.hpp"
#include "core/CueEngine.hpp"
#include "core/Project.hpp"
#include "core/SchemaParser.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;
using SatyrTest::ParseOneCommand;

// The first run's text of each planned cue.
static std::vector<std::string> Texts(const std::vector<const Command*>& plan){
	std::vector<std::string> out;
	for(const auto& runs : TextPlan::PlannedRuns(plan)){
		std::string s;
		for(const auto& r : runs) s += r.text;
		out.push_back(s);
	}
	return out;
}
using V = std::vector<std::string>;

TEST_CASE("Which cues add a centred line, and which end a block"){
	CHECK(TextPlan::AddsCentredLine(ParseOneCommand("text \"A\"")));
	CHECK(TextPlan::AddsCentredLine(ParseOneCommand("textbf \"A\"")));
	CHECK(TextPlan::AddsCentredLine(ParseOneCommand("textit \"A\"")));
	CHECK(TextPlan::AddsCentredLine(ParseOneCommand("typewrite(50, \"A\")")));
	CHECK(TextPlan::AddsCentredLine(ParseOneCommand("textD \"A\", \"a\"")));
	CHECK_FALSE(TextPlan::AddsCentredLine(ParseOneCommand("text pos([20, 80], \"A\")")));
	CHECK_FALSE(TextPlan::AddsCentredLine(ParseOneCommand("text move(W, E, 1000, \"A\")")));
	CHECK_FALSE(TextPlan::AddsCentredLine(ParseOneCommand("textCont \"A\"")));
	CHECK_FALSE(TextPlan::AddsCentredLine(ParseOneCommand("show logo.png")));

	CHECK(TextPlan::EndsBlock(ParseOneCommand("clear")));
	CHECK(TextPlan::EndsBlock(ParseOneCommand("clearText")));
	CHECK(TextPlan::EndsBlock(ParseOneCommand("clearAll")));
	CHECK_FALSE(TextPlan::EndsBlock(ParseOneCommand("clear grp")));
	CHECK_FALSE(TextPlan::EndsBlock(ParseOneCommand("clearImages")));
}

TEST_CASE("A block runs to the next clear, in firing order"){
	auto s = ParseScene(
		"[Commands]\n"
		"text \"A\"\n"                       // 0
		"text pos([10, 10], \"P\")\n"        // 1  positioned: not in the block
		"show logo.png\n"                    // 2
		"comment \"note\"\n"                 // 3
		"text \"B\\nC\"\n"                   // 4  one cue, two lines (split by the slave)
		"clear & text \"D\"\n"               // 5  compound: the clear ends A's block
		"textCont \" more\"\n"               // 6  appends, no new line
		"textD \"E\", \"e\"\n"               // 7
		"clear grp\n"                        // 8  one group only: block goes on
		"typewrite(40, \"F\")\n"             // 9
		"clearText\n"                        // 10
		"text \"G\"\n");                     // 11 runs to the end of the scene
	REQUIRE(s.commands.size() == 12);

	CHECK(Texts(TextPlan::PlanBlock(s, &s.commands[0])) == V{"A", "B\\nC"});
	// Starting mid-block plans from there on.
	CHECK(Texts(TextPlan::PlanBlock(s, &s.commands[4])) == V{"B\\nC"});
	// The text inside `clear & text "D"` starts the next block.
	REQUIRE(s.commands[5].type == CommandType::Compound);
	const Command* d = &s.commands[5].subCommands[1];
	CHECK(Texts(TextPlan::PlanBlock(s, d)) == V{"D", "E", "F"});
	CHECK(Texts(TextPlan::PlanBlock(s, &s.commands[11])) == V{"G"});

	SUBCASE("a cue not in the scene plans nothing"){
		Command stray = ParseOneCommand("text \"X\"");
		CHECK(TextPlan::PlanBlock(s, &stray).empty());
	}
}

TEST_CASE("A block may start with the clear that opens it"){
	// Starting a plan *at* a clear (the planner is never asked to, but it
	// should not stop before it has begun).
	auto s = ParseScene("[Commands]\nclear\ntext \"A\"\ntext \"B\"\n");
	CHECK(Texts(TextPlan::PlanBlock(s, &s.commands[0])) == V{"A", "B"});
}

TEST_CASE("Run blocks and loops are planned like any other cues"){
	auto s = ParseScene(
		"[Commands]\n"
		"run{\n"
		"    text \"A\"\n"
		"    text \"B\"\n"
		"}\n"
		"loop(2){\n"
		"    text \"L\"\n"
		"}\n"
		"clear\n");
	const Command* first = &s.commands[0].subCommands[0];
	CHECK(Texts(TextPlan::PlanBlock(s, first)) == V{"A", "B", "L", "L"});
}

TEST_CASE("FixedTextCentre: scene either way, project default off, new projects on"){
	CHECK(ParseScene("[Options]\nFixedTextCentre = true\n").fixedTextCentre == 1);
	CHECK(ParseScene("[Options]\nfixedtextcenter = false\n").fixedTextCentre == 0);
	CHECK(ParseScene("[Options]\nFontSize = 30\n").fixedTextCentre == -1);

	TempDir tmp("textplan");
	CHECK(SchemaParser::ParseFile(tmp.Write("a.toml", "[Options]\nFixedTextCentre = true\n")).fixedTextCentre);
	CHECK_FALSE(SchemaParser::ParseFile(tmp.Write("b.toml", "[Options]\nFontSize = 30\n")).fixedTextCentre);

	auto data = Project::Create(RevyType::Other, 2026, "Tester",
		(tmp.Path() / "Proj").string(), "Proj");
	CHECK(data.schema.fixedTextCentre);

	SUBCASE("the cascade"){
		auto make = [](int sceneVal, bool projectVal){
			ProjectData p;
			Akt akt;
			akt.number = 1;
			Scene sc;
			sc.name = "S";
			sc.fixedTextCentre = sceneVal;
			akt.sceneNames.push_back("S");
			akt.scenes.push_back(sc);
			p.schema.akts.push_back(akt);
			p.schema.aktCount = 1;
			p.schema.fixedTextCentre = projectVal;
			return p;
		};
		auto effective = [&](int sceneVal, bool projectVal){
			CueEngine e;
			e.LoadProject(make(sceneVal, projectVal));
			e.SelectAkt(0);
			e.SelectScene(0);
			return e.GetEffectiveFixedTextCentre();
		};
		CHECK(effective(-1, true));
		CHECK_FALSE(effective(-1, false));
		CHECK_FALSE(effective(0, true));    // a scene can switch it off
		CHECK(effective(1, false));         // ...or on
	}
}
