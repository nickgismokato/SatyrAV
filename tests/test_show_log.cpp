// The show time log: file naming, run numbering and the .log line format.
// The per-act subtitles are in test_subtitles.cpp.
//
// Pinned here: one numbered .log per run and day, and the .log line
// matches the TODO's example layout.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/ShowLog.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace SatyrAV;
using SatyrTest::TempDir;

static std::tm Date(int y, int mo, int d, int h = 0, int mi = 0, int s = 0){
	std::tm t{};
	t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
	t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
	return t;
}

static ShowTime At(int h, int mi, int s, int ms, double showSec){
	ShowTime t;
	t.local = Date(2026, 12, 24, h, mi, s);
	t.millis = ms;
	t.showSeconds = showSec;
	return t;
}

static std::string ReadAll(const std::string& path){
	std::ifstream f(path);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

TEST_CASE("SafeName drops spaces and replaces characters Windows refuses"){
	CHECK(ShowLog::SafeName("MatRevy 2026") == "MatRevy2026");
	CHECK(ShowLog::SafeName("A/B:C*D?") == "A_B_C_D_");
	CHECK(ShowLog::SafeName("  ") == "Show");
	CHECK(ShowLog::SafeName("Kemi\xC3\xA6Revy") == "Kemi\xC3\xA6Revy"); // æ kept
}

TEST_CASE("Base name: name, date and run number"){
	CHECK(ShowLog::BaseNameFor("MatRevy 2026", Date(2026, 12, 24), 1) == "MatRevy2026-2026-12-24-1");
	CHECK(ShowLog::BaseNameFor("X", Date(2027, 1, 5), 12) == "X-2027-01-05-12");
}

TEST_CASE("Log line matches the documented layout"){
	CHECK(ShowLog::FormatLogLine(At(19, 30, 12, 126, 0), "", "", "Started MatRevy2026")
		== "24/12-2026 | 19:30:12.126 | Started MatRevy2026");
	CHECK(ShowLog::FormatLogLine(At(19, 32, 59, 634, 0), "Akt 1", "Scene TeXIntro", "play TeXIntro.mp4")
		== "24/12-2026 | 19:32:59.634 | Akt 1 | Scene TeXIntro | play TeXIntro.mp4");
}

TEST_CASE("SRT and stopwatch times"){
	CHECK(ShowLog::FormatSrtTime(0) == "00:00:00,000");
	CHECK(ShowLog::FormatSrtTime(-3) == "00:00:00,000");
	CHECK(ShowLog::FormatSrtTime(3725.0405) == "01:02:05,041");
	CHECK(ShowLog::FormatDuration(0) == "0:00");
	CHECK(ShowLog::FormatDuration(252.9) == "4:12");
	CHECK(ShowLog::FormatDuration(3725) == "1:02:05");
}

TEST_CASE("Each run on the same day gets the next number"){
	TempDir tmp("showlog");
	const std::string project = tmp.Path().string();
	const std::tm day = Date(2026, 12, 24);

	std::string first, second;
	{
		ShowLogWriter w;
		REQUIRE(w.Open(project, "MatRevy 2026", day, &first));
		w.Close();
	}
	{
		ShowLogWriter w;
		REQUIRE(w.Open(project, "MatRevy 2026", day, &second));
		w.Close();
	}
	CHECK(first.size() > 2);
	CHECK(first.substr(first.size() - 2) == "-1");
	CHECK(second.substr(second.size() - 2) == "-2");
	CHECK(ShowLog::NextRunNumber(ShowLog::FolderFor(project), "MatRevy 2026", day) == 3);
	// Another day starts again at 1.
	CHECK(ShowLog::NextRunNumber(ShowLog::FolderFor(project), "MatRevy 2026", Date(2026, 12, 25)) == 1);
}

TEST_CASE("The .log holds one line per event"){
	TempDir tmp("showlog");
	std::string base;
	ShowLogWriter w;
	REQUIRE(w.Open(tmp.Path().string(), "MatRevy 2026", Date(2026, 12, 24), &base));
	CHECK(w.IsOpen());

	w.Write(At(19, 30, 0, 0, 0.0),   "", "", "Started MatRevy2026");
	w.Write(At(19, 30, 1, 500, 1.5), "Akt 1", "Scene BandIntro", "started scene");
	w.Write(At(19, 30, 21, 500, 21.5), "Akt 1", "Scene BandIntro", "show logo.png");
	w.Close();
	CHECK_FALSE(w.IsOpen());

	const std::string log = ReadAll(base + ".log");
	CHECK(log ==
		"24/12-2026 | 19:30:00.000 | Started MatRevy2026\n"
		"24/12-2026 | 19:30:01.500 | Akt 1 | Scene BandIntro | started scene\n"
		"24/12-2026 | 19:30:21.500 | Akt 1 | Scene BandIntro | show logo.png\n");
	// The subtitles are per act now, written by ActSubtitleWriter.
	CHECK_FALSE(std::filesystem::exists(base + ".srt"));
}

TEST_CASE("Writing to a closed log does nothing"){
	ShowLogWriter w;
	w.Write(At(1, 2, 3, 4, 5), "a", "b", "c");
	w.Close();
	CHECK_FALSE(w.IsOpen());
}
