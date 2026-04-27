#include "render/SlaveWindow.hpp"
#include <SDL2/SDL.h>
#include <cmath>
#include <algorithm>

namespace SatyrAV{

namespace{
	// Resolve a side sentinel to a top-left pixel position just off the
	// targeted rect, so the content is fully hidden before animation.
	// N = above (y < 0), S = below, E = right, W = left.
	// Vertical/horizontal centering is applied on the perpendicular axis.
	void SideToPixel(int side, int contentW, int contentH,
		int targetW, int targetH, int& outX, int& outY){
		int cx = (targetW - contentW) / 2;
		int cy = (targetH - contentH) / 2;
		switch(side){
			case 1: outX = cx;             outY = -contentH;          return; // N
			case 2: outX = cx;             outY = targetH;            return; // S
			case 3: outX = targetW;        outY = cy;                 return; // E
			case 4: outX = -contentW;      outY = cy;                 return; // W
			default: outX = cx;            outY = cy;                 return;
		}
	}
}

void SlaveWindow::Clear(Renderer& r, Colour bg){
	r.ClearSlave(bg);
}

// Resolve a run's effective text colour: the per-run colour (set by
// color(...) on this segment) takes precedence, then the entry-level
// mods.textColour (set by a legacy color(...) wrapping the whole line),
// then the default white. Alpha 0 = "not set".
static Colour ResolveRunColour(const TextRun& run, const RenderModifiers& mods){
	if(run.colour.a > 0) return run.colour;
	if(mods.textColour.a > 0) return mods.textColour;
	return Colours::WHITE;
}

// (1.6.4) Slave word-wrap. Splits each run into whitespace / non-whitespace
// tokens and greedily fills lines up to `maxWidth`. Wraps only on word
// boundaries — a single word longer than `maxWidth` is placed on its own
// line and is allowed to visually overflow rather than break mid-word
// (per the project's wrapping policy). Returns at least one line so the
// rest of the draw path can iterate uniformly. The returned runs preserve
// per-run style flags so bold/italic/colour follow the wrap boundary.
namespace{
	struct WrapToken{
		TextRun base; // style carrier; .text is unused
		std::string text;
		bool isSpace = false;
		int width = 0;
	};

	std::vector<WrapToken> TokenizeForWrap(
		const std::vector<TextRun>& runs, TextRenderer& text){
		std::vector<WrapToken> toks;
		for(const auto& r : runs){
			const std::string& s = r.text;
			size_t i = 0;
			while(i < s.size()){
				bool sp = (s[i] == ' ' || s[i] == '\t');
				size_t j = i + 1;
				while(j < s.size()){
					bool sp2 = (s[j] == ' ' || s[j] == '\t');
					if(sp2 != sp) break;
					j++;
				}
				WrapToken t;
				t.base = r;
				t.text = s.substr(i, j - i);
				t.isSpace = sp;
				int tw = 0, th = 0;
				text.MeasureSlave(t.text, tw, th, r.bold, r.italic);
				t.width = tw;
				toks.push_back(std::move(t));
				i = j;
			}
		}
		return toks;
	}

	// Coalesce a token onto the line's last run when style matches; otherwise
	// open a fresh run. Coalescing keeps the run vector compact, which makes
	// the per-line draw walk cheaper and avoids spurious style boundaries
	// inside a continuous segment.
	static bool RunStyleMatches(const TextRun& a, const TextRun& b){
		return a.bold == b.bold && a.italic == b.italic &&
			a.colour.r == b.colour.r && a.colour.g == b.colour.g &&
			a.colour.b == b.colour.b && a.colour.a == b.colour.a &&
			a.transparency == b.transparency;
	}

