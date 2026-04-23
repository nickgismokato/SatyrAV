#include "render/MasterWindow.hpp"
#include <algorithm>

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
	if(primedAkt >= 0 && primedScene >= 0){
		int cmdCount = engine.GetCommandCountForScene(primedAkt, primedScene);
		// Calculate scroll offset
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

		// Count executable commands (non-comments) in [0, cmdScroll) so we can
		// resume the displayed line-number counter when the view is scrolled.
		int execCounter = 0;
		for(int idx = 0; idx < cmdScroll; idx++){
			auto* c = engine.GetCommand(primedAkt, primedScene, idx);
			if(c && c->type != CommandType::Comment) execCounter++;
		}

		for(int i = 0; i < maxLines && (cmdScroll + i) < cmdCount; i++){
			int idx = cmdScroll + i;
			int y = startY + i * lineH;

			if(idx == primedCmd){
				r.DrawRect(renderer, cmdColX, y, cmdColW, lineH, Colours::HIGHLIGHT);
			}

			auto* cmd = engine.GetCommand(primedAkt, primedScene, idx);
			bool isComment = cmd && cmd->type == CommandType::Comment;

			// Line number — executable rows only; comments leave the column blank.
			if(!isComment){
				execCounter++;
				std::string num = std::to_string(execCounter);
				text.DrawText(renderer, num, cmdColX + 3, y + 2, {0x55, 0x55, 0x55, 0xFF});
			}

			// Command text
			std::string cmdText = cmd ? CommandToString(*cmd) : "???";
			// Truncate to fit column
			int maxChars = (cmdColW - 45) / 7;
			if((int)cmdText.size() > maxChars && maxChars > 3){
				cmdText = cmdText.substr(0, maxChars - 3) + "...";
			}
			Colour c;
			if(isComment){
				c = {0x70, 0x80, 0x90, 0xFF}; // dim slate for comments
			} else{
				c = (idx == primedCmd && activeColumn == 2)
					? Colours::ORANGE : Colours::WHITE;
			}
			text.DrawText(renderer, cmdText, cmdColX + 35, y + 2, c);
		}

		if(cmdScroll + maxLines < cmdCount){
			text.DrawText(renderer, "...", cmdColX + 5, startY + maxLines * lineH + 2, Colours::WHITE);
		}
	}
}

} // namespace SatyrAV
