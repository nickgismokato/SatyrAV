#include "core/Subtitles.hpp"
#include "core/Platform.hpp"
#include "core/ShowLog.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace SatyrAV{

namespace fs = std::filesystem;

// ── SubtitleTrack ────────────────────────────────────────────

void SubtitleTrack::CloseText(double t){
	if(!textOpen) return;
	textOpen = false;
	if(t > textStart) textCues.push_back({textStart, t, openLines});
	openLines.clear();
}

void SubtitleTrack::UpdateText(double t, const std::vector<SubLine>& lines){
	if(textOpen && lines == openLines) return;
	if(!textOpen && lines.empty()) return;
	CloseText(t);
	if(lines.empty()) return;
	textOpen = true;
	textStart = t;
	openLines = lines;
}

void SubtitleTrack::UpdateMedia(double t, const std::vector<SubMedia>& media,
	std::vector<std::string>* appeared, std::vector<MediaCue>* gone){
	// Gone: open items not in this snapshot.
	for(size_t i = 0; i < openMedia.size();){
		const uint64_t id = openMedia[i].id;
		const bool still = std::any_of(media.begin(), media.end(),
			[&](const SubMedia& m){ return m.id == id; });
		if(still){ i++; continue; }
		const MediaCue cue{openMedia[i].start, t, openMedia[i].label};
		if(t > cue.start) mediaCues.push_back(cue);
		if(gone) gone->push_back(cue);
		openMedia.erase(openMedia.begin() + (long)i);
	}
	// Appeared: snapshot items not open yet.
	for(const auto& m : media){
		const bool known = std::any_of(openMedia.begin(), openMedia.end(),
			[&](const OpenMedia& o){ return o.id == m.id; });
		if(known) continue;
		openMedia.push_back({m.id, m.label, t});
		if(appeared) appeared->push_back(m.label);
	}
}

void SubtitleTrack::Finish(double t, std::vector<MediaCue>* gone){
	CloseText(t);
	UpdateMedia(t, {}, nullptr, gone);
}

// ── Formatting ───────────────────────────────────────────────

namespace Subtitles{

std::string FormatAssTime(double seconds){
	if(!(seconds > 0)) seconds = 0;
	long long cs = (long long)std::llround(seconds * 100.0);
	const int h = (int)(cs / 360000); cs %= 360000;
	const int m = (int)(cs / 6000);   cs %= 6000;
	const int s = (int)(cs / 100);    cs %= 100;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%d:%02d:%02d.%02d", h, m, s, (int)cs);
	return buf;
}

std::string SrtLine(const SubLine& line){
	std::string out;
	for(const auto& r : line){
		std::string piece = r.text;
		if(r.italic) piece = "<i>" + piece + "</i>";
		if(r.bold)   piece = "<b>" + piece + "</b>";
		out += piece;
	}
	return out;
}

static std::string AssEscape(const std::string& s){
	std::string out = s;
	for(char& c : out){
		if(c == '{') c = '(';
		else if(c == '}') c = ')';
	}
	return out;
}

// Override tags for one run, relative to what is in effect. Updates the
// state in place. Shared by every line of one Dialogue, since .ass keeps
// overrides in effect across \N.
struct AssState{ std::string font; int size; bool bold; bool italic; };

static std::string AssTags(const SubRun& r, AssState& st){
	std::string tags;
	if(!r.font.empty() && r.font != st.font){
		tags += "\\fn" + AssEscape(r.font);
		st.font = r.font;
	}
	if(r.size > 0 && r.size != st.size){
		tags += "\\fs" + std::to_string(r.size);
		st.size = r.size;
	}
	if(r.bold != st.bold){
		tags += r.bold ? "\\b1" : "\\b0";
		st.bold = r.bold;
	}
	if(r.italic != st.italic){
		tags += r.italic ? "\\i1" : "\\i0";
		st.italic = r.italic;
	}
	return tags.empty() ? std::string() : "{" + tags + "}";
}

static std::string AssLineWith(const SubLine& line, AssState& st){
	std::string out;
	for(const auto& r : line) out += AssTags(r, st) + AssEscape(r.text);
	return out;
}

std::string AssLine(const SubLine& line, const AssStyle& style){
	AssState st{style.font, style.size, false, false};
	return AssLineWith(line, st);
}

std::string FormatSrt(const std::vector<TextCue>& text,
	const std::vector<MediaCue>& media){
	struct Item{ double start, end; std::string body; int kind; };
	std::vector<Item> items;
	items.reserve(text.size() + media.size());
	// Media first at equal start times: the clip goes up before its text.
	for(const auto& m : media) items.push_back({m.start, m.end, m.label, 0});
	for(const auto& c : text){
		std::string body;
		for(size_t i = 0; i < c.lines.size(); i++){
			if(i) body += '\n';
			body += SrtLine(c.lines[i]);
		}
		items.push_back({c.start, c.end, body, 1});
	}
	std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b){
		if(a.start != b.start) return a.start < b.start;
		return a.kind < b.kind;
	});

	std::string out;
	int n = 0;
	for(const auto& it : items){
		out += std::to_string(++n) + "\n"
			+ ShowLog::FormatSrtTime(it.start) + " --> " + ShowLog::FormatSrtTime(it.end) + "\n"
			+ it.body + "\n\n";
	}
	return out;
}

