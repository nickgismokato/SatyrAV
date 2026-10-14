// (2.0.1) The mark-line log behind the `O` key.
//
// What is pinned: the file and folder names the AV team will look for, one
// file per day that later runs append to, no file until the first mark, and
// a mark that names the right file, line and text — including for a cue in
// a run block or a macro.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/MarkLog.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

using namespace SatyrAV;
using SatyrTest::TempDir;
using SatyrTest::ParseScene;
namespace fs = std::filesystem;

static std::tm Day(int y, int mo, int d, int h = 20, int mi = 15, int s = 3){
	std::tm t{};
	t.tm_year = y - 1900;
	t.tm_mon  = mo - 1;
	t.tm_mday = d;
	t.tm_hour = h;
	t.tm_min  = mi;
	t.tm_sec  = s;
	return t;
}

static std::vector<std::string> ReadLines(const fs::path& p){
	std::vector<std::string> lines;
	std::ifstream in(p);
	std::string l;
	while(std::getline(in, l)){
		if(!l.empty() && l.back() == '\r') l.pop_back();
		lines.push_back(l);
	}
	return lines;
}

TEST_CASE("Mark log names: fix-it folder, one ErrorInFiles file per day"){
	CHECK(MarkLog::FolderFor("/shows/MyRevy") == "/shows/MyRevy/fix-it");
	CHECK(MarkLog::FolderFor("/shows/MyRevy/") == "/shows/MyRevy/fix-it");
	CHECK(MarkLog::FileNameFor(Day(2026, 3, 7)) == "ErrorInFiles-2026-03-07.LOG");
	CHECK(MarkLog::FileNameFor(Day(2026, 11, 24)) == "ErrorInFiles-2026-11-24.LOG");
}

TEST_CASE("Mark log entry: time, file, line and the code"){
	MarkedLine m{"Example.ngk", 42, "text \"Hello\""};
	CHECK(MarkLog::FormatEntry(Day(2026, 3, 7, 9, 5, 1), m)
		== "09:05:01  Example.ngk  line 42:  text \"Hello\"");
	MarkedLine unknown;
	CHECK(MarkLog::FormatEntry(Day(2026, 3, 7, 9, 5, 1), unknown)
		== "09:05:01  (unknown file)  line ?:  ");
}

TEST_CASE("The folder is made on demand; the file only by the first mark"){
	TempDir tmp("marklog");
	const std::string project = tmp.Path().string();
	const fs::path folder = tmp.Path() / "fix-it";

	REQUIRE(MarkLog::EnsureFolder(project));
	CHECK(fs::is_directory(folder));
	CHECK(fs::is_empty(folder));          // opening a project writes no file
	CHECK(MarkLog::EnsureFolder(project)); // already there is fine

	std::string written;
	REQUIRE(MarkLog::Append(project, {"A.ngk", 3, "text \"one\""},
		Day(2026, 3, 7, 20, 0, 0), &written));
	const fs::path day1 = folder / "ErrorInFiles-2026-03-07.LOG";
	CHECK(fs::path(written) == day1);
	CHECK(ReadLines(day1) == std::vector<std::string>{
		"20:00:00  A.ngk  line 3:  text \"one\""});
}

TEST_CASE("Marks on the same day append, also across restarts; a new day starts a new file"){
	TempDir tmp("marklog-days");
	const std::string project = tmp.Path().string();
	const fs::path folder = tmp.Path() / "fix-it";

	// No EnsureFolder first: a deleted fix-it folder is recreated by the mark.
	REQUIRE(MarkLog::Append(project, {"A.ngk", 3, "one"}, Day(2026, 3, 7, 20, 0, 0)));
	// A restart is just another Append against the same file.
	REQUIRE(MarkLog::Append(project, {"B.ngk", 9, "two"}, Day(2026, 3, 7, 22, 30, 0)));
	REQUIRE(MarkLog::Append(project, {"A.ngk", 4, "three"}, Day(2026, 3, 8, 0, 10, 0)));

	CHECK(ReadLines(folder / "ErrorInFiles-2026-03-07.LOG") == std::vector<std::string>{
		"20:00:00  A.ngk  line 3:  one",
		"22:30:00  B.ngk  line 9:  two"});
	CHECK(ReadLines(folder / "ErrorInFiles-2026-03-08.LOG") == std::vector<std::string>{
		"00:10:00  A.ngk  line 4:  three"});
}

TEST_CASE("A mark names the command's own file, line and text"){
	auto scene = ParseScene(
		"[Options]\n"                       // 1
		"FontSize = 30\n"                   // 2
		"\n"                                // 3
		"[Commands]\n"                      // 4
		"text \"Hello\"   # greet\n"        // 5
		"   play intro.mp4\n"               // 6
		"run{\n"                            // 7
		"\ttext \"a\"\n"                    // 8
		"}\n");                             // 9
	REQUIRE(scene.commands.size() == 3);

	auto m0 = MarkLog::MarkFor(scene, scene.commands[0]);
	CHECK(m0.fileName == "Scene.ngk");
	CHECK(m0.lineNumber == 5);
	CHECK(m0.source == "text \"Hello\"   # greet"); // the line as written

	auto m1 = MarkLog::MarkFor(scene, scene.commands[1]);
	CHECK(m1.lineNumber == 6);
	CHECK(m1.source == "play intro.mp4");

	// A run block fires as one cue: the mark points at its `run` line.
	auto m2 = MarkLog::MarkFor(scene, scene.commands[2]);
	CHECK(m2.lineNumber == 7);
	CHECK(m2.source == "run{");
}

TEST_CASE("A mark survives Windows line endings and out-of-range lines"){
	// The Windows build reads scene files in text mode, so a CRLF line
	// arrives without its \r; a stray \r or tab must not reach the log anyway.
	Scene scene;
	scene.fileName = "Scene.ngk";
	scene.sourceLines = {"[Commands]\r", "\ttext \"Hi\"\r"};
	Command hi;
	hi.lineNumber = 2;
	CHECK(MarkLog::MarkFor(scene, hi).source == "text \"Hi\"");

	Command stray;
	stray.lineNumber = 99;
	auto m = MarkLog::MarkFor(scene, stray);
	CHECK(m.lineNumber == 99);
	CHECK(m.source.empty());
}
