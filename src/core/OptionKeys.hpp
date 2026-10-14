#pragma once
#include <string>

namespace SatyrAV{

// (2.0.1) The one table of option names, shared by a scene's `[Options]`
// section and `schema.toml`'s `[Options]` table.
//
// Before 2.0.1 the two files spelled the same option differently — `font`
// in a scene but `Font` in the schema, `bc` versus `BackgroundColour`, `cap`
// versus `Capitalize` — and scenes even mixed conventions (`FontSize` next
// to `font`). Now every option has one documented name that works in both
// places, matched without regard to case, and the old spellings keep
// working as silent aliases so no existing file changes meaning.
//
// Being *known* here does not make an option valid everywhere: a scene
// still only honours the options it has always supported (see
// SceneParser), and the rest are project-wide by design.
enum class OptionKey{
	Unknown,
	FontSize,
	TextOutline,
	Font,
	FontItalic,
	FontBold,
	FontBoldItalic,
	Capitalize,
	AudioFade,
	BackgroundColour,
	TextColour,
	SubtitleItalic,
	SubtitleColour,
	SubtitleTransparency,
	SubtitlePosY,
	FixedTextCentre,   // (2.0.2)
};

// Case-insensitive; accepts the documented name and any alias.
OptionKey LookupOptionKey(const std::string& name);

// True when `name` is the documented spelling of its option (in any case)
// rather than an alias. Used to settle a file that gives both: the
// documented name wins.
bool IsDocumentedOptionName(const std::string& name);

// The documented spelling, e.g. "BackgroundColour". Empty for Unknown.
const char* DocumentedOptionName(OptionKey key);

// `true` / `1`, in any case, is true; anything else is false. Matches what
// boolean scene options have always accepted, now without case mattering.
bool ParseOptionBool(const std::string& value);

} // namespace SatyrAV
