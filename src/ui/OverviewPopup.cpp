#include "ui/OverviewPopup.hpp"
#include "core/CueEngine.hpp"
#include "core/Logger.hpp"
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

namespace SatyrAV{

namespace{
	// Mirrors PerformanceScreen::ResolvePath. Kept as a free helper so
	// the popup doesn't need a back-pointer to the screen. Subdirs match
	// the project create-time layout (`pictures/`, `movies/`, `sound/`).
	std::string ResolvePath(const std::string& base, const std::string& arg){
		std::string fullPath = base + "/" + arg;
		if(fs::exists(fullPath)) return fullPath;
		const char* SUBDIRS[] = {"pictures", "movies", "sound", nullptr};
		for(int i = 0; SUBDIRS[i]; i++){
			std::string tryPath = base + "/" + SUBDIRS[i] + "/" + arg;
			if(fs::exists(tryPath)) return tryPath;
		}
		return fullPath;
	}

	const char* CmdLabel(CommandType t){
		switch(t){
			case CommandType::Show: return "show";
			case CommandType::Play: return "play";
			default:                return "?";
		}
	}

	// Walk a command (recurse into compound + loop bodies) collecting
	// every Show/Play that points at a file that doesn't exist on disk.
	void CollectMissing(const Command& cmd, const std::string& base,
		std::vector<std::string>& out){
		auto check = [&](const Command& c){
			if(c.argument.empty()) return;
			std::string resolved = ResolvePath(base, c.argument);
			if(!fs::exists(resolved)){
				out.push_back(std::string(CmdLabel(c.type)) + ": " + c.argument);
			}
		};
		if(cmd.type == CommandType::Show || cmd.type == CommandType::Play){
			check(cmd);
		}
		for(const auto& sub : cmd.subCommands) CollectMissing(sub, base, out);
		for(const auto& sub : cmd.loopBody)    CollectMissing(sub, base, out);
	}
}

void OverviewPopup::Open(const CueEngine& engine, const std::string& projectBasePath){
	entries.clear();
	scroll = 0;
	totalWarnings = 0;
	totalMissing = 0;

	int aktCount = engine.GetAktCount();
	for(int a = 0; a < aktCount; a++){
		auto* akt = engine.GetAkt(a);
		if(!akt) continue;
		int sceneCount = (int)akt->scenes.size();
		for(int s = 0; s < sceneCount; s++){
			auto* scene = engine.GetScene(a, s);
			if(!scene) continue;

			SceneEntry entry;
			entry.aktSceneLabel = "Akt " + std::to_string(a + 1)
				+ " / " + scene->name;
			entry.fileName     = scene->fileName;
			entry.commandCount = (int)scene->commands.size();
			entry.warnings     = scene->warnings;

			for(const auto& c : scene->commands){
				CollectMissing(c, projectBasePath, entry.missingPaths);
			}

			totalWarnings += (int)entry.warnings.size();
			totalMissing  += (int)entry.missingPaths.size();
			entries.push_back(std::move(entry));
		}
	}

	Logger::Instance().Infof(
		"Overview: %zu scenes scanned, %d warnings, %d missing files",
		entries.size(), totalWarnings, totalMissing);
	Show();
}

void OverviewPopup::Close(){ Hide(); }

void OverviewPopup::ScrollUp(int steps){
	scroll -= steps;
	if(scroll < 0) scroll = 0;
}

void OverviewPopup::ScrollDown(int steps){
	scroll += steps;
	if(scroll >= (int)entries.size()) scroll = (int)entries.size() - 1;
	if(scroll < 0) scroll = 0;
}

void OverviewPopup::Draw(Renderer& r, TextRenderer& text){
	if(!visible) return;

	int lineH = text.GetLineHeight() + 2;
	int popW  = 720;
	int popH  = 520;
	DrawBox(r, text, "Overview Debug", popW, popH);

	auto* renderer = r.GetMaster();

	Colour hdr  = Colours::ORANGE;
	Colour ok   = Colours::LIME_GREEN;
	Colour warn = {0xFF, 0xCC, 0x66, 0xFF};
	Colour err  = {0xFF, 0x44, 0x44, 0xFF};
	Colour txt  = Colours::WHITE;
	Colour dim  = {0x80, 0x90, 0xA0, 0xFF};

	int x = boxX + 16;
	int y = boxY + 40;

	// ── Header summary ────────────────────────────────────────
	std::string summary = "Scenes: " + std::to_string(entries.size())
		+ "    Warnings: " + std::to_string(totalWarnings)
		+ "    Missing files: " + std::to_string(totalMissing);
	Colour summaryColour = (totalWarnings == 0 && totalMissing == 0) ? ok : warn;
	text.DrawText(renderer, summary, x, y, summaryColour);
	y += lineH + 6;

	// ── Per-scene rows ────────────────────────────────────────
	int yLimit = boxY + popH - 50; // leave footer space
	for(int i = scroll; i < (int)entries.size(); i++){
		if(y >= yLimit) break;
		const auto& e = entries[i];

		// Scene label + command count
		std::string head = e.aktSceneLabel + "   ("
			+ std::to_string(e.commandCount) + " cmds)";
		text.DrawText(renderer, head, x, y, hdr);
		y += lineH;

		if(e.warnings.empty() && e.missingPaths.empty()){
			text.DrawText(renderer, "  OK", x, y, dim);
			y += lineH + 2;
			continue;
		}

		for(const auto& w : e.warnings){
			if(y >= yLimit) break;
			std::string line = "  L" + std::to_string(w.lineNumber)
				+ ": " + w.message;
			text.DrawText(renderer, line, x, y, warn);
			y += lineH;
		}
		for(const auto& m : e.missingPaths){
			if(y >= yLimit) break;
			text.DrawText(renderer, "  missing " + m, x, y, err);
			y += lineH;
		}
		y += 4;
	}

	// ── Footer hint ───────────────────────────────────────────
	text.DrawTextCentered(renderer,
		"UP/DOWN to scroll  -  ESC to close",
		boxX + popW / 2, boxY + popH - 28, dim);
}

} // namespace SatyrAV
