// The project scanner (core/ProjectScanner): every mistake with its file
// and line, and no false alarms on a project as SatyrAV creates it.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/Project.hpp"
#include "core/ProjectScanner.hpp"
#include <algorithm>
#include <filesystem>

using namespace SatyrAV;
using SatyrTest::TempDir;

namespace{

// True if the report has a finding in `file` on `line` whose message
// contains `part`.
bool Has(const ScanReport& r, const std::string& file, int line, const std::string& part,
	ScanFinding::Level level = ScanFinding::Level::Error){
	return std::any_of(r.findings.begin(), r.findings.end(), [&](const ScanFinding& f){
		return f.file == file && f.line == line && f.level == level &&
			f.message.find(part) != std::string::npos;
	});
}

std::string Dump(const ScanReport& r){
	std::string s;
	for(const auto& f : r.findings){
		s += f.file + ":" + std::to_string(f.line) + " "
			+ (f.level == ScanFinding::Level::Error ? "E " : "W ") + f.message + "\n";
	}
	return s;
}

const char* kSchema =
	"[RevyData]\n"                          // 1
	"Revy = \"TestRevy 2026\"\n"            // 2
	"[Options]\n"                           // 3
	"FontSize = \"25\"\n"                   // 4  quoted number
	"Wobble = 3\n"                          // 5  unknown
	"TextColour = \"blurple\"\n"            // 6  bad colour
	"Font = \"Nope.ttf\"\n"                 // 7  missing font
	"Capitalize = true\n"                   // 8  fine
	"[Hotkeys]\n"                           // 9
	"F1 = \"knock.mp3\"\n"                  // 10 exists
	"F2 = \"gone.mp3\"\n"                   // 11 missing
	"[Structure]\n"                         // 12
	"akter = 1\n"                           // 13
	"[Structure.Akt1]\n"                    // 14
	"schema = [\"Intro\", \"Ghost\"]\n";    // 15 Ghost has no file

const char* kIntro =
	"stray line\n"                          // 1  outside any section
	"[Options]\n"                           // 2
	"bc = #102030\n"                        // 3  fine (hex, not a comment)
	"TextColour = nocolour\n"               // 4  bad colour
	"Capitalize = yes\n"                    // 5  not true/false
	"FontSize = big\n"                      // 6  not a number
	"SubtitlePosY = 80\n"                   // 7  project-only
	"Sparkle = 1\n"                         // 8  unknown
	"rain.SPEED = 2\n"                      // 9  particle tunable: fine
	"[Preload]\n"                           // 10
	"bg.png\n"                              // 11 exists
	"lost.mp4\n"                            // 12 missing
	"[commands]\n"                          // 13 wrong case
	"[Commands]\n"                          // 14
	"show bg.png\n"                         // 15 fine
	"show missing.png\n"                    // 16 missing
	"play nothing.mp4\n"                    // 17 missing
	"use NoFont.ttf\n"                      // 18 missing font
	"text \"Hej\"\n"                        // 19
	"flarp 3\n";                            // 20 unknown keyword

} // namespace

TEST_CASE("Scanner: schema.toml mistakes with their lines"){
	TempDir p("scan");
	p.Write("schema.toml", kSchema);
	p.Write("sound/knock.mp3", "x");
	p.Write("pictures/bg.png", "x");
	p.Write("scenes/Intro.ngk", "[Commands]\ntext \"ok\"\n");

	const ScanReport r = ProjectScanner::Scan(p.Str());
	INFO(Dump(r));
	CHECK(Has(r, "schema.toml", 4, "FontSize needs a number"));
	CHECK(Has(r, "schema.toml", 5, "unknown option 'Wobble'", ScanFinding::Level::Warning));
	CHECK(Has(r, "schema.toml", 6, "unknown colour 'blurple'"));
	CHECK(Has(r, "schema.toml", 7, "font file 'Nope.ttf' not found"));
	CHECK(Has(r, "schema.toml", 11, "sound 'gone.mp3' not found"));
	CHECK(Has(r, "schema.toml", 15, "scene 'Ghost' has no file"));
	CHECK_FALSE(Has(r, "schema.toml", 8, ""));
	CHECK_FALSE(Has(r, "schema.toml", 10, ""));
	CHECK(r.filesScanned == 2);
}

