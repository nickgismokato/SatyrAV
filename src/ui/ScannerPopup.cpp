#include "ui/ScannerPopup.hpp"
#include "core/Logger.hpp"
#include <algorithm>

namespace SatyrAV{

void ScannerPopup::Open(const std::string& path){
	projectPath = path;
	Rescan();
	Show();
}

void ScannerPopup::Rescan(){
	report = ProjectScanner::Scan(projectPath);
	rows.clear();
	scroll = 0;
	std::string currentFile;
	for(const auto& f : report.findings){
		if(f.file != currentFile){
			currentFile = f.file;
			rows.push_back({Row::Kind::File, f.file});
		}
		std::string where = f.line > 0 ? "L" + std::to_string(f.line) : "file";
		rows.push_back({f.level == ScanFinding::Level::Error ? Row::Kind::Error : Row::Kind::Warning,
			"  " + where + ": " + f.message});
	}
	Logger::Instance().Infof("Scanner: %d files, %d errors, %d warnings",
		report.filesScanned, report.errors, report.warnings);
	for(const auto& f : report.findings){
		Logger::Instance().Infof("Scanner: %s:%d %s %s", f.file.c_str(), f.line,
			f.level == ScanFinding::Level::Error ? "error:" : "warning:", f.message.c_str());
	}
}

void ScannerPopup::Close(){ Hide(); }

void ScannerPopup::Scroll(int n){
	const int maxScroll = std::max(0, (int)rows.size() - visibleRows);
	scroll = std::clamp(scroll + n, 0, maxScroll);
}

void ScannerPopup::Draw(Renderer& r, TextRenderer& text){
	if(!visible) return;

	const int lineH = text.GetLineHeight() + 2;
	const int w = std::min(1000, r.GetMasterWidth() - 60);
	const int h = std::min(600, r.GetMasterHeight() - 60);
	DrawBox(r, text, "Project Scanner", w, h);
	auto* renderer = r.GetMaster();

	const Colour ok   = Colours::LIME_GREEN;
	const Colour warn = {0xFF, 0xCC, 0x66, 0xFF};
	const Colour err  = {0xFF, 0x44, 0x44, 0xFF};
	const Colour dim  = {0x80, 0x90, 0xA0, 0xFF};

	const int x = boxX + 16;
	int y = boxY + 40;

	std::string summary = "Files: " + std::to_string(report.filesScanned)
		+ "    Errors: " + std::to_string(report.errors)
		+ "    Warnings: " + std::to_string(report.warnings);
	text.DrawText(renderer, summary, x, y,
		report.errors ? err : (report.warnings ? warn : ok));
	y += lineH + 6;

	if(report.Clean()){
		text.DrawText(renderer, "No problems found.", x, y, ok);
	}

	const int yLimit = boxY + h - 50;
	visibleRows = std::max(1, (yLimit - y) / lineH);
	const int maxW = w - 32;
	for(int i = scroll; i < (int)rows.size() && y + lineH <= yLimit; i++){
		const Row& row = rows[i];
		std::string s = row.text;
		// Cut what doesn't fit; the full text is in the log.
		if(text.MeasureWidth(s) > maxW){
			while(!s.empty() && text.MeasureWidth(s + "...") > maxW){
				s.pop_back();
				// Don't leave half a UTF-8 character.
				while(!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
				if(!s.empty() && ((unsigned char)s.back() & 0xC0) == 0xC0) s.pop_back();
			}
			s += "...";
		}
		const Colour c = row.kind == Row::Kind::File ? Colours::ORANGE
			: row.kind == Row::Kind::Error ? err : warn;
		text.DrawText(renderer, s, x, y, c);
		y += lineH;
	}

	std::string footer = "UP/DOWN scroll (SHIFT: 10)  -  R rescan  -  C / ESC close";
	if((int)rows.size() > visibleRows){
		footer += "  -  rows " + std::to_string(scroll + 1) + "-"
			+ std::to_string(std::min((int)rows.size(), scroll + visibleRows))
			+ " of " + std::to_string(rows.size());
	}
	text.DrawTextCentered(renderer, footer, boxX + w / 2, boxY + h - 28, dim);
}

} // namespace SatyrAV