	std::vector<std::vector<TextRun>> WrapEntryLines(
		const std::vector<TextRun>& runs, int maxWidth, TextRenderer& text){
		std::vector<std::vector<TextRun>> lines;
		if(runs.empty() || maxWidth <= 0){
			lines.push_back(runs);
			return lines;
		}
		auto toks = TokenizeForWrap(runs, text);
		std::vector<TextRun> cur;
		int curW = 0;

		auto append = [&](const WrapToken& t){
			if(!cur.empty() && RunStyleMatches(cur.back(), t.base)){
				cur.back().text += t.text;
			} else{
				TextRun r = t.base;
				r.text = t.text;
				cur.push_back(std::move(r));
			}
		};

		for(const auto& t : toks){
			// Drop leading whitespace on an empty line — the gap was the
			// reason we wrapped, so reproducing it on the next line would
			// inset the wrapped text by one space-width.
			if(cur.empty() && t.isSpace) continue;

			if(curW + t.width <= maxWidth){
				append(t);
				curW += t.width;
				continue;
			}

			// Token doesn't fit. Flush the current line if it's not empty.
			if(!cur.empty()){
				lines.push_back(std::move(cur));
				cur.clear();
				curW = 0;
			}

			if(t.isSpace){
				// Whitespace at a break point is consumed.
				continue;
			}

			// Non-space token starting a fresh line. Even if it's wider than
			// maxWidth we place it as-is — wrap policy is word-boundary only,
			// so an overlong single word visually overflows rather than
			// being chopped mid-word.
			append(t);
			curW = t.width;
		}
		if(!cur.empty()) lines.push_back(std::move(cur));
		if(lines.empty()) lines.push_back({});
		return lines;
	}

