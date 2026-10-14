#pragma once
#include "screens/Screen.hpp"
#include "ui/TextField.hpp"
#include "ui/Dropdown.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace SatyrAV{

// (1.6.2) Per-project metadata captured during the directory scan. The
// LoadProject screen never opens scene files just to render the list —
// only `schema.toml` is parsed, which keeps the scan fast even on revy
// archives with hundreds of projects.
struct ProjectInfo{
	std::string folderName;   // disk directory name (used as a fallback display)
	std::string path;         // absolute project path
	std::string fullRevyName; // schema RevyData.Revy verbatim, e.g. "MatRevy 2025"
	std::string groupName;    // fullRevyName with the trailing year stripped
	int         year = 0;     // parsed off the trailing 4 digits, 0 if unknown
	std::string creator;      // schema RevyData.Creator (1.6.2+); empty for older
	std::filesystem::file_time_type mtime{}; // folder mtime — used by Recent sort
};

// Display rows interleave group headers with project rows so navigation
// can skip past headers without an extra map lookup. -1 in `projectIdx`
// marks a header.
struct LoadRow{
	bool isHeader = false;
	std::string headerText;
	int projectIdx = -1; // index into LoadProjectScreen::projects
};

enum class LoadFocus{ Search, Filter, Sort, List };

class LoadProjectScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

private:
	std::vector<ProjectInfo> projects;
	std::vector<LoadRow> rows;

	TextField searchField;
	Dropdown  sortDropdown;
	Dropdown  filterDropdown; // (1.6.2) Revy-type filter; "All" + every group name
	LoadFocus focus = LoadFocus::List;

	int selectedRow = 0; // index into `rows`; always points at a project row
	int scrollOffset = 0;

	void ScanProjects();
	void Rebuild();           // re-applies filter + sort, regenerates `rows`
	void MoveSelection(int delta); // skips header rows automatically
	void JumpSelectionToTop();
	void JumpSelectionToBottom();
	void OpenSelected();
	void SetFocus(LoadFocus f);
	int  FirstProjectRow() const;
	int  LastProjectRow() const;
};

} // namespace SatyrAV
