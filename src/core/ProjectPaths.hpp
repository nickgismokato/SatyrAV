#pragma once
#include <string>

namespace SatyrAV{

// Where a name written in a scene points on disk. One place, used by the
// show (PerformanceScreen) and by the project scanner, so the scanner
// reports a file missing exactly when the show would fail to find it.
namespace ProjectPaths{

// A `show` / `play` / [Preload] / [Hotkeys] name: the project folder
// first, then its `pictures/`, `movies/` and `sound/` folders. Returns
// "<project>/<name>" when nothing exists, so a caller can still name it.
std::string ResolveMedia(const std::string& projectPath, const std::string& name);

// A font as written (`use X.ttf`, the Font options): an absolute path as
// it is; a path with a folder in it against the project; a bare name in
// the project's `fonts/`, `font/`, `FONTS/` and the project folder, then
// the shared FONTS folder from config.toml. Empty when no file exists.
std::string FindFont(const std::string& projectPath, const std::string& name);

// True when `path` is an existing file (UTF-8 path, any platform).
bool IsFile(const std::string& path);
bool IsDirectory(const std::string& path);

} // namespace ProjectPaths

} // namespace SatyrAV
