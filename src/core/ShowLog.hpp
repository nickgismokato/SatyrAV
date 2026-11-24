#pragma once
#include <ctime>
#include <fstream>
#include <string>

namespace SatyrAV{

// The show time log. While a show runs, everything that happens is written
// with its time of day, so the recording can be lined up with the show in
// post-production: acts and scenes starting and ending, every cue, the
// operator's keys, and pictures and video appearing and going.
//
// In `<project>/showlog/`, one per run:
//   <Name>-YYYY-MM-DD-N.log  — one line per event, readable as it is:
//       24/12-2026 | 19:30:12.126 | Started MatRevy2026
//       24/12-2026 | 19:30:12.453 | Akt 1 | Scene BandIntro | started scene
//       24/12-2026 | 19:32:59.634 | Akt 1 | Scene TeXIntro | play TeXIntro.mp4
// Next to it, one .srt and one .ass per act (see core/Subtitles.hpp):
//   <Name>-YYYY-MM-DD-N-Akt1.srt / .ass
//
// <Name> is the revy name with spaces removed ("MatRevy 2026" ->
// "MatRevy2026"); N counts runs on the same day, starting at 1. Nothing is
// created until the first cue runs, and nothing at all unless *Show Log*
// is on in Options.
//
// The .log is flushed line by line, so a crash loses nothing.

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

// The lowest run number >= 1 for which no .log exists in `folder`.
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

	// Creates the folder and the .log for the next free run number.
	// `outBasePath`, if given, receives the path without extension.
	bool Open(const std::string& projectPath, const std::string& revyName,
		const std::tm& date, std::string* outBasePath = nullptr);

	// Writes one event. Ignored when not open.
	void Write(const ShowTime& t, const std::string& akt,
		const std::string& scene, const std::string& event);

	void Close();

private:
	std::ofstream logFile;
	bool open = false;
};

} // namespace SatyrAV
