// (2.0.2) The backstage screen: reading [Backstage] blocks, choosing the
// three scenes to show, and writing the body the ticker expects.
//
// Pinned here: the ticker's line format, that scenes without a block are
// skipped (and that starting one sends nothing), that the lineup crosses
// akter, and that text the ticker cannot take is cleaned and warned about.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/Backstage.hpp"
#include "core/SchemaParser.hpp"
#include "core/Project.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;

static bool HasWarningContaining(const Scene& s, const std::string& text){
	for(const auto& w : s.warnings){
		if(w.message.find(text) != std::string::npos) return true;
	}
	return false;
}

TEST_CASE("ExpectedTime: every accepted spelling"){
	int s = -1;
	CHECK((Backstage::ParseExpectedTime("2m40s", s) && s == 160));
	CHECK((Backstage::ParseExpectedTime("2m", s) && s == 120));
	CHECK((Backstage::ParseExpectedTime("45s", s) && s == 45));
	CHECK((Backstage::ParseExpectedTime("1h5m", s) && s == 3900));
	CHECK((Backstage::ParseExpectedTime("2:40", s) && s == 160));
	CHECK((Backstage::ParseExpectedTime("1:02:03", s) && s == 3723));
	CHECK((Backstage::ParseExpectedTime("160", s) && s == 160));
	CHECK((Backstage::ParseExpectedTime("\"2M 40S\"", s) && s == 160));
	CHECK((Backstage::ParseExpectedTime("0:45", s) && s == 45));

	SUBCASE("rejects"){
		for(const char* bad : {"", "abc", "2m40", "2:75", "2:", ":40", "2x", "1:2:3:4", "-5"}){
			CAPTURE(bad);
			CHECK_FALSE(Backstage::ParseExpectedTime(bad, s));
		}
	}
}

TEST_CASE("FormatTime writes m:ss"){
	CHECK(Backstage::FormatTime(160) == "2:40");
	CHECK(Backstage::FormatTime(45) == "0:45");
	CHECK(Backstage::FormatTime(0) == "0:00");
	CHECK(Backstage::FormatTime(4500) == "75:00");
	CHECK(Backstage::FormatTime(-1) == "");
}

TEST_CASE("Lists: TOML style, bare, and the unclosed-quote typo"){
	using V = std::vector<std::string>;
	CHECK(Backstage::ParseList("[\"Actor 1\", \"Actor 2\"]") == V{"Actor 1", "Actor 2"});
	CHECK(Backstage::ParseList("['A', 'B']") == V{"A", "B"});
	CHECK(Backstage::ParseList("[A, B ,C]") == V{"A", "B", "C"});
	CHECK(Backstage::ParseList("A, B") == V{"A", "B"});
	CHECK(Backstage::ParseList("[\"Ninja 1]") == V{"Ninja 1"});
	CHECK(Backstage::ParseList("[\"Smith, Jr.\", \"B\"]") == V{"Smith, Jr.", "B"});
	CHECK(Backstage::ParseList("[]").empty());
	CHECK(Backstage::ParseList("[\"\", \"A\", ]") == V{"A"});
}

TEST_CASE("A scene's [Backstage] block, in any case"){
	auto s = ParseScene(
		"[backstage]\n"
		"Name = Scene 1\n"
		"expectedtime = 2m40s   # roughly\n"
		"ACTORS = [\"Actor 1\", \"Actor 2\"]\n"
		"Ninjas = [\"Ninja 1]\n"
		"[Commands]\n"
		"text \"hi\"\n");
	CHECK(s.backstage.present);
	CHECK(s.backstage.name == "Scene 1");
	CHECK(s.backstage.expectedSeconds == 160);
	CHECK(s.backstage.actors == std::vector<std::string>{"Actor 1", "Actor 2"});
	CHECK(s.backstage.ninjas == std::vector<std::string>{"Ninja 1"});
	CHECK(s.commands.size() == 1);
	CHECK(s.warnings.empty());
}

TEST_CASE("A list may span lines, TOML style"){
	auto s = ParseScene(
		"[Backstage]\n"
		"Actors = [\n"
		"    \"Actor 1\",   # lead\n"
		"\n"
		"    \"Actor 2\"\n"
		"]\n"
		"ExpectedTime = 3:00\n");
	CHECK(s.backstage.actors == std::vector<std::string>{"Actor 1", "Actor 2"});
	CHECK(s.backstage.expectedSeconds == 180);
	CHECK(s.warnings.empty());
}

TEST_CASE("No block, an empty block, and the default name"){
	CHECK_FALSE(ParseScene("[Commands]\ntext \"hi\"\n").backstage.present);
	auto empty = ParseScene("[Backstage]\n[Commands]\ntext \"hi\"\n");
	CHECK(empty.backstage.present);
	CHECK(empty.backstage.name == "Scene");   // the file is Scene.ngk
	CHECK(empty.backstage.expectedSeconds == -1);
}

TEST_CASE("[Backstage] problems become Overview warnings"){
	auto s = ParseScene(
		"[Backstage]\n"
		"Name = Tom & Jerry\n"
		"ExpectedTime = soon\n"
		"Props = [\"chair\"]\n"
		"Actors = [\"A;B\"]\n"
		"just text\n");
	CHECK(HasWarningContaining(s, "shows as HTML"));
	CHECK(HasWarningContaining(s, "ExpectedTime 'soon'"));
	CHECK(HasWarningContaining(s, "Unknown [Backstage] key 'Props'"));
	CHECK(HasWarningContaining(s, "contains ';'"));
	CHECK(HasWarningContaining(s, "expects 'Key = value'"));
	CHECK(s.backstage.expectedSeconds == -1);
}

