// (1.8.1) Project — creation, generation and load/save.
//
// This file is the regression net for what 1.8.0 shipped: a generated
// schema.toml that lists every option at its default, and four tutorial
// scenes that must parse clean. Both are things a user only discovers are
// broken when they create a new project, which makes them exactly the
// kind of thing worth pinning down here.
//
// Deliberately not covered: the copy of `satyr-logo-body.png` into
// pictures/. It depends on FindAsset locating the repo's assets/ folder
// relative to the working directory, so asserting on it would make the
// suite pass or fail based on where it was launched from. The scenes are
// instead checked to reference nothing *but* that one file, which is the
// property that actually matters.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/Project.hpp"
#include "core/SchemaParser.hpp"
#include "core/SceneParser.hpp"

#include <algorithm>
#include <functional>
#include <set>

using namespace SatyrAV;
using SatyrTest::TempDir;

namespace fs = std::filesystem;

static ProjectData CreateIn(const TempDir& tmp,
	RevyType type = RevyType::MatRevy,
	int year = 2026,
	const std::string& creator = "Nick",
	const std::string& revyName = "MatRevy"){
	return Project::Create(type, year, creator,
		(tmp.Path() / "MyRevy").string(), revyName);
}

// The four scenes the 1.8.0 generator writes, in schema order.
static const char* kSceneNames[] = {
	"TextShowcase", "PictureShowcase", "ParticleShowcase", "MacroShowcase"
};

// ── Directory scaffolding ────────────────────────────────────

TEST_CASE("Create lays out the project directory"){
	TempDir tmp("create");
	auto data = CreateIn(tmp);
	const fs::path root = tmp.Path() / "MyRevy";

	CHECK(fs::is_directory(root));
	CHECK(fs::is_directory(root / "scenes"));
	CHECK(fs::is_directory(root / "movies"));
	CHECK(fs::is_directory(root / "sound"));
	CHECK(fs::is_directory(root / "pictures"));
	CHECK(fs::is_regular_file(root / "schema.toml"));
	CHECK(data.projectPath == root.string());
}

TEST_CASE("Create writes exactly the four generated scenes"){
	TempDir tmp("scenes");
	CreateIn(tmp);
	const fs::path scenes = tmp.Path() / "MyRevy" / "scenes";

	for(const char* name : kSceneNames){
		CAPTURE(name);
		CHECK(fs::is_regular_file(scenes / (std::string(name) + ".ngk")));
	}

	int count = 0;
	for(const auto& e : fs::directory_iterator(scenes)) { (void)e; count++; }
	CHECK(count == 4);
}

TEST_CASE("Create is safe to run twice over the same path"){
	// The New Project screen can be re-entered; nothing may throw.
	TempDir tmp("twice");
	CreateIn(tmp);
	auto second = CreateIn(tmp);
	CHECK(second.schema.akts.size() == 2);
}

// ── Generated schema.toml ────────────────────────────────────

TEST_CASE("Generated schema.toml is valid and carries the project identity"){
	TempDir tmp("schema");
	CreateIn(tmp, RevyType::KemiRevy, 2027, "Ada", "KemiRevyJubi");
	auto s = SchemaParser::ParseFile(
		(tmp.Path() / "MyRevy" / "schema.toml").string());

	CHECK(s.revyName    == "KemiRevyJubi 2027");
	CHECK(s.creatorName == "Ada");
	CHECK(s.year        == 2027);
}

TEST_CASE("Create falls back to the enum name when no display name is given"){
	TempDir tmp("fallback");
	auto data = Project::Create(RevyType::FysikRevy, 2026, "Nick",
		(tmp.Path() / "P").string()); // revyName defaulted
	CHECK(data.schema.revyName == std::string(RevyTypeName(RevyType::FysikRevy))
		+ " 2026");
}

TEST_CASE("An empty creator name still produces a parseable schema"){
	TempDir tmp("nocreator");
	auto data = CreateIn(tmp, RevyType::SaTyR, 2026, "", "SaTyR");
	CHECK(data.schema.creatorName.empty());
	CHECK(data.schema.akts.size() == 2);
}