	// Per-line measurement: width = sum of run widths (each measured with
	// its own style face), height = max run height. Mirrors the inline
	// loop the draw path uses to advance the cursor.
	void MeasureLine(const std::vector<TextRun>& runs, TextRenderer& text,
		int& outW, int& outH, std::vector<int>& runWidthsOut){
		outW = 0; outH = 0;
		runWidthsOut.assign(runs.size(), 0);
		for(size_t k = 0; k < runs.size(); k++){
			int rw = 0, rh = 0;
			text.MeasureSlave(runs[k].text, rw, rh, runs[k].bold, runs[k].italic);
			runWidthsOut[k] = rw;
			outW += rw;
			if(rh > outH) outH = rh;
		}
	}
} // namespace

void SlaveWindow::DrawText(Renderer& r, TextRenderer& text){
	auto* renderer = r.GetSlave();
	if(!renderer) return;
	// Text lives inside the targeted rect — use target dims for layout.
	int slaveW = r.GetTargetWidth();
	int slaveH = r.GetTargetHeight();
	int slaveLineH = text.GetSlaveLineHeight();

	// (1.6.4) Word-wrap budget: the targeted-display width minus a small
	// horizontal margin on each side so glyphs don't run flush to the
	// edge. 2.5% per side ≈ 5% total. Lines wider than this are split on
	// word boundaries; single words wider than the budget are kept intact
	// and allowed to overflow (no mid-word breaks).
	int wrapMargin = std::max(20, slaveW / 40);
	int wrapWidth = slaveW - 2 * wrapMargin;
	if(wrapWidth < 1) wrapWidth = slaveW; // pathological narrow target

	// ── Pre-pass: wrap every entry into visual lines and measure each. ──
	// `entryLines[i]` is the wrapped run-list per visual line for entry i;
	// `lineWidths[i][k]` and `runWidthsPerLine[i][k]` cache the metrics so
	// the layout and draw passes don't re-measure.
	std::vector<std::vector<std::vector<TextRun>>> entryLines(textEntries.size());
	std::vector<std::vector<int>> lineWidths(textEntries.size());
	std::vector<std::vector<int>> lineHeights(textEntries.size());
	std::vector<std::vector<std::vector<int>>> runWidthsPerLine(textEntries.size());
	for(size_t i = 0; i < textEntries.size(); i++){
		auto& entry = textEntries[i];
		if(entry.runs.empty()){
			entryLines[i].push_back({});
			lineWidths[i].push_back(0);
			lineHeights[i].push_back(slaveLineH);
			runWidthsPerLine[i].push_back({});
			continue;
		}
		entryLines[i] = WrapEntryLines(entry.runs, wrapWidth, text);
		for(auto& line : entryLines[i]){
			int lw = 0, lh = 0;
			std::vector<int> rws;
			MeasureLine(line, text, lw, lh, rws);
			lineWidths[i].push_back(lw);
			lineHeights[i].push_back(lh > 0 ? lh : slaveLineH);
			runWidthsPerLine[i].push_back(std::move(rws));
		}
	}

	// Total visual lines occupied by default-positioned, non-animated text.
	// Animated and subtitle entries float independently and don't take
	// slots in the centred stack. With wrap, one entry can contribute
	// several lines here.
	int defaultLineCount = 0;
	for(size_t i = 0; i < textEntries.size(); i++){
		auto& entry = textEntries[i];
		bool animated = entry.mods.hasMove;
		if(!entry.mods.HasPosition() && !animated && !entry.subtitleAnchor){
			defaultLineCount += (int)entryLines[i].size();
		}
	}
	int totalH = defaultLineCount * slaveLineH;
	int defaultY = slaveH / 2 - totalH / 2;

	// (1.6.1, 1.6.4) Subtitle bottom-Y assignment, wrap-aware. Each
	// subtitle entry can wrap to multiple visual lines; the stack is
	// flattened so the most-recently-pushed entry's *last* line sits at
	// the anchor and every earlier line (within or before that entry) is
	// one slaveLineH higher. The anchor is taken from the LAST subtitle's
	// `mods.posY` so a project-options change picks up without requiring
	// a clearText.
	std::vector<std::vector<int>> subtitleBottomY(textEntries.size());
	for(size_t i = 0; i < textEntries.size(); i++){
		subtitleBottomY[i].assign(entryLines[i].size(), 0);
	}
	{
		std::vector<size_t> subIdx;
		subIdx.reserve(textEntries.size());
		for(size_t i = 0; i < textEntries.size(); i++){
			if(textEntries[i].subtitleAnchor) subIdx.push_back(i);
		}
		if(!subIdx.empty()){
			const auto& last = textEntries[subIdx.back()];
			float anchorPct = last.mods.HasPosition() ? last.mods.posY : 90.0f;
			int anchorY = (int)(anchorPct / 100.0f * (float)slaveH);
			int rank = 0;
			for(int j = (int)subIdx.size() - 1; j >= 0; j--){
				size_t ei = subIdx[j];
				auto& bv = subtitleBottomY[ei];
				for(int li = (int)bv.size() - 1; li >= 0; li--){
					bv[li] = anchorY - rank * slaveLineH;
					rank++;
				}
			}
		}
	}

	for(size_t entryIdx = 0; entryIdx < textEntries.size(); entryIdx++){
		auto& entry = textEntries[entryIdx];
		if(entry.runs.empty()) continue;

		const auto& lines = entryLines[entryIdx];
		const auto& widths = lineWidths[entryIdx];
		const auto& heights = lineHeights[entryIdx];
		const auto& runWs = runWidthsPerLine[entryIdx];
		if(lines.empty()) continue;

		int blockMaxW = 0;
		for(int w : widths) if(w > blockMaxW) blockMaxW = w;
		int blockH = (int)lines.size() * slaveLineH;

		// ── Compute rotation: static rotate() + accumulated spin(). ──
		float rotation = entry.mods.rotation;
		if(entry.mods.spinDegPerSec != 0.0f){
			entry.currentSpin += entry.mods.spinDegPerSec * lastDt;
			rotation += entry.currentSpin;
		}

		// ── Compute the entry's anchor position. drawX/drawY denote the
		// top-left of the entry's bounding block; per-line draw positions
		// are derived from this anchor below. ──
		int blockX = 0, blockY = 0;
		// "centerEachLine" controls whether each visual line is centred
		// around blockX (block-centre) or left-aligned at blockX.
		bool centerEachLine = false;

		if(entry.mods.hasMove && entry.mods.moveDurationSec > 0.0f){
			float now = SDL_GetTicks() / 1000.0f;
			float elapsed = now - entry.spawnTimeSec;
			float t = std::clamp(elapsed / entry.mods.moveDurationSec, 0.0f, 1.0f);

			int fromX, fromY, toX, toY;
			if(entry.mods.moveFromSide != 0){
				SideToPixel(entry.mods.moveFromSide, blockMaxW, blockH, slaveW, slaveH, fromX, fromY);
			} else{
				fromX = (int)(entry.mods.moveFromX / 100.0f * (float)slaveW);
				fromY = (int)(entry.mods.moveFromY / 100.0f * (float)slaveH);
			}
			if(entry.mods.moveToSide != 0){
				SideToPixel(entry.mods.moveToSide, blockMaxW, blockH, slaveW, slaveH, toX, toY);
			} else{
				toX = (int)(entry.mods.moveToX / 100.0f * (float)slaveW);
				toY = (int)(entry.mods.moveToY / 100.0f * (float)slaveH);
			}
			blockX = (int)((1.0f - t) * (float)fromX + t * (float)toX);
			blockY = (int)((1.0f - t) * (float)fromY + t * (float)toY);
		} else if(entry.subtitleAnchor){
			// Subtitle: each visual line is centred on the canvas, with
			// per-line bottom-Y already computed above.
			centerEachLine = true;
			blockX = slaveW / 2;
			blockY = 0; // unused — vertical comes from subtitleBottomY
		} else if(entry.mods.HasPosition()){
			blockX = (int)(entry.mods.posX / 100.0f * (float)slaveW);
			blockY = (int)(entry.mods.posY / 100.0f * (float)slaveH);
		} else{
			// Default: centred stack. Each visual line is centred around
			// the canvas centre; vertical advances per line.
			centerEachLine = true;
			blockX = slaveW / 2;
			blockY = defaultY;
			defaultY += blockH;
		}

		for(size_t li = 0; li < lines.size(); li++){
			const auto& lineRuns = lines[li];
			if(lineRuns.empty()) continue;
			int lineW = widths[li];
			int lineH = heights[li];

			int lineX, lineY;
			if(entry.subtitleAnchor){
				// Subtitle lines centre on canvas; vertical from per-line
				// bottom anchor minus this line's height.
				lineX = slaveW / 2 - lineW / 2;
				lineY = subtitleBottomY[entryIdx][li] - lineH;
			} else if(centerEachLine){
				lineX = blockX - lineW / 2;
				lineY = blockY + (int)li * slaveLineH;
			} else{
				lineX = blockX;
				lineY = blockY + (int)li * slaveLineH;
			}

			int cursorX = lineX;
			for(size_t k = 0; k < lineRuns.size(); k++){
				const TextRun& run = lineRuns[k];
				Colour c = ResolveRunColour(run, entry.mods);

				if(rotation != 0.0f){
					text.DrawTextSlaveRotated(renderer, run.text, cursorX, lineY, c,
						rotation, run.bold, run.italic);
				} else{
					text.DrawTextSlave(renderer, run.text, cursorX, lineY, c,
						run.bold, run.italic);
				}
				cursorX += runWs[li][k];
			}
		}
	}
}

void SlaveWindow::DrawMedia(Renderer& r, MediaPlayer& media){
	auto* renderer = r.GetSlave();
	if(!renderer) return;
	// Media (video/image) is laid out inside the targeted rect.
	int slaveW = r.GetTargetWidth();
	int slaveH = r.GetTargetHeight();

	// Video takes full screen (behind text). Use HasVideo() (not
	// IsVideoPlaying()) so a paused video keeps presenting its last
	// decoded frame on `K` instead of disappearing.
	if(media.HasVideo()){
		auto* tex = media.GetVideoTexture();
		if(tex){
			RenderModifiers noMods;
			RenderTextureFitted(renderer, tex,
				media.GetVideoWidth(), media.GetVideoHeight(),
				slaveW, slaveH, noMods);
		}
	}

	// All active images, painted in the order they were pushed. Grouped
	// shows accumulate; ungrouped shows replaced any previous ungrouped
	// image at push-time.
	for(auto& img : imageEntries){
		// Animated GIF: advance the current frame by wall time. The frame
		// delay vector is aligned 1:1 with `frames`, so we roll forward as
		// long as the elapsed delta exceeds the current frame's delay.
		SDL_Texture* drawTex = img.tex;
		if(!img.frames.empty()){
			img.frameElapsedSec += lastDt;
			while(img.frameIdx < (int)img.frameDelaysMs.size()){
				float frameSec = (float)img.frameDelaysMs[img.frameIdx] / 1000.0f;
				if(frameSec <= 0.0f) frameSec = 0.1f; // paranoia
				if(img.frameElapsedSec < frameSec) break;
				img.frameElapsedSec -= frameSec;
				img.frameIdx = (img.frameIdx + 1) % (int)img.frames.size();
			}
			drawTex = img.frames[img.frameIdx];
		}
		if(!drawTex) continue;
		RenderTextureFitted(renderer, drawTex,
			img.w, img.h, slaveW, slaveH, img.mods,
			img.spawnTimeSec, &img.currentSpin);
	}
}

void SlaveWindow::DrawParticles(SDL_Renderer* renderer, float dt){
	for(auto& p : particleEntries){
		p.system.Update(dt);
		p.system.Draw(renderer);
	}
	// (1.4) Garbage-collect smooth-stopping systems that have drained
	// their last live particle. Update() flips them inactive when the
	// last one dies; we drop the entry here to keep the list tidy.
	particleEntries.erase(
		std::remove_if(particleEntries.begin(), particleEntries.end(),
			[](const SlaveParticleEntry& e){ return !e.system.IsActive(); }),
		particleEntries.end());
}

void SlaveWindow::Update(Renderer& r, MediaPlayer& media, float dt){
	lastDt = dt;
	Clear(r, bgColour);
	media.UpdateVideoFrame(r.GetSlave());
	DrawMedia(r, media);
	if(r.GetSlave()) DrawParticles(r.GetSlave(), dt);
}

// Apply capitalisation (if the flag is set) to a single run's text in place.
static void ApplyCapitalize(std::string& s, bool cap){
	if(!cap) return;
	std::transform(s.begin(), s.end(), s.begin(), ::toupper);
}

void SlaveWindow::PushText(const std::string& line, const RenderModifiers& mods,
	const std::string& groupName){
	// Wrap the single-string call as a single-run entry so it flows through
	// the same code path as the `+` concatenation variant.
	TextRun run;
	run.text = line;
	// Pre-apply the entry's textColour to the run so ResolveRunColour has
	// just one place to look; callers using the legacy single-string API
	// may set colour via mods.textColour.
	if(mods.textColour.a > 0) run.colour = mods.textColour;
	run.transparency = mods.transparency;
	PushTextRuns({run}, mods, groupName);
}

void SlaveWindow::PushTextRuns(const std::vector<TextRun>& runs,
	const RenderModifiers& mods, const std::string& groupName){
	float now = SDL_GetTicks() / 1000.0f;

	// Split the collective run text on literal `\n` markers, producing one
	// SlaveTextEntry per output line. A `\n` can fall inside a run; when
	// that happens the run is split and its style is carried to both halves.
	// The common path (no `\n` in any run) just emits one entry with every
	// run intact.
	auto emit = [&](std::vector<TextRun> lineRuns){
		if(lineRuns.empty()) return;
		for(auto& r : lineRuns) ApplyCapitalize(r.text, capitalizeText);
		SlaveTextEntry entry;
		entry.runs = std::move(lineRuns);
		entry.mods = mods;
		entry.spawnTimeSec = now;
		entry.groupName = groupName;
		textEntries.push_back(std::move(entry));
	};

	std::vector<TextRun> current;
	for(const auto& run : runs){
		std::string remaining = run.text;
		size_t pos;
		while((pos = remaining.find("\\n")) != std::string::npos){
			TextRun head = run;
			head.text = remaining.substr(0, pos);
			if(!head.text.empty()) current.push_back(std::move(head));
			emit(std::move(current));
			current.clear();
			remaining = remaining.substr(pos + 2);
		}
		if(!remaining.empty()){
			TextRun tail = run;
			tail.text = std::move(remaining);
			current.push_back(std::move(tail));
		}
	}
	if(!current.empty()) emit(std::move(current));
}

void SlaveWindow::PushSubtitleRuns(const std::vector<TextRun>& runs,
	const RenderModifiers& mods, const std::string& groupName){
	// Reuses the PushTextRuns plumbing for capitalize/`\n`-splitting/spawn
	// time, then flips `subtitleAnchor` on every emitted entry produced by
	// the call. Marking the entries afterwards is simpler than threading a
	// subtitle flag through emit/PushTextRuns and keeps them in lockstep.
	size_t before = textEntries.size();
	PushTextRuns(runs, mods, groupName);
	for(size_t i = before; i < textEntries.size(); i++){
		textEntries[i].subtitleAnchor = true;
	}
}

void SlaveWindow::AppendToLastText(const std::vector<TextRun>& runs,
	const RenderModifiers& mods, const std::string& groupName){
	// `textCont` semantics: extend the most recently pushed text entry with
	// the given runs so they render on the same line. If there is no prior
	// entry (or if the last one was already flushed by a `clear`), fall
	// back to pushing a fresh entry so the content still shows up.
	if(textEntries.empty()){
		PushTextRuns(runs, mods, groupName);
		return;
	}

	// Honour `\n` inside the appended runs: the part before the first \n
	// joins the last entry; anything after becomes fresh entries.
	std::vector<TextRun> firstLine;
	std::vector<TextRun> overflow;
	bool overflowing = false;
	for(const auto& run : runs){
		if(overflowing){
			overflow.push_back(run);
			continue;
		}
		std::string remaining = run.text;
		size_t pos = remaining.find("\\n");
		if(pos == std::string::npos){
			TextRun copy = run;
			ApplyCapitalize(copy.text, capitalizeText);
			firstLine.push_back(std::move(copy));
		} else{
			TextRun head = run;
			head.text = remaining.substr(0, pos);
			if(!head.text.empty()){
				ApplyCapitalize(head.text, capitalizeText);
				firstLine.push_back(std::move(head));
			}
			TextRun tail = run;
			tail.text = remaining.substr(pos + 2);
			if(!tail.text.empty()) overflow.push_back(std::move(tail));
			overflowing = true;
		}
	}

	SlaveTextEntry& last = textEntries.back();
	for(auto& r : firstLine) last.runs.push_back(std::move(r));

	if(!overflow.empty()) PushTextRuns(overflow, mods, groupName);
}

void SlaveWindow::ClearText(){
	// Drop only ungrouped entries — grouped text survives plain `clear`.
	textEntries.erase(
		std::remove_if(textEntries.begin(), textEntries.end(),
			[](const SlaveTextEntry& e){ return e.groupName.empty(); }),
		textEntries.end());
}

void SlaveWindow::ClearTextAll(){
	textEntries.clear();
}

void SlaveWindow::SetBackgroundColour(Colour c){
	bgColour = c;
}

void SlaveWindow::SetCapitalize(bool cap){
	capitalizeText = cap;
}

void SlaveWindow::ShowImage(SDL_Texture* tex, int w, int h,
	const RenderModifiers& mods, const std::string& groupName){
	// Ungrouped shows replace any existing ungrouped image so the old
	// single-image semantics are preserved. Grouped shows always append.
	if(groupName.empty()){
		imageEntries.erase(
			std::remove_if(imageEntries.begin(), imageEntries.end(),
				[](const SlaveImageEntry& e){ return e.groupName.empty(); }),
			imageEntries.end());
	}

	SlaveImageEntry entry;
	entry.tex = tex; // Not owned — comes from cache or MediaPlayer
	entry.w = w;
	entry.h = h;
	entry.mods = mods;
	entry.spawnTimeSec = SDL_GetTicks() / 1000.0f;
	entry.currentSpin = 0.0f;
	entry.groupName = groupName;
	imageEntries.push_back(entry);
}

void SlaveWindow::ShowAnimation(const std::vector<SDL_Texture*>& frames,
	const std::vector<int>& delaysMs, int w, int h,
	const RenderModifiers& mods, const std::string& groupName){
	// Ungrouped-replaces-ungrouped rule, identical to ShowImage.
	if(groupName.empty()){
		imageEntries.erase(
			std::remove_if(imageEntries.begin(), imageEntries.end(),
				[](const SlaveImageEntry& e){ return e.groupName.empty(); }),
			imageEntries.end());
	}

	SlaveImageEntry entry;
	entry.tex = frames.empty() ? nullptr : frames.front();
	entry.w = w;
	entry.h = h;
	entry.mods = mods;
	entry.spawnTimeSec = SDL_GetTicks() / 1000.0f;
	entry.currentSpin = 0.0f;
	entry.groupName = groupName;
	entry.frames = frames;
	entry.frameDelaysMs = delaysMs;
	entry.frameIdx = 0;
	entry.frameElapsedSec = 0.0f;
	imageEntries.push_back(std::move(entry));
}

void SlaveWindow::ClearImage(){
	imageEntries.erase(
		std::remove_if(imageEntries.begin(), imageEntries.end(),
			[](const SlaveImageEntry& e){ return e.groupName.empty(); }),
		imageEntries.end());
}

void SlaveWindow::ClearImageAll(){
	imageEntries.clear();
}

void SlaveWindow::StartParticles(ParticleType type, SDL_Texture* tex,
	int texW, int texH, int screenW, int screenH,
	const RenderModifiers& mods, const std::string& groupName){
	// (1.5) Ungrouped particle systems now coexist — no replace-on-start.
	// `clear` / `stop` still drain all ungrouped systems together.
	SlaveParticleEntry entry;
	entry.groupName = groupName;
	entry.system.Start(type, tex, texW, texH, screenW, screenH, mods);
	particleEntries.push_back(std::move(entry));
}

void SlaveWindow::StartParticlesPool(ParticleType type,
	const std::vector<ParticleTex>& pool,
	int screenW, int screenH, const RenderModifiers& mods,
	const std::string& groupName){
	SlaveParticleEntry entry;
	entry.groupName = groupName;
	entry.system.StartPool(type, pool, screenW, screenH, mods);
	particleEntries.push_back(std::move(entry));
}

void SlaveWindow::StopParticles(){
	// (1.4) Drop only ungrouped *non-smooth-stopping* systems. A
	// `stopParticleCont` issued earlier puts a system in smooth-stop
	// mode, and the user wants its ending to play out even if a later
	// `stop` runs.
	particleEntries.erase(
		std::remove_if(particleEntries.begin(), particleEntries.end(),
			[](const SlaveParticleEntry& e){
				return e.groupName.empty() && !e.system.IsStoppingSmoothly();
			}),
		particleEntries.end());
}

void SlaveWindow::StopParticlesAll(){
	// (1.4) Same protection as StopParticles — keep smoothly-stopping ones.
	particleEntries.erase(
		std::remove_if(particleEntries.begin(), particleEntries.end(),
			[](const SlaveParticleEntry& e){
				return !e.system.IsStoppingSmoothly();
			}),
		particleEntries.end());
}

void SlaveWindow::StopParticlesSmoothly(const std::string& groupName){
	for(auto& e : particleEntries){
		bool matches = groupName.empty()
			? e.groupName.empty()
			: (e.groupName == groupName);
		if(matches) e.system.StopSmoothly();
	}
}

void SlaveWindow::ClearGroup(const std::string& groupName){
	if(groupName.empty()) return;
	auto matches = [&](const std::string& g){ return g == groupName; };
	textEntries.erase(
		std::remove_if(textEntries.begin(), textEntries.end(),
			[&](const SlaveTextEntry& e){ return matches(e.groupName); }),
		textEntries.end());
	imageEntries.erase(
		std::remove_if(imageEntries.begin(), imageEntries.end(),
			[&](const SlaveImageEntry& e){ return matches(e.groupName); }),
		imageEntries.end());
	// (1.4) Smooth-stopping particle systems survive `clear NAME` so the
	// ending cue plays out — same rule as `stop` above.
	particleEntries.erase(
		std::remove_if(particleEntries.begin(), particleEntries.end(),
			[&](const SlaveParticleEntry& e){
				return matches(e.groupName) && !e.system.IsStoppingSmoothly();
			}),
		particleEntries.end());
}

void SlaveWindow::ClearAll(){
	textEntries.clear();
	imageEntries.clear();
	// (1.4) Same smooth-stop protection. `clearAll` is the closest thing
	// to a hard reset, but the user explicitly asked for endings to
	// survive any later command.
	particleEntries.erase(
		std::remove_if(particleEntries.begin(), particleEntries.end(),
			[](const SlaveParticleEntry& e){
				return !e.system.IsStoppingSmoothly();
			}),
		particleEntries.end());
}

void SlaveWindow::RenderTextureFitted(SDL_Renderer* r, SDL_Texture* tex,
	int texW, int texH, int screenW, int screenH,
	const RenderModifiers& mods,
	float spawnTimeSec, float* currentSpin){
	if(!tex || texW <= 0 || texH <= 0) return;

	int drawW, drawH, drawX, drawY;

	if(mods.width > 0 && mods.height > 0){
		drawW = mods.width;
		drawH = mods.height;
	} else{
		float scaleW = (float)screenW / (float)texW;
		float scaleH = (float)screenH / (float)texH;
		float scale = (scaleW < scaleH) ? scaleW : scaleH;
		drawW = (int)(texW * scale);
		drawH = (int)(texH * scale);
	}

	// ── Position: animated move() wins over pos(); otherwise centred ──
	if(mods.hasMove && mods.moveDurationSec > 0.0f && spawnTimeSec > 0.0f){
		float now = SDL_GetTicks() / 1000.0f;
		float elapsed = now - spawnTimeSec;
		float t = std::clamp(elapsed / mods.moveDurationSec, 0.0f, 1.0f);

		int fromX, fromY, toX, toY;
		if(mods.moveFromSide != 0){
			SideToPixel(mods.moveFromSide, drawW, drawH, screenW, screenH, fromX, fromY);
		} else{
			fromX = (int)(mods.moveFromX / 100.0f * (float)screenW);
			fromY = (int)(mods.moveFromY / 100.0f * (float)screenH);
		}
		if(mods.moveToSide != 0){
			SideToPixel(mods.moveToSide, drawW, drawH, screenW, screenH, toX, toY);
		} else{
			toX = (int)(mods.moveToX / 100.0f * (float)screenW);
			toY = (int)(mods.moveToY / 100.0f * (float)screenH);
		}
		drawX = (int)((1.0f - t) * (float)fromX + t * (float)toX);
		drawY = (int)((1.0f - t) * (float)fromY + t * (float)toY);
	} else if(mods.HasPosition()){
		drawX = (int)(mods.posX / 100.0f * (float)screenW);
		drawY = (int)(mods.posY / 100.0f * (float)screenH);
	} else{
		drawX = (screenW - drawW) / 2;
		drawY = (screenH - drawH) / 2;
	}

	// ── Rotation: static rotate() + accumulated spin() ──
	float rotation = mods.rotation;
	if(mods.spinDegPerSec != 0.0f && currentSpin){
		*currentSpin += mods.spinDegPerSec * lastDt;
		rotation += *currentSpin;
	}

	// Apply transparency
	uint8_t alpha = (uint8_t)(std::clamp(mods.transparency, 0.0f, 1.0f) * 255.0f);
	SDL_SetTextureAlphaMod(tex, alpha);

	SDL_Rect dst = {drawX, drawY, drawW, drawH};

	if(rotation != 0.0f){
		SDL_RenderCopyEx(r, tex, nullptr, &dst,
			(double)rotation, nullptr, SDL_FLIP_NONE);
	} else{
		SDL_RenderCopy(r, tex, nullptr, &dst);
	}

	// Reset alpha
	SDL_SetTextureAlphaMod(tex, 255);
}

} // namespace SatyrAV
