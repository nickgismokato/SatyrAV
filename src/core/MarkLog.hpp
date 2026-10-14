#pragma once
#include "core/Types.hpp"
#include <ctime>
#include <string>

namespace SatyrAV{

// (2.0.1) The mark-line log. Pressing `O` during a show writes the last
// executed cue's file and line to `<project>/fix-it/ErrorInFiles-<date>.LOG`,
// so whoever fixes the scene files after the show knows where to look.
//
// One file per day. Marks from several runs on the same day append to the
// same file, and the file is only created by the first mark of the day, so
// a show with nothing to fix leaves no file behind.

struct MarkedLine{
	std::string fileName;   // e.g. "Example.ngk"
	int lineNumber = 0;     // 1-based; 0 = unknown
	std::string source;     // the line as written in the file, trimmed
};

namespace MarkLog{

// "<project>/fix-it"
std::string FolderFor(const std::string& projectPath);

// Create the fix-it folder if it is missing. True if it exists afterwards.
bool EnsureFolder(const std::string& projectPath);

// "ErrorInFiles-YYYY-MM-DD.LOG"
std::string FileNameFor(const std::tm& date);

// What gets marked for `cmd` in `scene`: the scene's file name, the line
// the command came from, and that line's text as it was when parsed.
MarkedLine MarkFor(const Scene& scene, const Command& cmd);

// One log line, without the newline:
//   14:32:07  Example.ngk  line 42:  text "Hello"
std::string FormatEntry(const std::tm& when, const MarkedLine& mark);

// Append one entry to the day's file, creating the folder and the file as
// needed. On success `outPath`, if given, receives the file's path.
bool Append(const std::string& projectPath, const MarkedLine& mark,
	const std::tm& when, std::string* outPath = nullptr);

// Same, stamped with the current local time.
bool Append(const std::string& projectPath, const MarkedLine& mark,
	std::string* outPath = nullptr);

} // namespace MarkLog

} // namespace SatyrAV
