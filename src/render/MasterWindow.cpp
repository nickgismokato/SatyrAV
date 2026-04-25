#include "render/MasterWindow.hpp"
#include <algorithm>
#include <vector>
#include <string>

namespace SatyrAV{

static std::string TruncateScene(const std::string& name){
	if((int)name.size() <= SCENE_NAME_DISPLAY_MAX) return name;
	return name.substr(0, SCENE_NAME_TRUNCATE_AT) + "...";
}

static std::string CommandToString(const Command& cmd){
	switch(cmd.type){
		case CommandType::Text:        return "text \"" + cmd.argument + "\"";
		case CommandType::TextCont:    return "textCont \"" + cmd.argument + "\"";
		case CommandType::Clear:       return cmd.argument.empty() ? "clear" : "clear " + cmd.argument;
		case CommandType::ClearText:   return "clearText";
		case CommandType::ClearImages: return "clearImages";
		case CommandType::ClearAll:    return "clearAll";
		case CommandType::Play:        return "play " + cmd.argument;
		case CommandType::Stop:        return cmd.argument.empty() ? "stop" : "stop " + cmd.argument;
		case CommandType::StopParticleCont: return cmd.argument.empty() ? "stopParticleCont" : "stopParticleCont " + cmd.argument;
		case CommandType::Show:        return "show " + cmd.argument;
		case CommandType::Loop:        return "loop(...)";
		case CommandType::Comment:     return "# " + cmd.argument;
		case CommandType::Compound:{
			std::string result;
			for(size_t i = 0; i < cmd.subCommands.size(); i++){
				if(i > 0) result += " & ";
				result += CommandToString(cmd.subCommands[i]);
			}
			return result;
		}
	}
	return "???";
}

void MasterWindow::DrawHeader(Renderer& r, TextRenderer& text, const CueEngine& engine){
	auto* renderer = r.GetMaster();
	int w = r.GetMasterWidth();
	int boxW = w / 3;
	int boxH = 60;

	// Three header boxes
	for(int i = 0; i < 3; i++){
		r.DrawRect(renderer, i * boxW, 0, boxW - 2, boxH, Colours::HEADER_BG);
	}

	// Labels
	text.DrawText(renderer, "Akt:", 10, 5, Colours::WHITE);
	text.DrawText(renderer, "Scene:", boxW + 10, 5, Colours::WHITE);
	text.DrawText(renderer, "Næste Scene:", 2 * boxW + 10, 5, Colours::WHITE);

	// Values — only show if akt/scene are actually chosen
	auto* akt = engine.GetCurrentAkt();
	if(akt){
		text.DrawText(renderer, std::to_string(akt->number), 10, 30, Colours::ORANGE);
	}

	auto* scene = engine.GetCurrentScene();
	if(scene){
		text.DrawText(renderer, TruncateScene(scene->name), boxW + 10, 30, Colours::ORANGE);

		// Next scene — only show when a scene is active
		auto* next = engine.GetNextScene();
		if(next){
			// Check if next scene is in a different akt
			int curAkt = engine.GetCurrentAktIndex();
			int curScene = engine.GetCurrentSceneIndex();
			int sceneCount = engine.GetSceneCountForAkt(curAkt);
			if(curScene + 1 >= sceneCount){
				// Next akt
				std::string label = "Akt " + std::to_string(curAkt + 2) + ", " + TruncateScene(next->name);
				text.DrawText(renderer, label, 2 * boxW + 10, 30, Colours::ORANGE);
			} else{
				text.DrawText(renderer, TruncateScene(next->name), 2 * boxW + 10, 30, Colours::ORANGE);
			}
		} else{
			text.DrawText(renderer, "THE END!", 2 * boxW + 10, 30, Colours::ORANGE);
		}
	}
	// If no scene chosen yet, "Naeste Scene:" stays empty (just the label)
}

void MasterWindow::DrawNavigator(Renderer& r, TextRenderer& text,
	const CueEngine& engine,
	int primedAkt, int primedScene, int primedCmd, int activeColumn){
	auto* renderer = r.GetMaster();
	int w = r.GetMasterWidth();
	int h = r.GetMasterHeight();
	int startY = 100;
	int lineH = text.GetLineHeight() + 4;

	// Column widths
	int aktColW = 80;
	int sceneColW = w / 3;
	int sceneColX = aktColW;
	int cmdColX = aktColW + sceneColW;
	int cmdColW = w - cmdColX;

	int maxLines = (h - startY - 20) / lineH;
	if(maxLines < 1) maxLines = 1;

	// Draw column separators
	r.DrawRect(renderer, aktColW - 1, startY, 1, h - startY, {0x40, 0x50, 0x60, 0xFF});
	r.DrawRect(renderer, cmdColX - 1, startY, 1, h - startY, {0x40, 0x50, 0x60, 0xFF});

	// ── Akt column ───────────────────────────────────────────
	int aktCount = engine.GetAktCount();
	for(int i = 0; i < aktCount && i < maxLines; i++){
		int y = startY + i * lineH;
		if(i == primedAkt){
			r.DrawRect(renderer, 0, y, aktColW - 2, lineH, Colours::HIGHLIGHT);
		}
		std::string label = "Akt " + std::to_string(i + 1);
		Colour c = (i == primedAkt && activeColumn == 0)
			? Colours::ORANGE : Colours::WHITE;
		text.DrawText(renderer, label, 8, y + 2, c);
	}

	// ── Scene column (shows scenes of primed akt) ────────────
	if(primedAkt >= 0){
		int sceneCount = engine.GetSceneCountForAkt(primedAkt);
		// Calculate scroll offset for scene list
		int sceneScroll = 0;
		if(primedScene > maxLines / 2){
			sceneScroll = primedScene - maxLines / 2;
		}
		if(sceneScroll + maxLines > sceneCount){
			sceneScroll = std::max(0, sceneCount - maxLines);
		}

		// Overflow indicator top
		if(sceneScroll > 0){
			text.DrawText(renderer, "...", sceneColX + 5, startY - lineH + 2, Colours::WHITE);
		}

		for(int i = 0; i < maxLines && (sceneScroll + i) < sceneCount; i++){
			int idx = sceneScroll + i;
			int y = startY + i * lineH;

			if(idx == primedScene){
				r.DrawRect(renderer, sceneColX, y, sceneColW - 2, lineH, Colours::HIGHLIGHT);
			}

			// Line number
			std::string num = std::to_string(idx + 1);
			text.DrawText(renderer, num, sceneColX + 3, y + 2, {0x55, 0x55, 0x55, 0xFF});

			// Scene name
			auto* scene = engine.GetScene(primedAkt, idx);
			std::string name = scene ? TruncateScene(scene->name) : "???";
			Colour c = (idx == primedScene && activeColumn == 1)
				? Colours::ORANGE : Colours::WHITE;
			text.DrawText(renderer, name, sceneColX + 35, y + 2, c);
		}

		// Overflow indicator bottom
		if(sceneScroll + maxLines < sceneCount){
			text.DrawText(renderer, "...", sceneColX + 5, startY + maxLines * lineH + 2, Colours::WHITE);
		}
	}

	// ── Command column (shows commands of primed scene) ──────
	//
	// (1.6) Long commands wrap onto continuation rows instead of being
	// truncated with "...". Each command consumes one or more rows; the
	// loop tracks a real y cursor and stops once we run out of vertical
	// space. The primed-row highlight spans every wrapped row, and the
	// line-number column only prints on the first row of the command.
	if(primedAkt >= 0 && primedScene >= 0){
		int cmdCount = engine.GetCommandCountForScene(primedAkt, primedScene);
		// Available pixels for the command text on each row, after the
		// line-number gutter. Real glyph widths come from MeasureWidth so
		// we don't have to guess a per-character fudge factor.
		int rowPxBudget = cmdColW - 35 - 5;
		if(rowPxBudget < 20) rowPxBudget = 20;

		auto wrapCommand = [&text, rowPxBudget](const std::string& s){
			std::vector<std::string> rows;
			if(s.empty()){ rows.emplace_back(); return rows; }
			if(text.MeasureWidth(s) <= rowPxBudget){
				rows.push_back(s);
				return rows;
			}
			size_t pos = 0;
			while(pos < s.size()){
				// Find the longest prefix from `pos` that fits in the budget.
				// Doubling-then-binary-search keeps measurement calls bounded
				// at O(log N) per row instead of scanning char-by-char.
				size_t lo = 1;
				size_t hi = s.size() - pos;
				if(text.MeasureWidth(s.substr(pos, hi)) <= rowPxBudget){
					rows.push_back(s.substr(pos, hi));
					pos += hi;
					break;
				}
				while(lo < hi){
					size_t mid = lo + (hi - lo + 1) / 2;
					if(text.MeasureWidth(s.substr(pos, mid)) <= rowPxBudget){
						lo = mid;
					} else{
						hi = mid - 1;
					}
				}
				size_t take = lo;
				// Prefer breaking at the last whitespace inside the slice
				// so words don't split (only if there's more to render).
				if(pos + take < s.size()){
					size_t breakAt = s.find_last_of(' ', pos + take);
					if(breakAt != std::string::npos && breakAt > pos){
						take = breakAt - pos;
					}
				}
				if(take == 0) take = 1; // pathological narrow column safety
				rows.push_back(s.substr(pos, take));
				pos += take;
				if(pos < s.size() && s[pos] == ' ') pos++;
			}
			return rows;
		};

		// Scroll: keep primedCmd visible. Compute how many wrapped rows
		// each command above primedCmd consumes so we can scroll by rows
		// rather than indices and avoid clipping a wrapped primed entry.
		int cmdScroll = 0;
		if(primedCmd > maxLines / 2){
			cmdScroll = primedCmd - maxLines / 2;
		}
		if(cmdScroll + maxLines > cmdCount){
			cmdScroll = std::max(0, cmdCount - maxLines);
		}

		if(cmdScroll > 0){
			text.DrawText(renderer, "...", cmdColX + 5, startY - lineH + 2, Colours::WHITE);
		}

		int execCounter = 0;
		for(int idx = 0; idx < cmdScroll; idx++){
			auto* c = engine.GetCommand(primedAkt, primedScene, idx);
			if(c && c->type != CommandType::Comment) execCounter++;
		}

		int yCursor = startY;
		int yLimit = startY + maxLines * lineH;
		bool overflowed = false;
		for(int idx = cmdScroll; idx < cmdCount; idx++){
			if(yCursor >= yLimit){ overflowed = true; break; }

			auto* cmd = engine.GetCommand(primedAkt, primedScene, idx);
			bool isComment = cmd && cmd->type == CommandType::Comment;

			std::string cmdText = cmd ? CommandToString(*cmd) : "???";
			auto rows = wrapCommand(cmdText);
			int rowCount = (int)rows.size();
			int blockH = rowCount * lineH;

			if(idx == primedCmd){
				int hlH = std::min(blockH, yLimit - yCursor);
				r.DrawRect(renderer, cmdColX, yCursor, cmdColW, hlH, Colours::HIGHLIGHT);
			}

			if(!isComment){
				execCounter++;
				std::string num = std::to_string(execCounter);
				text.DrawText(renderer, num, cmdColX + 3, yCursor + 2,
					{0x55, 0x55, 0x55, 0xFF});
			}

			Colour c;
			if(isComment){
				c = {0x70, 0x80, 0x90, 0xFF};
			} else{
				c = (idx == primedCmd && activeColumn == 2)
					? Colours::ORANGE : Colours::WHITE;
			}

			for(int ri = 0; ri < rowCount; ri++){
				int rowY = yCursor + ri * lineH;
				if(rowY >= yLimit){ overflowed = true; break; }
				text.DrawText(renderer, rows[ri], cmdColX + 35, rowY + 2, c);
			}

			yCursor += blockH;
		}

		if(overflowed || yCursor >= yLimit){
			text.DrawText(renderer, "...", cmdColX + 5, yLimit + 2, Colours::WHITE);
		}
	}
}

} // namespace SatyrAV
