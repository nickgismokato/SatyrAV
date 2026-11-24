// Per-act subtitles for the recording (core/Subtitles.hpp): how snapshots
// of the slave become text and media cues, and the .srt / .ass they give.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/Subtitles.hpp"
#include <fstream>
#include <sstream>

using namespace SatyrAV;
using SatyrTest::TempDir;

static SubRun R(const std::string& text, bool bold = false, bool italic = false,
	const std::string& font = "DejaVu Sans", int size = 48){
	SubRun r;
	r.text = text; r.bold = bold; r.italic = italic; r.font = font; r.size = size;
	return r;
}

static std::string ReadAll(const std::string& path){
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	std::string s = ss.str();
	// Compare without the platform's line endings.
	std::string out;
	for(char c : s) if(c != '\r') out += c;
	return out;
}

TEST_CASE("Text: one cue per state of the screen, ended by a change or a clear"){
	SubtitleTrack t;
	t.UpdateText(0.0, {});
	t.UpdateText(1.0, {{R("Hello")}});
	t.UpdateText(1.5, {{R("Hello")}});                 // unchanged: no new cue
	t.UpdateText(3.0, {{R("Hello")}, {R("World")}});   // a second line stacks
	t.UpdateText(6.0, {});                              // clear
	t.UpdateText(7.0, {});

	REQUIRE(t.Text().size() == 2);
	CHECK(t.Text()[0].start == 1.0);
	CHECK(t.Text()[0].end == 3.0);
	CHECK(t.Text()[0].lines.size() == 1);
	CHECK(t.Text()[1].start == 3.0);
	CHECK(t.Text()[1].end == 6.0);
	CHECK(t.Text()[1].lines.size() == 2);
}

TEST_CASE("Text: a change of style alone is a new cue"){
	SubtitleTrack t;
	t.UpdateText(0.0, {{R("A")}});
	t.UpdateText(1.0, {{R("A", true)}});
	t.Finish(2.0);
	CHECK(t.Text().size() == 2);
}

TEST_CASE("Media: one long cue per item, from appearing to going"){
	SubtitleTrack t;
	std::vector<std::string> appeared;
	std::vector<MediaCue> gone;

	t.UpdateMedia(0.0, {{1, "[PICTURE] bg.png"}}, &appeared, &gone);
	CHECK(appeared == std::vector<std::string>{"[PICTURE] bg.png"});
	t.UpdateMedia(5.0, {{1, "[PICTURE] bg.png"}, {2, "[VIDEO] intro.mp4"}}, &appeared, &gone);
	CHECK(appeared.size() == 2);
	CHECK(gone.empty());
	t.UpdateMedia(65.0, {{1, "[PICTURE] bg.png"}}, &appeared, &gone); // video ended
	REQUIRE(gone.size() == 1);
	CHECK(gone[0].label == "[VIDEO] intro.mp4");
	CHECK(gone[0].start == 5.0);
	CHECK(gone[0].end == 65.0);

	// The same file shown again is a new item (new id), so a new cue.
	t.UpdateMedia(70.0, {{1, "[PICTURE] bg.png"}, {3, "[VIDEO] intro.mp4"}}, &appeared, &gone);
	CHECK(appeared.back() == "[VIDEO] intro.mp4");

	gone.clear();
	t.Finish(80.0, &gone);
	CHECK(gone.size() == 2);
	CHECK(t.Media().size() == 3);
}

TEST_CASE("Time formats"){
	CHECK(Subtitles::FormatAssTime(0) == "0:00:00.00");
	CHECK(Subtitles::FormatAssTime(-1) == "0:00:00.00");
	CHECK(Subtitles::FormatAssTime(3725.456) == "1:02:05.46");
}

TEST_CASE(".srt: bold and italic as tags, media and text sorted by start"){
	CHECK(Subtitles::SrtLine({R("a "), R("b", true), R(" c", false, true)})
		== "a <b>b</b><i> c</i>");
	CHECK(Subtitles::SrtLine({R("x", true, true)}) == "<b><i>x</i></b>");

	std::vector<TextCue> text{{2.0, 4.0, {{R("Hej")}, {R("med dig")}}}};
	std::vector<MediaCue> media{{1.0, 61.5, "[VIDEO] intro.mp4"}};
	CHECK(Subtitles::FormatSrt(text, media) ==
		"1\n00:00:01,000 --> 00:01:01,500\n[VIDEO] intro.mp4\n\n"
		"2\n00:00:02,000 --> 00:00:04,000\nHej\nmed dig\n\n");
}

TEST_CASE(".ass: fonts and styles as override tags, only where they change"){
	Subtitles::AssStyle st;
	st.font = "DejaVu Sans";
	st.size = 48;

	// Same as the style: no tags.
	CHECK(Subtitles::AssLine({R("plain")}, st) == "plain");
	// Font, size, bold and back again.
	CHECK(Subtitles::AssLine({R("A ", false, false, "Comic", 60), R("B", true, false, "Comic", 60), R(" C")}, st)
		== "{\\fnComic\\fs60}A {\\b1}B{\\fnDejaVu Sans\\fs48\\b0} C");
	// Braces would be read as tags.
	CHECK(Subtitles::AssLine({R("{x}")}, st) == "(x)");

	std::vector<TextCue> text{{1.0, 2.5, {{R("One")}, {R("Two", false, true)}}}};
	const std::string ass = Subtitles::FormatAss(text, st);
	CHECK(ass.find("PlayResY: 1080") != std::string::npos);
	CHECK(ass.find("Style: Default,DejaVu Sans,48,") != std::string::npos);
	CHECK(ass.find("Dialogue: 0,0:00:01.00,0:00:02.50,Default,,0,0,0,,One\\N{\\i1}Two\n")
		!= std::string::npos);
	CHECK(ass.find("[VIDEO]") == std::string::npos);
}

TEST_CASE("Act files: named per act, a re-entered act gets its own pair"){
	TempDir tmp("subs");
	const std::string run = tmp.File("MatRevy2026-2026-12-24-1");
	CHECK(Subtitles::ActBasePath(run, 1) == run + "-Akt1");

	ActSubtitleWriter w;
	REQUIRE(w.Open(Subtitles::ActBasePath(run, 1), Subtitles::AssStyle{}));
	CHECK(Subtitles::ActBasePath(run, 1) == run + "-Akt1-2");
	CHECK(Subtitles::ActBasePath(run, 2) == run + "-Akt2");

	// Files are rewritten as cues end, so they are current during the act.
	w.Update(0.0, {{R("Hej")}}, {{7, "[PICTURE] bg.png"}});
	w.Update(3.0, {}, {{7, "[PICTURE] bg.png"}});
	CHECK(ReadAll(run + "-Akt1.srt") == "1\n00:00:00,000 --> 00:00:03,000\nHej\n\n");

	w.Close(10.0);
	CHECK_FALSE(w.IsOpen());
	CHECK(ReadAll(run + "-Akt1.srt") ==
		"1\n00:00:00,000 --> 00:00:10,000\n[PICTURE] bg.png\n\n"
		"2\n00:00:00,000 --> 00:00:03,000\nHej\n\n");
	const std::string ass = ReadAll(run + "-Akt1.ass");
	CHECK(ass.rfind("\xEF\xBB\xBF[Script Info]", 0) == 0);
	CHECK(ass.find("Hej") != std::string::npos);
	CHECK(ass.find("bg.png") == std::string::npos);
}
