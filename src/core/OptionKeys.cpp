#include "core/OptionKeys.hpp"
#include <algorithm>
#include <cctype>

namespace SatyrAV{

namespace{

struct OptionName{
	OptionKey   key;
	const char* documented;
	const char* aliases[3]; // lower-case, nullptr-terminated
};

// Aliases are the spellings used before 2.0.1, kept so existing scenes and
// schemas read exactly as they did. `color` spellings are aliases of the
// `colour` names for the same reason.
const OptionName kNames[] = {
	{OptionKey::FontSize,             "FontSize",             {nullptr}},
	{OptionKey::TextOutline,          "TextOutline",          {nullptr}},
	{OptionKey::Font,                 "Font",                 {nullptr}},
	{OptionKey::FontItalic,           "FontItalic",           {nullptr}},
	{OptionKey::FontBold,             "FontBold",             {nullptr}},
	{OptionKey::FontBoldItalic,       "FontBoldItalic",       {nullptr}},
	{OptionKey::Capitalize,           "Capitalize",           {"cap", nullptr}},
	{OptionKey::AudioFade,            "AudioFade",            {nullptr}},
	{OptionKey::BackgroundColour,     "BackgroundColour",     {"bc", "backgroundcolor", nullptr}},
	{OptionKey::TextColour,           "TextColour",           {"textcolor", nullptr}},
	{OptionKey::SubtitleItalic,       "SubtitleItalic",       {nullptr}},
	{OptionKey::SubtitleColour,       "SubtitleColour",       {"subtitlecolor", nullptr}},
	{OptionKey::SubtitleTransparency, "SubtitleTransparency", {nullptr}},
	{OptionKey::SubtitlePosY,         "SubtitlePosY",         {nullptr}},
	// (2.0.2) New in 2.0.2, so no old spelling; `center` is accepted the way
	// `color` is.
	{OptionKey::FixedTextCentre,      "FixedTextCentre",      {"fixedtextcenter", nullptr}},
};

std::string Lower(const std::string& s){
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });
	return out;
}

std::string Trimmed(const std::string& s){
	size_t a = 0, b = s.size();
	while(a < b && std::isspace((unsigned char)s[a])) a++;
	while(b > a && std::isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

} // namespace

OptionKey LookupOptionKey(const std::string& name){
	const std::string n = Lower(Trimmed(name));
	for(const auto& e : kNames){
		if(n == Lower(e.documented)) return e.key;
		for(const char* const* a = e.aliases; *a; ++a){
			if(n == *a) return e.key;
		}
	}
	return OptionKey::Unknown;
}

bool IsDocumentedOptionName(const std::string& name){
	const std::string n = Lower(Trimmed(name));
	for(const auto& e : kNames){
		if(n == Lower(e.documented)) return true;
	}
	return false;
}

const char* DocumentedOptionName(OptionKey key){
	for(const auto& e : kNames){
		if(e.key == key) return e.documented;
	}
	return "";
}

bool ParseOptionBool(const std::string& value){
	const std::string v = Lower(Trimmed(value));
	return v == "true" || v == "1";
}

} // namespace SatyrAV