TEST_CASE("Scanner: scene mistakes with their lines"){
	TempDir p("scan");
	p.Write("schema.toml", kSchema);
	p.Write("pictures/bg.png", "x");
	p.Write("scenes/Intro.ngk", kIntro);

	const ScanReport r = ProjectScanner::Scan(p.Str());
	INFO(Dump(r));
	const std::string f = "scenes/Intro.ngk";
	CHECK(Has(r, f, 1, "outside any section", ScanFinding::Level::Warning));
	CHECK_FALSE(Has(r, f, 3, ""));
	CHECK(Has(r, f, 4, "unknown colour 'nocolour'"));
	CHECK(Has(r, f, 5, "true or false", ScanFinding::Level::Warning));
	CHECK(Has(r, f, 6, "FontSize needs a number"));
	CHECK(Has(r, f, 7, "only works in schema.toml", ScanFinding::Level::Warning));
	CHECK(Has(r, f, 8, "unknown option 'Sparkle'"));
	CHECK_FALSE(Has(r, f, 9, ""));
	CHECK_FALSE(Has(r, f, 11, ""));
	CHECK(Has(r, f, 12, "[Preload]: file 'lost.mp4' not found"));
	CHECK(Has(r, f, 13, "unknown section [commands] - write it [Commands]"));
	CHECK_FALSE(Has(r, f, 15, ""));
	CHECK(Has(r, f, 16, "show: file 'missing.png' not found"));
	CHECK(Has(r, f, 17, "play: file 'nothing.mp4' not found"));
	CHECK(Has(r, f, 18, "use: font file 'NoFont.ttf' not found"));
	CHECK(Has(r, f, 20, "Unknown keyword: 'flarp'", ScanFinding::Level::Warning));

	// schema.toml first, then the scenes; each file by line.
	REQUIRE_FALSE(r.findings.empty());
	CHECK(r.findings.front().file == "schema.toml");
	for(size_t i = 1; i < r.findings.size(); i++){
		if(r.findings[i].file == r.findings[i - 1].file){
			CHECK(r.findings[i].line >= r.findings[i - 1].line);
		}
	}
	CHECK(r.errors + r.warnings == (int)r.findings.size());
}

TEST_CASE("Scanner: a scene no akt lists is reported and still scanned"){
	TempDir p("scan");
	p.Write("schema.toml", "[Structure]\nakter = 1\n[Structure.Akt1]\nschema = [\"A\"]\n");
	p.Write("scenes/A.ngk", "[Commands]\ntext \"a\"\n");
	p.Write("scenes/Old.ngk", "[Commands]\nshow gone.png\n");

	const ScanReport r = ProjectScanner::Scan(p.Str());
	INFO(Dump(r));
	CHECK(Has(r, "scenes/Old.ngk", 0, "not in any akt", ScanFinding::Level::Warning));
	CHECK(Has(r, "scenes/Old.ngk", 2, "show: file 'gone.png' not found"));
	CHECK(r.filesScanned == 3);
}

TEST_CASE("Scanner: a schema.toml that does not parse"){
	TempDir p("scan");
	p.Write("schema.toml", "[Options]\nFontSize = = 3\n");
	const ScanReport r = ProjectScanner::Scan(p.Str());
	INFO(Dump(r));
	CHECK(Has(r, "schema.toml", 2, "does not parse"));

	TempDir empty("scan");
	CHECK(Has(ProjectScanner::Scan(empty.Str()), "schema.toml", 0, "schema.toml is missing"));
}

TEST_CASE("Scanner: a new project scans clean"){
	TempDir p("scan");
	const std::string path = p.File("MatRevy2026");
	Project::Create(RevyType::MatRevy, 2026, "Tester", path);
	// Create copies the example picture from assets/, found from the
	// working directory. Run from elsewhere (CTest runs in build/tests) it
	// can't, and the scanner rightly reports it missing; stand one in.
	if(!std::filesystem::exists(path + "/pictures/satyr-logo.png")){
		p.Write("MatRevy2026/pictures/satyr-logo.png", "png");
	}
	const ScanReport r = ProjectScanner::Scan(path);
	INFO(Dump(r));
	CHECK(r.Clean());
	CHECK(r.filesScanned > 1);
}

TEST_CASE("Scanner: folders, spaces and [Preload] wildcards"){
	TempDir p("scan");
	p.Write("schema.toml", "[Structure]\nakter = 1\n[Structure.Akt1]\nschema = [\"A\"]\n");
	p.Write("pictures/MEGA/Lyric/Sequence 1/1A.png", "x");
	p.Write("pictures/MEGA/5.png", "x");
	p.Write("scenes/A.ngk",
		"[Preload]\r\n"                          // 1  (CRLF file, as saved on Windows)
		"MEGA/*.png\r\n"                         // 2  matches
		"MEGA/Nothing/*.png\r\n"                 // 3  matches nothing
		"MEGA/Lyric/Sequence 1/1A.png\r\n"       // 4  fine
		"[Commands]\r\n"                         // 5
		"show MEGA/Lyric/Sequence 1/1A.png\r\n"  // 6  fine
		"show mega/5.png\r\n"                    // 7  fine (case)
		"show MEGA/Lyric/Sequence 1/9Z.png\r\n"  // 8  missing
		"show randomImages(MEGA/Lyric/Sequence 1)\r\n"); // 9 fine
	const ScanReport r = ProjectScanner::Scan(p.Str());
	INFO(Dump(r));
	const std::string f = "scenes/A.ngk";
	CHECK_FALSE(Has(r, f, 2, ""));
	CHECK(Has(r, f, 3, "matches no file", ScanFinding::Level::Warning));
	CHECK_FALSE(Has(r, f, 4, ""));
	CHECK_FALSE(Has(r, f, 6, ""));
	CHECK_FALSE(Has(r, f, 7, ""));
	CHECK(Has(r, f, 8, "file 'MEGA/Lyric/Sequence 1/9Z.png' not found"));
	CHECK_FALSE(Has(r, f, 9, ""));
	CHECK(r.findings.size() == 2);
}