TEST_CASE("(1.8.4) A generated project starts at a projector-sized font"){
	// The parser's fallback of 25 suits a desktop window and is far too
	// small on a projected slave. New projects therefore start at 60. This
	// is the single deliberate divergence from "generated == built-in
	// default" — see the companion invariant test below.
	TempDir tmp("fontsize");
	CreateIn(tmp);
	auto generated = SchemaParser::ParseFile(
		(tmp.Path() / "MyRevy" / "schema.toml").string());

	CHECK(generated.fontSize == 60);

	SUBCASE("the built-in fallback is untouched, so old projects don't jump"){
		// A schema with no FontSize key at all must still resolve to 25;
		// changing the fallback would silently resize every project that
		// predates the key being written out.
		const RevySchema builtin;
		CHECK(builtin.fontSize == 25);
	}
}

TEST_CASE("(1.8.0) Every generated option equals its built-in default"){
	// The point of the 1.8.0 change: the generated file spells out every
	// option, and spelling it out must not change any behaviour. So a
	// generated schema and a completely empty one have to resolve to the
	// same values for every option field.
	//
	// `FontSize` is the one deliberate exception (1.8.4) and is asserted
	// on its own above; everything else must still match exactly.
	TempDir tmp("defaults");
	CreateIn(tmp);
	auto generated = SchemaParser::ParseFile(
		(tmp.Path() / "MyRevy" / "schema.toml").string());

	const RevySchema builtin; // defaults straight from Types.hpp

	CHECK(generated.textOutline == builtin.textOutline);
	CHECK(generated.capitalize  == builtin.capitalize);
	CHECK(generated.fontPath           == builtin.fontPath);
	CHECK(generated.fontPathItalic     == builtin.fontPathItalic);
	CHECK(generated.fontPathBold       == builtin.fontPathBold);
	CHECK(generated.fontPathBoldItalic == builtin.fontPathBoldItalic);

	CHECK((int)generated.backgroundColor.r == (int)builtin.backgroundColor.r);
	CHECK((int)generated.backgroundColor.g == (int)builtin.backgroundColor.g);
	CHECK((int)generated.backgroundColor.b == (int)builtin.backgroundColor.b);

	CHECK(generated.subtitleItalic == builtin.subtitleItalic);
	CHECK((int)generated.subtitleColour.r == (int)builtin.subtitleColour.r);
	CHECK((int)generated.subtitleColour.g == (int)builtin.subtitleColour.g);
	CHECK((int)generated.subtitleColour.b == (int)builtin.subtitleColour.b);
	CHECK(generated.subtitleTransparency
		== doctest::Approx(builtin.subtitleTransparency));
	CHECK(generated.subtitlePosY == doctest::Approx(builtin.subtitlePosY));

	// [Display] all zero is the "fill the physical display" sentinel — a
	// generated project must not arrive pre-letterboxed.
	CHECK(generated.targetedDisplay.width    == builtin.targetedDisplay.width);
	CHECK(generated.targetedDisplay.height   == builtin.targetedDisplay.height);
	CHECK(generated.targetedDisplay.centerX  == builtin.targetedDisplay.centerX);
	CHECK(generated.targetedDisplay.centerY  == builtin.targetedDisplay.centerY);
	CHECK(generated.targetedDisplay.rotation
		== doctest::Approx(builtin.targetedDisplay.rotation));

	// All twelve hotkey slots are written as empty strings, which the
	// parser drops — so nothing is actually bound.
	CHECK(generated.functionKeyAudio.empty());

	// The particle tables ship commented out, so no tuning is stored and
	// the cascade keeps falling through to the built-in multipliers.
	CHECK(generated.particleTunings.empty());
}

