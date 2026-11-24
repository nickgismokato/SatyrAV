#pragma once
#include "ui/Popup.hpp"
#include "core/ProjectScanner.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

// The project scanner window (core/ProjectScanner). Opened with `C` in a
// project, or `D` then `O`. The scan runs when the window opens and again
// on `R`, never otherwise, so it costs nothing during a show. Modal while
// open: UP / DOWN scroll (SHIFT for ten rows), R rescans, C or ESC closes.
//
// Replaces the Overview popup, which checked the scenes as they were last
// loaded; the scanner reads the files from disk, so it sees edits made
// since, and it reports every finding with its file and line.
class ScannerPopup : public Popup{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	void Open(const std::string& projectPath);
	void Rescan();
	void Close();

	void Scroll(int rows);

private:
	struct Row{
		enum class Kind{ File, Error, Warning } kind;
		std::string text;
	};
	std::string projectPath;
	ScanReport report;
	std::vector<Row> rows;
	int scroll = 0;
	int visibleRows = 1; // set by Draw
};

} // namespace SatyrAV