TEST_CASE("A ticker line: the format from the ticker's docs"){
	BackstageInfo a;
	a.present = true;
	a.name = "Name of current scene";
	a.expectedSeconds = 180;
	a.actors = {"Actor 1", "Actor 2", "Actor 3"};
	CHECK(Backstage::FormatLine(a) == "Name of current scene;3:00;actors;Actor 1;Actor 2;Actor 3");

	a.ninjas = {"Ninja 1"};
	CHECK(Backstage::FormatLine(a) == "Name of current scene;3:00;actors;Actor 1;Actor 2;Actor 3;ninjas;Ninja 1");

	SUBCASE("no actors, no time"){
		BackstageInfo b;
		b.name = "Interlude";
		b.ninjas = {"N"};
		CHECK(Backstage::FormatLine(b) == "Interlude;;ninjas;N");
	}
	SUBCASE("separators and line breaks are removed from every field"){
		BackstageInfo c;
		c.name = "Bad;Name\r\n";
		c.expectedSeconds = 5;
		c.actors = {"A;1", ";", "B"};
		CHECK(Backstage::FormatLine(c) == "BadName;0:05;actors;A1;B");
	}
}

static Scene SceneWith(const char* name, bool block){
	Scene s;
	s.name = name;
	if(block){
		s.backstage.present = true;
		s.backstage.name = name;
	}
	return s;
}

TEST_CASE("The lineup: this scene and the next two with a block, across akter"){
	std::vector<Akt> akts(2);
	akts[0].scenes.push_back(SceneWith("A1", true));
	akts[0].scenes.push_back(SceneWith("AV interlude", false));
	akts[0].scenes.push_back(SceneWith("A3", true));
	akts[1].scenes.push_back(SceneWith("B1", false));
	akts[1].scenes.push_back(SceneWith("B2", true));
	akts[1].scenes.push_back(SceneWith("B3", true));

	auto names = [&](int a, int s){
		std::vector<std::string> out;
		for(auto* info : Backstage::BuildLineup(akts, a, s)) out.push_back(info->name);
		return out;
	};
	using V = std::vector<std::string>;
	CHECK(names(0, 0) == V{"A1", "A3", "B2"});   // skips the interlude and B1
	CHECK(names(0, 2) == V{"A3", "B2", "B3"});   // crosses into akt 2
	CHECK(names(1, 1) == V{"B2", "B3"});         // near the end: fewer lines
	CHECK(names(1, 2) == V{"B3"});
	CHECK(names(0, 1).empty());                  // a scene without a block sends nothing
	CHECK(names(1, 0).empty());
	CHECK(names(5, 0).empty());
	CHECK(names(0, 9).empty());

	CHECK(Backstage::FormatBody(Backstage::BuildLineup(akts, 0, 0)) == "A1;\nA3;\nB2;");
}

TEST_CASE("Backstage Url: scheme, port and the default path"){
	Backstage::Url u;
	REQUIRE(Backstage::ParseUrl("backstage.texnik.dk/set-bck", u));
	CHECK(u.https);
	CHECK(u.host == "backstage.texnik.dk");
	CHECK(u.port == 443);
	CHECK(u.path == "/set-bck");
	CHECK(Backstage::FormatUrl(u) == "https://backstage.texnik.dk/set-bck");

	REQUIRE(Backstage::ParseUrl("http://192.168.1.20:8080/set-bck", u));
	CHECK_FALSE(u.https);
	CHECK(u.host == "192.168.1.20");
	CHECK(u.port == 8080);
	CHECK(Backstage::FormatUrl(u) == "http://192.168.1.20:8080/set-bck");

	REQUIRE(Backstage::ParseUrl("  HTTP://localhost  ", u));
	CHECK(u.port == 80);
	CHECK(u.path == "/set-bck");   // no path: the ticker's endpoint

	REQUIRE(Backstage::ParseUrl("https://example.org/", u));
	CHECK(u.path == "/set-bck");

	for(const char* bad : {"", "https://", "ftp://x/y", "host:0/x", "host:99999", "host:ab/x", "a b/x"}){
		CAPTURE(bad);
		CHECK_FALSE(Backstage::ParseUrl(bad, u));
	}
}

TEST_CASE("schema.toml [Backstage]: Url and Enabled, in any case"){
	TempDir tmp("backstage");
	auto on = SchemaParser::ParseFile(tmp.Write("a.toml",
		"[backstage]\nURL = \"https://backstage.texnik.dk/set-bck\"\n"));
	CHECK(on.backstageUrl == "https://backstage.texnik.dk/set-bck");
	CHECK(on.backstageEnabled);                  // missing Enabled = on

	auto off = SchemaParser::ParseFile(tmp.Write("b.toml",
		"[Backstage]\nenabled = false\nUrl = \"https://x/set-bck\"\n"));
	CHECK_FALSE(off.backstageEnabled);
	CHECK(off.backstageUrl == "https://x/set-bck"); // kept for switching back on

	auto none = SchemaParser::ParseFile(tmp.Write("c.toml", "[Options]\nFontSize = 30\n"));
	CHECK(none.backstageUrl.empty());               // older projects stay off
}

TEST_CASE("New projects: the texnik ticker, switched on; no scene has a block yet"){
	TempDir tmp("backstage-new");
	auto data = Project::Create(RevyType::Other, 2026, "Tester",
		(tmp.Path() / "Proj").string(), "Proj");
	CHECK(data.schema.backstageUrl == "https://backstage.texnik.dk/set-bck");
	CHECK(data.schema.backstageEnabled);
	for(const auto& akt : data.schema.akts){
		for(const auto& scene : akt.scenes){
			CAPTURE(scene.name);
			CHECK_FALSE(scene.backstage.present);   // the example is commented out
		}
	}
}