std::string FormatAss(const std::vector<TextCue>& text, const AssStyle& style){
	std::string out;
	out += "[Script Info]\n";
	out += "; Written by SatyrAV: the text shown on the slave during one act.\n";
	out += "ScriptType: v4.00+\n";
	out += "PlayResX: " + std::to_string(style.playResX) + "\n";
	out += "PlayResY: " + std::to_string(style.playResY) + "\n";
	out += "WrapStyle: 0\n";
	out += "ScaledBorderAndShadow: yes\n\n";

	out += "[V4+ Styles]\n";
	out += "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
		"OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
		"ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, "
		"MarginR, MarginV, Encoding\n";
	// White text, black outline, bottom centre.
	out += "Style: Default," + AssEscape(style.font) + "," + std::to_string(style.size)
		+ ",&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,40,1\n\n";

	out += "[Events]\n";
	out += "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
	for(const auto& c : text){
		AssState st{style.font, style.size, false, false};
		std::string body;
		for(size_t i = 0; i < c.lines.size(); i++){
			if(i) body += "\\N";
			body += AssLineWith(c.lines[i], st);
		}
		out += "Dialogue: 0," + FormatAssTime(c.start) + "," + FormatAssTime(c.end)
			+ ",Default,,0,0,0,," + body + "\n";
	}
	return out;
}

std::string ActBasePath(const std::string& runBasePath, int aktNumber){
	const std::string first = runBasePath + "-Akt" + std::to_string(aktNumber);
	std::error_code ec;
	if(!fs::exists(Platform::Utf8ToPath(first + ".srt"), ec)) return first;
	for(int n = 2; n < 1000; n++){
		const std::string next = first + "-" + std::to_string(n);
		if(!fs::exists(Platform::Utf8ToPath(next + ".srt"), ec)) return next;
	}
	return first + "-1000";
}

} // namespace Subtitles

// ── ActSubtitleWriter ────────────────────────────────────────

ActSubtitleWriter::~ActSubtitleWriter(){
	// Nothing open is lost silently: the destructor has no clock, so the
	// screen closes the writer itself; this only finishes the files.
	if(open) Write();
}

static bool WriteWhole(const std::string& path, const std::string& content){
	// Text mode, so the platform's line endings: both formats open cleanly
	// in Notepad and in video editors.
	std::ofstream f(Platform::Utf8ToPath(path), std::ios::out | std::ios::trunc);
	if(!f.is_open()) return false;
	f << content;
	return (bool)f;
}

bool ActSubtitleWriter::Write(){
	const bool a = WriteWhole(base + ".srt", Subtitles::FormatSrt(track.Text(), track.Media()));
	// UTF-8 BOM: some editors guess the wrong encoding for .ass without it.
	const bool b = WriteWhole(base + ".ass", "\xEF\xBB\xBF" + Subtitles::FormatAss(track.Text(), style));
	writtenText  = track.Text().size();
	writtenMedia = track.Media().size();
	return a && b;
}

bool ActSubtitleWriter::Open(const std::string& basePath, const Subtitles::AssStyle& st){
	open = false;
	base = basePath;
	style = st;
	track = SubtitleTrack();
	if(!Write()) return false;
	open = true;
	return true;
}

void ActSubtitleWriter::Update(double t, const std::vector<SubLine>& lines,
	const std::vector<SubMedia>& media,
	std::vector<std::string>* appeared, std::vector<MediaCue>* gone){
	if(!open) return;
	track.UpdateText(t, lines);
	track.UpdateMedia(t, media, appeared, gone);
	// Only a finished cue changes the files.
	if(track.Text().size() != writtenText || track.Media().size() != writtenMedia){
		Write();
	}
}

void ActSubtitleWriter::Close(double t, std::vector<MediaCue>* gone){
	if(!open) return;
	track.Finish(t, gone);
	Write();
	open = false;
}

} // namespace SatyrAV
