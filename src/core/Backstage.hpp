#pragma once
#include "core/Types.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

// (2.0.2) The backstage screen. When the show moves into a new scene,
// SatyrAV POSTs the current scene and the two after it to the revy's
// backstage ticker (@fysikrevy/backstage-ticker), which shows the actors
// and ninjas due on stage next.
//
// This file is the SDL-free half: reading a scene's `[Backstage]` block,
// choosing which scenes go on the screen, and writing the request body.
// Sending lives in render/BackstageSender.
//
// The ticker's format is one line per scene, fields separated by `;`:
//
//     Name of scene;3:00;actors;Actor 1;Actor 2;ninjas;Ninja 1
//
// Only the first three lines are shown, and the ticker shows the text as
// HTML without escaping it.

namespace Backstage{

// ── [Backstage] in a .ngk ────────────────────────────────────────────────
//
//     [Backstage]
//     Name         = Scene 1
//     ExpectedTime = 2m40s
//     Actors       = ["Actor 1", "Actor 2"]
//     Ninjas       = ["Ninja 1"]
//
// Keys are case-insensitive. Applies one `key = value` line to `info`.
// Returns false and fills `warning` when the line is not understood; a
// line can also succeed with a warning (text the ticker will mangle).
bool ApplyKey(BackstageInfo& info, const std::string& key,
	const std::string& value, std::string& warning);

// ExpectedTime: `2m40s`, `2m`, `40s`, `2:40`, `1:02:03`, or plain seconds
// (`160`). Quotes are allowed. False if it is none of those.
bool ParseExpectedTime(const std::string& text, int& seconds);

// `m:ss` as the ticker expects it (`2:40`, `0:45`, `75:00`); empty for -1.
std::string FormatTime(int seconds);

// A list value: `["A", "B"]`, `['A', 'B']`, `[A, B]` or `A, B`. A quote
// that is never closed runs to the end, so a typo such as `["Ninja 1]`
// still gives `Ninja 1`. Empty entries are dropped.
std::vector<std::string> ParseList(const std::string& text);

// ── What goes on the screen ──────────────────────────────────────────────

// The scenes to show when the show is at (aktIdx, sceneIdx): that scene and
// the next two scenes *with a [Backstage] block*, in show order across
// akter. Empty when the current scene has no block — starting a scene that
// is not on the backstage screen sends nothing.
std::vector<const BackstageInfo*> BuildLineup(const std::vector<Akt>& akts,
	int aktIdx, int sceneIdx);

// One ticker line. `;` and line breaks are removed from every field, since
// they would split it; empty actor / ninja lists are left out.
std::string FormatLine(const BackstageInfo& info);

// The request body: one FormatLine per scene, separated by `\n`.
std::string FormatBody(const std::vector<const BackstageInfo*>& lineup);

// ── [Backstage] Url in schema.toml ───────────────────────────────────────

struct Url{
	bool https = true;
	std::string host;
	int port = 443;
	std::string path = "/set-bck";
};

// `https://host[:port]/path`, `http://…`, or just `host/path`. No scheme
// means https; no path means the ticker's `/set-bck`. False for an empty
// host or a bad port.
bool ParseUrl(const std::string& text, Url& out);

// `https://host/path`, with the port only when it is not the default.
std::string FormatUrl(const Url& url);

} // namespace Backstage
} // namespace SatyrAV
