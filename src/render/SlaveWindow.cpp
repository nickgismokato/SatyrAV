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

void SlaveWindow::DrawText(Renderer& r, TextRenderer& text){
	auto* renderer = r.GetSlave();
	if(!renderer) return;
	// Text lives inside the targeted rect — use target dims for layout.
	int slaveW = r.GetTargetWidth();
	int slaveH = r.GetTargetHeight();
	int slaveLineH = text.GetSlaveLineHeight();

	// Calculate total height for default-positioned, non-animated text.
	// Animated (move) entries float independently and don't take a slot.
	int defaultCount = 0;
	for(auto& entry : textEntries){
		bool animated = entry.mods.hasMove;
		if(!entry.mods.HasPosition() && !animated) defaultCount++;
	}
	int totalH = defaultCount * slaveLineH;
	int defaultY = slaveH / 2 - totalH / 2;

	for(auto& entry : textEntries){
		if(entry.runs.empty()) continue;

		// ── Measure the full line (sum of every run's width, max of heights). ──
		int textW = 0, textH = 0;
		std::vector<int> runWidths(entry.runs.size(), 0);
		for(size_t k = 0; k < entry.runs.size(); k++){
			int rw = 0, rh = 0;
			text.MeasureSlave(entry.runs[k].text, rw, rh);
			runWidths[k] = rw;
			textW += rw;
			if(rh > textH) textH = rh;
		}

		// ── Compute rotation: static rotate() + accumulated spin(). ──
		float rotation = entry.mods.rotation;
		if(entry.mods.spinDegPerSec != 0.0f){
			entry.currentSpin += entry.mods.spinDegPerSec * lastDt;
			rotation += entry.currentSpin;
		}

		// ── Compute position: animated move() wins over pos(); else stack. ──
		// In all three branches drawX/drawY denote the top-left of the first
		// run; the renderer walks runs left-to-right from there.
		int drawX, drawY;

		if(entry.mods.hasMove && entry.mods.moveDurationSec > 0.0f){
			float now = SDL_GetTicks() / 1000.0f;
			float elapsed = now - entry.spawnTimeSec;
			float t = std::clamp(elapsed / entry.mods.moveDurationSec, 0.0f, 1.0f);

			int fromX, fromY, toX, toY;
			if(entry.mods.moveFromSide != 0){
				SideToPixel(entry.mods.moveFromSide, textW, textH, slaveW, slaveH, fromX, fromY);
			} else{
				// Percent coords are resolved as the top-left of the text bounds,
				// matching how pos() already works elsewhere.
				fromX = (int)(entry.mods.moveFromX / 100.0f * (float)slaveW);
				fromY = (int)(entry.mods.moveFromY / 100.0f * (float)slaveH);
			}
			if(entry.mods.moveToSide != 0){
				SideToPixel(entry.mods.moveToSide, textW, textH, slaveW, slaveH, toX, toY);
			} else{
				toX = (int)(entry.mods.moveToX / 100.0f * (float)slaveW);
				toY = (int)(entry.mods.moveToY / 100.0f * (float)slaveH);
			}
			drawX = (int)((1.0f - t) * (float)fromX + t * (float)toX);
			drawY = (int)((1.0f - t) * (float)fromY + t * (float)toY);
		} else if(entry.mods.HasPosition()){
			drawX = (int)(entry.mods.posX / 100.0f * (float)slaveW);
			drawY = (int)(entry.mods.posY / 100.0f * (float)slaveH);
		} else{
			// Default: centred stack. Anchor the line so the sum of runs is
			// centred, then walk runs left-to-right.
			drawX = slaveW / 2 - textW / 2;
			drawY = defaultY;
			defaultY += slaveLineH;
		}

		// ── Draw — rotated path if any rotation is present. ──
		// For rotated multi-run lines we draw each run separately using the
		// rotated glyph path. Runs sit side-by-side in the line's local frame;
		// when the whole line rotates, each run rotates around its own anchor.
		// This is a reasonable first pass — a future refinement could rotate
		// around the line's centroid so runs pivot as a rigid group.
		int cursorX = drawX;
		for(size_t k = 0; k < entry.runs.size(); k++){
			const TextRun& run = entry.runs[k];
			Colour c = ResolveRunColour(run, entry.mods);

			if(rotation != 0.0f){
				text.DrawTextSlaveRotated(renderer, run.text, cursorX, drawY, c, rotation);
			} else{
				text.DrawTextSlave(renderer, run.text, cursorX, drawY, c);
			}
			cursorX += runWidths[k];
		}
	}
}

void SlaveWindow::DrawMedia(Renderer& r, MediaPlayer& media){
	auto* renderer = r.GetSlave();
	if(!renderer) return;
	// Media (video/image) is laid out inside the targeted rect.
	int slaveW = r.GetTargetWidth();
	int slaveH = r.GetTargetHeight();

	// Video takes full screen (behind text)
	if(media.IsVideoPlaying()){
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