TEST_CASE("(1.8.0) Generated schema declares two akter with four scenes"){
	TempDir tmp("structure");
	CreateIn(tmp);
	auto s = SchemaParser::ParseFile(
		(tmp.Path() / "MyRevy" / "schema.toml").string());

	REQUIRE(s.aktCount == 2);
	REQUIRE(s.akts.size() == 2);
	REQUIRE(s.akts[0].sceneNames.size() == 2);
	REQUIRE(s.akts[1].sceneNames.size() == 2);

	CHECK(s.akts[0].sceneNames[0] == kSceneNames[0]);
	CHECK(s.akts[0].sceneNames[1] == kSceneNames[1]);
	CHECK(s.akts[1].sceneNames[0] == kSceneNames[2]);
	CHECK(s.akts[1].sceneNames[1] == kSceneNames[3]);
}

TEST_CASE("Every scene named in the schema exists on disk"){
	// A name in the schema with no matching file loads as an empty scene
	// and silently disappears from the navigator — the exact failure the
	// 1.8.0 generator must never produce.
	TempDir tmp("consistency");
	auto data = CreateIn(tmp);
	for(const auto& akt : data.schema.akts){
		for(const auto& name : akt.sceneNames){
			CAPTURE(name);
			CHECK(fs::is_regular_file(
				tmp.Path() / "MyRevy" / "scenes" / (name + ".ngk")));
		}
	}
}

// ── Generated scenes ─────────────────────────────────────────

TEST_CASE("(1.8.0) Every generated scene parses without warnings"){
	// A typo in a generated scene would greet every new user with a
	// warning in the Overview Debug popup.
	TempDir tmp("parse");
	CreateIn(tmp);

	for(const char* name : kSceneNames){
		CAPTURE(name);
		auto scene = SceneParser::ParseFile(
			(tmp.Path() / "MyRevy" / "scenes" / (std::string(name) + ".ngk"))
				.string());
		CHECK(scene.name == name);
		CHECK(scene.commands.size() > 0);
		for(const auto& w : scene.warnings){
			CAPTURE(w.lineNumber);
			CAPTURE(w.message);
			CHECK_MESSAGE(false, "generated scene raised a parser warning");
		}
		CHECK(scene.warnings.empty());
	}
}

TEST_CASE("(1.8.0) Generated scenes reference no asset but satyr-logo.png"){
	// A fresh project has exactly one picture in it. If a generated scene
	// pointed anywhere else, the tutorial would show nothing.
	TempDir tmp("assets");
	CreateIn(tmp);

	std::set<std::string> referenced;
	std::function<void(const Command&)> walk = [&](const Command& c){
		if(c.type == CommandType::Show || c.type == CommandType::Play){
			if(!c.argument.empty()) referenced.insert(c.argument);
		}
		for(const auto& p : c.mods.randomImagePaths) referenced.insert(p);
		for(const auto& sub : c.subCommands) walk(sub);
		for(const auto& sub : c.loopBody)    walk(sub);
	};

	for(const char* name : kSceneNames){
		auto scene = SceneParser::ParseFile(
			(tmp.Path() / "MyRevy" / "scenes" / (std::string(name) + ".ngk"))
				.string());
		for(const auto& c : scene.commands) walk(c);
	}

	REQUIRE_FALSE(referenced.empty());
	for(const auto& r : referenced){
		CAPTURE(r);
		CHECK(r == "satyr-logo.png");
	}
}

