#pragma once
#include <string>
#include <vector>

namespace SatyrAV{

// Where a name written in a scene points on disk. One place, used by the
// show (PerformanceScreen) and by the project scanner, so the scanner
// reports a file missing exactly when the show would fail to find it.
namespace ProjectPaths{

// A `show` / `play` / [Preload] / [Hotkeys] name: the project folder
// first, then its `pictures/`, `movies/` and `sound/` folders. A name may
// have folders in it (`MEGA/Lyric/Sequence 1/1A.png`, spaces and all),
// written with `/` or `\`; folder and file names that differ only in
// case are found too, as on Windows. An absolute path is used as it is.
// Returns "<project>/<name>" when nothing exists, so a caller can still
// name it. Paths are joined with `/` so the same file always gives the
// same string — the media cache is keyed on it.
std::string ResolveMedia(const std::string& projectPath, const std::string& name);

// `\` -> `/`, and a leading `./` dropped.
std::string Normalise(const std::string& name);
bool IsAbsolute(const std::string& name);
// The folders a media name is looked for in, in order.
std::vector<std::string> MediaRoots(const std::string& projectPath);

// True when a name has `*` or `?` in it.
bool HasWildcard(const std::string& name);
// Every file a pattern matches, in each media root, sorted. `*` is any
// run of characters and `?` one character, within one name; `**` is any
// number of folders (`MEGA/**/*.png`: every PNG under MEGA). Case is
// ignored.
std::vector<std::string> ExpandWildcard(const std::string& projectPath, const std::string& pattern);
// [Preload] entries to files: patterns expanded, names resolved.
std::vector<std::string> ExpandPreload(const std::string& projectPath,
	const std::vector<std::string>& entries);

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
