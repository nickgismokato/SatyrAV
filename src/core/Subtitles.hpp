#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace SatyrAV{

// Subtitles for the recording of a show, one .srt and one .ass per act.
//
// PerformanceScreen looks at the slave every frame and hands over what is
// on it: the text lines (with the style each piece was shown in) and the
// pictures and video. SubtitleTrack turns that stream of snapshots into
// timed cues:
//
//   - Text: one cue per distinct state of the text on screen. `text A`
//     gives "A"; a following `text B` ends that cue and starts "A / B";
//     a clear ends it. Lines that are only spaces are left out.
//   - Pictures and video (.srt only): one long cue per item, from when it
//     appears to when it goes, so the editor can see where to cut in the
//     clip or still. Labelled "[VIDEO] name.mp4" / "[PICTURE] name.png".
//
// Times are seconds from the start of the act. Nothing here touches SDL,
// so the whole thing is unit-tested.

// One piece of a subtitle line, with the style it was shown in.
struct SubRun{
	std::string text;
	bool bold = false;
	bool italic = false;
	std::string font; // font family name, for .ass
	int size = 0;     // font size on the slave, for .ass

	bool operator==(const SubRun& o) const{
		return text == o.text && bold == o.bold && italic == o.italic &&
			font == o.font && size == o.size;
	}
	bool operator!=(const SubRun& o) const{ return !(*this == o); }
};
using SubLine = std::vector<SubRun>;

// A picture or video on the slave.
struct SubMedia{
	uint64_t id = 0;   // the same for as long as this item is on screen
	std::string label; // "[VIDEO] intro.mp4"
};

struct TextCue{
	double start = 0, end = 0;
	std::vector<SubLine> lines;
};

struct MediaCue{
	double start = 0, end = 0;
	std::string label;
};

class SubtitleTrack{
public:
	// The text on screen at time `t`. Ends the open cue and starts a new
	// one if it differs from last time.
	void UpdateText(double t, const std::vector<SubLine>& lines);

	// The pictures and video on screen at time `t`. Items not seen before
	// are added to `appeared` (labels); items gone since last time are
	// closed and added to `gone`. Either may be null.
	void UpdateMedia(double t, const std::vector<SubMedia>& media,
		std::vector<std::string>* appeared = nullptr,
		std::vector<MediaCue>* gone = nullptr);

	// Ends every open cue at `t`. Items still open are added to `gone`.
	void Finish(double t, std::vector<MediaCue>* gone = nullptr);

	// Finished cues, in the order they ended (text) / were closed (media).
	const std::vector<TextCue>&  Text()  const{ return textCues; }
	const std::vector<MediaCue>& Media() const{ return mediaCues; }

private:
	bool textOpen = false;
	double textStart = 0;
	std::vector<SubLine> openLines;

	struct OpenMedia{ uint64_t id; std::string label; double start; };
	std::vector<OpenMedia> openMedia;

	std::vector<TextCue>  textCues;
	std::vector<MediaCue> mediaCues;

	void CloseText(double t);
};

namespace Subtitles{

// The .ass header's look. Play resolution is the slave canvas, so `size`
// is the same number as the slave's font size.
struct AssStyle{
	int playResX = 1920;
	int playResY = 1080;
	std::string font = "DejaVu Sans";
	int size = 48;
};

// "H:MM:SS.cc" (centiseconds), as .ass wants it. Negative is 0.
std::string FormatAssTime(double seconds);

// A line as .srt text: bold / italic as <b> / <i>.
std::string SrtLine(const SubLine& line);

// A line as .ass text: font, size, bold and italic as override tags,
// only where they differ from what is already in effect. `{` and `}` in
// the text become `(` and `)`, since .ass would read them as tags.
std::string AssLine(const SubLine& line, const AssStyle& style);

// The whole .srt: text and media cues together, sorted by start time and
// numbered from 1.
std::string FormatSrt(const std::vector<TextCue>& text,
	const std::vector<MediaCue>& media);

// The whole .ass: header plus one Dialogue per text cue. No media cues.
std::string FormatAss(const std::vector<TextCue>& text, const AssStyle& style);

// "<dir>/<base>-Akt<N>" — with "-2", "-3", … added if a .srt by that name
// already exists (the act was entered again in the same run).
std::string ActBasePath(const std::string& runBasePath, int aktNumber);

} // namespace Subtitles

// One act's .srt and .ass. Both files are rewritten whenever a cue ends,
// so a crash loses only what was on screen at that moment.
class ActSubtitleWriter{
public:
	~ActSubtitleWriter();

	// `basePath` without extension. Creates both files (empty cue lists).
	bool Open(const std::string& basePath, const Subtitles::AssStyle& style);
	bool IsOpen() const{ return open; }
	const std::string& BasePath() const{ return base; }

	// One snapshot of the slave at act time `t`. See SubtitleTrack.
	void Update(double t, const std::vector<SubLine>& lines,
		const std::vector<SubMedia>& media,
		std::vector<std::string>* appeared = nullptr,
		std::vector<MediaCue>* gone = nullptr);

	// Ends every open cue at `t`, writes the files a last time and closes.
	void Close(double t, std::vector<MediaCue>* gone = nullptr);

private:
	bool open = false;
	std::string base;
	Subtitles::AssStyle style;
	SubtitleTrack track;
	size_t writtenText = 0, writtenMedia = 0;
	bool Write();
};

} // namespace SatyrAV