TEST_CASE("(1.8.0) Generated scenes exercise the layers they advertise"){
	TempDir tmp("coverage");
	CreateIn(tmp);
	auto load = [&](const char* name){
		return SceneParser::ParseFile(
			(tmp.Path() / "MyRevy" / "scenes" / (std::string(name) + ".ngk"))
				.string());
	};
	auto hasType = [](const Scene& s, CommandType t){
		return std::any_of(s.commands.begin(), s.commands.end(),
			[&](const Command& c){ return c.type == t; });
	};

	SUBCASE("TextShowcase covers the text layer"){
		auto s = load("TextShowcase");
		CHECK(hasType(s, CommandType::Text));
		CHECK(hasType(s, CommandType::TextBf));
		CHECK(hasType(s, CommandType::TextIt));
		CHECK(hasType(s, CommandType::TextCont));
		CHECK(hasType(s, CommandType::TextD));
		CHECK(hasType(s, CommandType::Countdown));
		CHECK(hasType(s, CommandType::ClearCountdown));
		CHECK(hasType(s, CommandType::Comment));
	}
	SUBCASE("PictureShowcase covers the image layer"){
		auto s = load("PictureShowcase");
		CHECK(hasType(s, CommandType::Show));
		CHECK(hasType(s, CommandType::ClearImages));
		CHECK(hasType(s, CommandType::Compound)); // the & example
		CHECK(s.preloadPaths.size() == 1);
		CHECK(s.preloadPaths[0] == "satyr-logo.png");

		bool sawSize = false, sawMove = false, sawSpin = false, sawTrans = false;
		for(const auto& c : s.commands){
			if(c.mods.width > 0)              sawSize  = true;
			if(c.mods.hasMove)                sawMove  = true;
			if(c.mods.spinDegPerSec != 0.0f)  sawSpin  = true;
			if(c.mods.transparency < 1.0f)    sawTrans = true;
		}
		CHECK(sawSize);
		CHECK(sawMove);
		CHECK(sawSpin);
		CHECK(sawTrans);
	}
	SUBCASE("ParticleShowcase covers every motion model"){
		auto s = load("ParticleShowcase");
		std::set<int> types;
		for(const auto& c : s.commands){
			if(c.mods.particleType != ParticleType::None){
				types.insert((int)c.mods.particleType);
			}
		}
		CHECK(types.size() >= 6);
		CHECK(hasType(s, CommandType::StopParticleCont));
		// The scene's own [Options] tuning must have been read.
		CHECK(s.particleTunings.count((int)ParticleType::Snow) == 1);

		bool sawTrail = false, sawBounce = false;
		for(const auto& c : s.commands){
			if(c.mods.trailLength > 0) sawTrail  = true;
			if(c.mods.bounceEdges)     sawBounce = true;
		}
		CHECK(sawTrail);
		CHECK(sawBounce);
	}
	SUBCASE("MacroShowcase covers grouping and blocks"){
		auto s = load("MacroShowcase");
		CHECK(hasType(s, CommandType::Compound));  // run{}
		CHECK(hasType(s, CommandType::ClearText));
		CHECK(hasType(s, CommandType::ClearAll));

		// The [Macro] bodies were expanded — a mistyped macro name would
		// have been skipped silently, leaving nothing behind.
		bool sawGrouped = false;
		for(const auto& c : s.commands){
			if(!c.groupName.empty()) sawGrouped = true;
		}
		CHECK(sawGrouped);

		// `clear Backdrop` must name the group the block declares.
		bool sawNamedClear = false;
		std::set<std::string> groups;
		for(const auto& c : s.commands){
			if(!c.groupName.empty()) groups.insert(c.groupName);
			if(c.type == CommandType::Clear && !c.argument.empty()){
				sawNamedClear = true;
				CHECK(groups.count(c.argument) == 1);
			}
		}
		CHECK(sawNamedClear);
	}
}

// ── Load ─────────────────────────────────────────────────────

TEST_CASE("Load populates every akt with its parsed scenes"){
	TempDir tmp("load");
	CreateIn(tmp);
	auto data = Project::Load((tmp.Path() / "MyRevy").string());

	REQUIRE(data.schema.akts.size() == 2);
	for(const auto& akt : data.schema.akts){
		CAPTURE(akt.number);
		CHECK(akt.scenes.size() == akt.sceneNames.size());
		for(size_t i = 0; i < akt.scenes.size(); i++){
			CHECK(akt.scenes[i].name == akt.sceneNames[i]);
			CHECK(akt.scenes[i].commands.size() > 0);
		}
	}
	CHECK(data.creatorName == "Nick");
	CHECK(data.year == 2026);
}

TEST_CASE("Load of a directory with no schema returns an empty project"){
	TempDir tmp("noschema");
	auto data = Project::Load(tmp.Str());
	CHECK(data.schema.akts.empty());
	CHECK(data.projectPath == tmp.Str());
}

