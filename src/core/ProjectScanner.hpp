#pragma once
#include <string>
#include <vector>

namespace SatyrAV{

// The project scanner: reads a project from disk and reports every mistake
// it can find before the show does, each with its file and line.
//
// It reads the files fresh, so it sees edits made since the project was
// loaded (no `U` needed), and it runs only when asked — the Scanner popup
// calls it when it opens — so it costs nothing during a show.
//
// What it checks:
//   schema.toml  — that it parses; scenes listed in the akter exist as
//                  .ngk files; [Hotkeys] sounds and the Font options exist;
//                  [Options] names are known.
//   scenes/*.ngk — everything the parser warns about (unknown keywords,
//                  [Animation], [Backstage], [Cue], `use` / `fontSize`
//                  mistakes); unknown sections and lines outside any
//                  section; [Options] names, values (colours, numbers,
//                  true/false) and project-only options; files named by
//                  `show`, `play`, randomImages, [Preload], [Animation] and
//                  fonts named by `use` and the Font options.
//                  A .ngk in scenes/ that no akt lists is reported too.
//
// No SDL, so it is unit-tested (tests/test_project_scanner.cpp).

struct ScanFinding{
	enum class Level{ Error, Warning };
	Level level = Level::Error;
	std::string file;   // relative to the project: "scenes/Intro.ngk"
	int line = 0;       // 1-based; 0 = the file as a whole
	std::string message;
};

struct ScanReport{
	std::vector<ScanFinding> findings; // by file (schema.toml first), then line
	int filesScanned = 0;
	int errors = 0;
	int warnings = 0;
	bool Clean() const{ return findings.empty(); }
};

namespace ProjectScanner{

ScanReport Scan(const std::string& projectPath);

} // namespace ProjectScanner

} // namespace SatyrAV
