#pragma once
#include <ctime>
#include <fstream>
#include <string>

namespace SatyrAV{

// (2.1.0) The show time log. While a show runs, every scene change, every
// cue that fires and the operator's keys (BACKSPACE, F-keys, K, M, O, U)
// are written with their time of day, so the recording can be lined up
// with the show in post-production.
//
// Two files per run, in `<project>/showlog/`:
//   <Name>-YYYY-MM-DD-N.log  — one line per event, readable as it is:
//       24/12-2026 | 19:30:12.126 | Started MatRevy2026
//       24/12-2026 | 19:30:12.453 | Akt 1 | Scene BandIntro | started scene
//       24/12-2026 | 19:32:59.634 | Akt 1 | Scene TeXIntro | play TeXIntro.mp4
//   <Name>-YYYY-MM-DD-N.srt  — the same events as subtitles, timed from the
//       start of the show, to drop onto the recording in an editor.
//
// <Name> is the revy name with spaces removed ("MatRevy 2026" ->
// "MatRevy2026"); N counts runs on the same day, starting at 1. Nothing is
// created until the first cue runs, so opening a project to look at it
// leaves no files behind.
//
// The .log is flushed line by line, so a crash loses nothing. The .srt
// needs each subtitle's end time, which is the next event's start, so it
// runs one event behind and the last one is written on Close.

struct ShowTime{
	std::tm local{};        // wall-clock date and time of day
	int millis = 0;         // 0..999
	double showSeconds = 0; // since the show log was opened
};

namespace ShowLog{

// "<project>/showlog"
std::string FolderFor(const std::string& projectPath);

// The revy name made safe for a file name: spaces dropped, characters that
// Windows refuses replaced by '_'. Empty becomes "Show".
std::string SafeName(const std::string& revyName);

// "<SafeName>-YYYY-MM-DD-N" (no extension).
std::string BaseNameFor(const std::string& revyName, const std::tm& date, int run);

// The lowest run number >= 1 for which neither the .log nor the .srt
// exists in `folder`.
int NextRunNumber(const std::string& folder, const std::string& revyName,
	const std::tm& date);

// One .log line, without the newline. Akt and scene columns are left out
// when empty.
std::string FormatLogLine(const ShowTime& t, const std::string& akt,
	const std::string& scene, const std::string& event);

// SRT timestamp: "HH:MM:SS,mmm". Negative input is treated as 0.
std::string FormatSrtTime(double seconds);

// Stopwatch style: "M:SS" under an hour, "H:MM:SS" from an hour.
std::string FormatDuration(double seconds);

// The current local time, with milliseconds.
void Now(std::tm& local, int& millis);

} // namespace ShowLog

class ShowLogWriter{
public:
	~ShowLogWriter();

	bool IsOpen() const;

	// Creates the folder and both files for the next free run number.
	// `outBasePath`, if given, receives the path without extension.
	bool Open(const std::string& projectPath, const std::string& revyName,
		const std::tm& date, std::string* outBasePath = nullptr);

	// Writes one event. Ignored when not open.
	void Write(const ShowTime& t, const std::string& akt,
		const std::string& scene, const std::string& event);

	// Writes the last subtitle (ending at `endSeconds`, at most
	// kMaxSubtitleSeconds after it started) and closes both files.
	void Close(double endSeconds);

	// A subtitle lasts until the next event, but no longer than this, so a
	// quiet stretch of the show doesn't keep the last cue on screen.
	static constexpr double kMaxSubtitleSeconds = 5.0;

private:
	void FlushPendingSubtitle(double endSeconds);

	std::ofstream logFile;
	std::ofstream srtFile;
	bool open = false;
	int srtIndex = 0;
	bool havePending = false;
	double pendingStart = 0;
	std::string pendingText;
};

} // namespace SatyrAV