TEST_CASE("A scene named in the schema but missing on disk loads as empty"){
	// Documents the failure mode the Overview Debug popup exists to catch:
	// the akt keeps its slot rather than the project refusing to open.
	TempDir tmp("missingscene");
	const fs::path root = tmp.Path() / "P";
	fs::create_directories(root / "scenes");
	tmp.Write("P/schema.toml",
		"[RevyData]\nRevy = \"X 2026\"\n"
		"[Structure]\nakter = 1\n[Structure.Akt1]\nschema = [ \"Ghost\" ]\n");

	auto data = Project::Load(root.string());
	REQUIRE(data.schema.akts.size() == 1);
	REQUIRE(data.schema.akts[0].scenes.size() == 1);
	CHECK(data.schema.akts[0].scenes[0].name == "Ghost");
	CHECK(data.schema.akts[0].scenes[0].commands.empty());
}

// ── Save ─────────────────────────────────────────────────────

TEST_CASE("Save writes the display rect back and preserves the rest"){
	TempDir tmp("save");
	auto data = CreateIn(tmp);

	data.schema.targetedDisplay.width    = 1920;
	data.schema.targetedDisplay.height   = 1080;
	data.schema.targetedDisplay.centerX  = 960;
	data.schema.targetedDisplay.centerY  = 540;
	data.schema.targetedDisplay.rotation = 7.5f;
	Project::Save(data);

	auto reread = SchemaParser::ParseFile(
		(tmp.Path() / "MyRevy" / "schema.toml").string());

	CHECK(reread.targetedDisplay.width    == 1920);
	CHECK(reread.targetedDisplay.height   == 1080);
	CHECK(reread.targetedDisplay.centerX  == 960);
	CHECK(reread.targetedDisplay.centerY  == 540);
	CHECK(reread.targetedDisplay.rotation == doctest::Approx(7.5f));

	// Everything Save does not manage must survive the round-trip.
	CHECK(reread.revyName == "MatRevy 2026");
	CHECK(reread.creatorName == "Nick");
	CHECK(reread.fontSize == 60);
	CHECK(reread.textOutline == 4);
	CHECK(reread.subtitlePosY == doctest::Approx(90.0f));
	REQUIRE(reread.akts.size() == 2);
	CHECK(reread.akts[0].sceneNames[0] == kSceneNames[0]);
	CHECK(reread.akts[1].sceneNames[1] == kSceneNames[3]);
}

TEST_CASE("Save is a no-op when there is no schema to update"){
	TempDir tmp("nosave");
	ProjectData data;
	data.projectPath = tmp.Str();
	Project::Save(data); // must not throw or create a file
	CHECK_FALSE(fs::exists(tmp.Path() / "schema.toml"));
}

TEST_CASE("Save survives a hand-corrupted schema without destroying it"){
	// Project::Save reparses before emitting. A schema the user broke by
	// hand must be left alone rather than overwritten with a stub.
	TempDir tmp("corrupt");
	const fs::path root = tmp.Path() / "P";
	fs::create_directories(root);
	const std::string broken = "[Options\nFontSize = = 30\n";
	tmp.Write("P/schema.toml", broken);

	ProjectData data;
	data.projectPath = root.string();
	data.schema.targetedDisplay.width = 800;
	Project::Save(data);

	std::ifstream in((root / "schema.toml").string(), std::ios::binary);
	std::string after((std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	CHECK(after == broken);
}

TEST_CASE("Save then Load round-trips a rect through the real parser"){
	TempDir tmp("roundtrip");
	auto data = CreateIn(tmp);
	data.schema.targetedDisplay.width  = 1280;
	data.schema.targetedDisplay.height = 720;
	Project::Save(data);

	auto reloaded = Project::Load((tmp.Path() / "MyRevy").string());
	CHECK(reloaded.schema.targetedDisplay.width  == 1280);
	CHECK(reloaded.schema.targetedDisplay.height == 720);
	CHECK(reloaded.schema.akts.size() == 2);
}
