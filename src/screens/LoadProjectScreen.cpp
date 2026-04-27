#include "screens/LoadProjectScreen.hpp"
#include "App.hpp"
#include "core/Config.hpp"
#include "core/Project.hpp"
#include "core/SchemaParser.hpp"
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <unordered_map>

namespace fs = std::filesystem;

namespace SatyrAV{

namespace{
	// (1.6.2) Strip a trailing 4-digit year token from a Revy display
	// name, e.g. "MatRevyJubi 2025" → "MatRevyJubi". Step 1 of the group-
	// key derivation; StripJubiSuffix below is step 2 and folds Jubi
	// editions into their parent revy. If no trailing year is found the
	// whole name becomes the group name.
	std::string StripTrailingYear(const std::string& name){
		if(name.size() < 5) return name;
		size_t end = name.size();
		size_t yStart = end - 4;
		bool allDigits = true;
		for(size_t i = yStart; i < end; i++){
			if(name[i] < '0' || name[i] > '9'){ allDigits = false; break; }
		}
		if(!allDigits) return name;
		// Need at least one whitespace before the year so we don't chop
		// the digits off something like "Revy2025" (treated as the whole
		// name, since the user wrote it without a separator).
		if(yStart == 0 || (name[yStart - 1] != ' ' && name[yStart - 1] != '\t')){
			return name;
		}
		size_t cut = yStart;
		while(cut > 0 && (name[cut - 1] == ' ' || name[cut - 1] == '\t')) cut--;
		return name.substr(0, cut);
	}

	// (1.6.2) Drop a trailing "Jubi" suffix so jubilee editions collapse
	// into their parent revy group (e.g. "MatRevyJubi" → "MatRevy"). The
	// suffix is recognised as a literal trailing token; anything attached
	// after Jubi (none in current usage) would suppress the strip and
	// keep the name intact.
	std::string StripJubiSuffix(const std::string& name){
		const std::string suffix = "Jubi";
		if(name.size() <= suffix.size()) return name;
		if(name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0){
			return name;
		}
		return name.substr(0, name.size() - suffix.size());
	}

	std::string ToLower(std::string s){
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return (char)std::tolower(c); });
		return s;
	}
}

void LoadProjectScreen::OnEnter(){
	searchField.label = "Search";
	searchField.maxLength = 64;
	searchField.Clear();

	// (1.6.4) Force-close both dropdowns on entry. Without this an ESC
	// while either was open leaves `expanded=true`, so the next visit
	// shows the menu open until the user re-navigates onto it.
	sortDropdown.Close();
	filterDropdown.Close();

	sortDropdown.label = "Sort";
	sortDropdown.SetOptions({"Year", "Recent", "Name"});

	filterDropdown.label = "Revy";
	// Options are populated from the live scan in ScanProjects so the
	// list always reflects what's actually on disk. Default seeded here
	// keeps the dropdown drawable before the first scan completes.
	filterDropdown.SetOptions({"All"});

	focus = LoadFocus::List;
	scrollOffset = 0;
	selectedRow = 0;

	ScanProjects();
	Rebuild();
	app->GetInput().SetTextInputMode(false);
}

void LoadProjectScreen::OnExit(){
	app->GetInput().SetTextInputMode(false);
}

void LoadProjectScreen::ScanProjects(){
	auto& config = Config::Instance();
	projects.clear();
	if(config.projectsDir.empty() || !fs::exists(config.projectsDir)) return;

	for(auto& entry : fs::directory_iterator(config.projectsDir)){
		if(!entry.is_directory()) continue;
		auto schemaPath = entry.path() / "schema.toml";
		if(!fs::exists(schemaPath)) continue;

		// Lightweight schema parse — no scene files. SchemaParser already
		// only touches schema.toml, so this is the cheapest path.
		RevySchema schema = SchemaParser::ParseFile(schemaPath.string());

		ProjectInfo info;
		info.folderName   = entry.path().filename().string();
		info.path         = entry.path().string();
		info.fullRevyName = schema.revyName;
		info.year         = schema.year;
		info.creator      = schema.creatorName;
		info.groupName    = StripJubiSuffix(StripTrailingYear(schema.revyName));
		// Empty group → fall back to the disk folder so the row still
		// renders under *something*. Shouldn't happen on a well-formed
		// project, but defending against an empty Revy field is cheap.
		if(info.groupName.empty()) info.groupName = info.folderName;

		std::error_code ec;
		info.mtime = fs::last_write_time(entry.path(), ec);

		projects.push_back(std::move(info));
	}

	// (1.6.2) Derive the filter dropdown's option list from the live
	// scan. Always start with "All", then every distinct group name
	// alphabetically. If the user had a non-"All" filter selected from
	// a previous visit and the group still exists, preserve it; if the
	// group is gone (project deleted), fall back to "All".
	std::string previous = filterDropdown.GetSelectedIndex() >= 0
		? filterDropdown.GetSelectedOption() : "All";
	std::vector<std::string> uniqueGroups;
	for(const auto& info : projects){
		if(std::find(uniqueGroups.begin(), uniqueGroups.end(), info.groupName)
			== uniqueGroups.end()){
			uniqueGroups.push_back(info.groupName);
		}
	}
	std::sort(uniqueGroups.begin(), uniqueGroups.end(),
		[](const std::string& a, const std::string& b){ return ToLower(a) < ToLower(b); });
	std::vector<std::string> options;
	options.reserve(uniqueGroups.size() + 1);
	options.push_back("All");
	for(auto& g : uniqueGroups) options.push_back(g);
	filterDropdown.SetOptions(options);
	for(int i = 0; i < (int)options.size(); i++){
		if(options[i] == previous){
			filterDropdown.SetSelectedIndex(i);
			break;
		}
	}
}

void LoadProjectScreen::Rebuild(){
	rows.clear();

	// (1.6.2) Filter dropdown — when set to a specific Revy group, hide
	// every project that doesn't belong to it. "All" passes everything
	// through. The group keys here match what ScanProjects derives, so
	// "MatRevy" picks up MatRevy and MatRevyJubi together.
	const std::string& filterChoice = filterDropdown.GetSelectedIndex() >= 0
		? filterDropdown.GetSelectedOption() : std::string("All");

	// Search filter — case-insensitive substring on the project's full
	// revy name. Matches the Revy-name column the user actually reads.
	std::string needle = ToLower(searchField.GetText());
	std::vector<size_t> matches;
	matches.reserve(projects.size());
	for(size_t i = 0; i < projects.size(); i++){
		if(filterChoice != "All" && projects[i].groupName != filterChoice){
			continue;
		}
		if(needle.empty()){
			matches.push_back(i);
			continue;
		}
		std::string hay = ToLower(projects[i].fullRevyName);
		if(hay.find(needle) != std::string::npos) matches.push_back(i);
	}

	// Sort selection drives the order *within* each group. Groups
	// themselves are always alphabetical so layout stays predictable
	// across re-sorts.
	const std::string& mode = sortDropdown.GetSelectedOption();
	auto byYearDesc = [&](size_t a, size_t b){
		return projects[a].year > projects[b].year;
	};
	auto byMtimeDesc = [&](size_t a, size_t b){
		return projects[a].mtime > projects[b].mtime;
	};
	auto byNameAsc = [&](size_t a, size_t b){
		return ToLower(projects[a].fullRevyName) < ToLower(projects[b].fullRevyName);
	};

	// Bucket by group, then explicitly sort group order alphabetically
	// (case-insensitive). std::unordered_map keeps the bucket lookup O(1)
	// without imposing its own ordering — the visible order comes from
	// `groupOrder` and is independent of insertion timing.
	std::vector<std::string> groupOrder;
	std::unordered_map<std::string, std::vector<size_t>> buckets;
	for(size_t idx : matches){
		const auto& g = projects[idx].groupName;
		auto it = std::find(groupOrder.begin(), groupOrder.end(), g);
		if(it == groupOrder.end()) groupOrder.push_back(g);
		buckets[g].push_back(idx);
	}
	std::sort(groupOrder.begin(), groupOrder.end(),
		[](const std::string& a, const std::string& b){ return ToLower(a) < ToLower(b); });

	for(const auto& g : groupOrder){
		LoadRow header;
		header.isHeader = true;
		header.headerText = g;
		rows.push_back(std::move(header));

		auto& bucket = buckets[g];
		if(mode == "Recent")     std::sort(bucket.begin(), bucket.end(), byMtimeDesc);
		else if(mode == "Name")  std::sort(bucket.begin(), bucket.end(), byNameAsc);
		else                     std::sort(bucket.begin(), bucket.end(), byYearDesc);

		for(size_t pIdx : bucket){
			LoadRow row;
			row.isHeader = false;
			row.projectIdx = (int)pIdx;
			rows.push_back(std::move(row));
		}
	}

	// Re-anchor selection on the first project row whenever the row set
	// changes. Keeping the previous selectedRow would risk landing on a
	// header or out-of-bounds after a filter shrink.
	int first = FirstProjectRow();
	selectedRow = first >= 0 ? first : 0;
	scrollOffset = 0;
}

int LoadProjectScreen::FirstProjectRow() const{
	for(size_t i = 0; i < rows.size(); i++){
		if(!rows[i].isHeader) return (int)i;
	}
	return -1;
}

int LoadProjectScreen::LastProjectRow() const{
	for(int i = (int)rows.size() - 1; i >= 0; i--){
		if(!rows[i].isHeader) return i;
	}
	return -1;
}

void LoadProjectScreen::MoveSelection(int delta){
	if(rows.empty()) return;
	int cur = selectedRow;
	int step = delta > 0 ? 1 : -1;
	int remaining = std::abs(delta);
	while(remaining > 0){
		int next = cur + step;
		// Skip past header rows. We never let the cursor land on one.
		while(next >= 0 && next < (int)rows.size() && rows[next].isHeader){
			next += step;
		}
		if(next < 0 || next >= (int)rows.size()) break;
		cur = next;
		remaining--;
	}
	selectedRow = cur;
}

void LoadProjectScreen::JumpSelectionToTop(){
	int first = FirstProjectRow();
	if(first >= 0) selectedRow = first;
}

void LoadProjectScreen::JumpSelectionToBottom(){
	int last = LastProjectRow();
	if(last >= 0) selectedRow = last;
}

void LoadProjectScreen::SetFocus(LoadFocus f){
	focus = f;
	searchField.focused   = (f == LoadFocus::Search);
	filterDropdown.focused = (f == LoadFocus::Filter);
	sortDropdown.focused  = (f == LoadFocus::Sort);
	if(f != LoadFocus::Sort)   sortDropdown.Close();
	if(f != LoadFocus::Filter) filterDropdown.Close();
	app->GetInput().SetTextInputMode(f == LoadFocus::Search);
}

void LoadProjectScreen::OpenSelected(){
	if(selectedRow < 0 || selectedRow >= (int)rows.size()) return;
	const auto& row = rows[selectedRow];
	if(row.isHeader || row.projectIdx < 0) return;
	const auto& info = projects[row.projectIdx];
	ProjectData data = Project::Load(info.path);
	app->OpenProject(data);
}

void LoadProjectScreen::HandleInput(InputAction action){
	auto& input = app->GetInput();

	// ESC always defocuses to the list, or returns to MainMenu when
	// already on the list.
	if(action == InputAction::Quit){
		if(focus != LoadFocus::List){
			SetFocus(LoadFocus::List);
		} else{
			app->SwitchScreen(ScreenID::MainMenu);
		}
		return;
	}

	// Tab cycles focus regardless of which mode we're in.
	if(action == InputAction::Tab){
		LoadFocus next = LoadFocus::List;
		if(focus == LoadFocus::List)        next = LoadFocus::Search;
		else if(focus == LoadFocus::Search) next = LoadFocus::Filter;
		else if(focus == LoadFocus::Filter) next = LoadFocus::Sort;
		else                                next = LoadFocus::List;
		SetFocus(next);
		return;
	}

	if(focus == LoadFocus::Search){
		if(action == InputAction::TextInput){
			searchField.AppendText(input.GetLastTextInput());
			Rebuild();
			return;
		}
		if(action == InputAction::Backspace){
			searchField.RemoveLastChar();
			Rebuild();
			return;
		}
		// ENTER on the search field jumps to the list — common pattern
		// when typing then committing a query.
		if(action == InputAction::Execute){
			SetFocus(LoadFocus::List);
		}
		return;
	}

	// Helper to share the dropdown navigation logic between Filter and
	// Sort. Both behave identically: ENTER toggles expand, Up/Down moves
	// the selection (live, with a Rebuild so the table reflects the
	// choice without an explicit confirm).
	auto handleDropdown = [&](Dropdown& dd) -> bool{
		if(dd.IsExpanded()){
			switch(action){
				case InputAction::NavUp:   dd.SelectPrev(); Rebuild(); return true;
				case InputAction::NavDown: dd.SelectNext(); Rebuild(); return true;
				case InputAction::Execute: dd.Close();      Rebuild(); return true;
				default: break;
			}
		}
		if(action == InputAction::Execute){ dd.Toggle();     return true; }
		if(action == InputAction::NavUp){   dd.SelectPrev(); Rebuild(); return true; }
		if(action == InputAction::NavDown){ dd.SelectNext(); Rebuild(); return true; }
		return false;
	};

	if(focus == LoadFocus::Filter){
		handleDropdown(filterDropdown);
		return;
	}

	if(focus == LoadFocus::Sort){
		handleDropdown(sortDropdown);
		return;
	}

	// LoadFocus::List
	switch(action){
		case InputAction::NavUp:      MoveSelection(-1); break;
		case InputAction::NavDown:    MoveSelection(1); break;
		case InputAction::JumpUp:     MoveSelection(-JUMP_STEP); break;
		case InputAction::JumpDown:   MoveSelection(JUMP_STEP); break;
		case InputAction::JumpTop:    JumpSelectionToTop(); break;
		case InputAction::JumpBottom: JumpSelectionToBottom(); break;
		case InputAction::Execute:    OpenSelected(); break;
		case InputAction::NavLeft:
			app->SwitchScreen(ScreenID::MainMenu);
			break;
		default: break;
	}
}

void LoadProjectScreen::Update(float deltaTime){ (void)deltaTime; }

void LoadProjectScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);
	auto* renderer = r.GetMaster();
	int w = r.GetMasterWidth();
	int h = r.GetMasterHeight();
	int centerX = w / 2;

	text.DrawTextCentered(renderer, "Load Project", centerX, 30, Colours::WHITE);

	// ── Top row: search field + revy filter + sort dropdown ──────
	int topY = 70;
	int searchW = std::min(360, w / 2 - 60);
	int filterW = 200;
	int sortW   = 160;
	int gap     = 30;
	int totalW  = searchW + gap + filterW + gap + sortW;
	int startX  = (w - totalW) / 2;

	searchField.x = startX;
	searchField.y = topY;
	searchField.w = searchW;
	searchField.Draw(r, text);

	filterDropdown.x = startX + searchW + gap;
	filterDropdown.y = topY;
	filterDropdown.w = filterW;
	filterDropdown.Draw(r, text);

	sortDropdown.x = startX + searchW + gap + filterW + gap;
	sortDropdown.y = topY;
	sortDropdown.w = sortW;
	sortDropdown.Draw(r, text);

	// ── Project table ─────────────────────────────────────────────
	int tableX = 60;
	int tableY = topY + 80;
	int tableW = w - 120;
	int tableH = h - tableY - 40;

	// Light divider above the table for visual separation from the
	// filters.
	r.DrawRect(renderer, tableX, tableY - 12, tableW, 1,
		{0x40, 0x50, 0x60, 0xFF});

	int lineH    = text.GetLineHeight();
	int rowH     = lineH + 8;
	int headerH  = lineH + 6;

	// Two-column layout: name on the left, creator on the right. Creator
	// column is right-anchored so longer names don't push it around.
	int nameX    = tableX + 16;
	int creatorRightX = tableX + tableW - 16;

	if(rows.empty()){
		std::string msg = projects.empty()
			? "No projects found in projectsDir."
			: "No projects match the search.";
		text.DrawTextCentered(renderer, msg.c_str(),
			centerX, tableY + 40, Colours::WHITE);
	}

	// Compute how many rows fit. Each row takes either rowH (project)
	// or headerH (group header) — keep it simple by treating both as
	// rowH for visibility math.
	int maxVisibleRows = tableH / rowH;
	if(maxVisibleRows < 1) maxVisibleRows = 1;

	// Keep selection inside the visible window.
	if(selectedRow < scrollOffset) scrollOffset = selectedRow;
	if(selectedRow >= scrollOffset + maxVisibleRows){
		scrollOffset = selectedRow - maxVisibleRows + 1;
	}
	if(scrollOffset < 0) scrollOffset = 0;
	if(scrollOffset > (int)rows.size() - maxVisibleRows){
		scrollOffset = std::max(0, (int)rows.size() - maxVisibleRows);
	}

	// Overflow indicator above the table when scrolled.
	if(scrollOffset > 0){
		text.DrawText(renderer, "...", nameX, tableY - rowH,
			Colours::WHITE);
	}

	for(int i = 0; i < maxVisibleRows; i++){
		int rowIdx = scrollOffset + i;
		if(rowIdx >= (int)rows.size()) break;
		const auto& row = rows[rowIdx];
		int yPx = tableY + i * rowH;

		if(row.isHeader){
			// Section header: bold-looking divider line + group title.
			Colour div = {0x50, 0x60, 0x70, 0xFF};
			r.DrawRect(renderer, nameX, yPx + headerH - 2,
				tableW - 32, 1, div);
			text.DrawText(renderer, row.headerText, nameX, yPx + 2,
				Colours::ORANGE);
		} else{
			if(rowIdx == selectedRow){
				r.DrawRect(renderer, tableX + 2, yPx,
					tableW - 4, rowH, Colours::HIGHLIGHT);
			}
			const auto& info = projects[row.projectIdx];
			Colour rowC = (rowIdx == selectedRow)
				? Colours::ORANGE : Colours::WHITE;
			text.DrawText(renderer, info.fullRevyName,
				nameX, yPx + 4, rowC);

			// Right-align the creator column. Empty creator (legacy
			// project pre-1.6.2) renders as a dim em-dash so the column
			// reads obviously-blank rather than just absent.
			std::string creator = info.creator.empty() ? "—" : info.creator;
			Colour creatorC = info.creator.empty()
				? Colour{0x70, 0x80, 0x90, 0xFF}
				: rowC;
			int cw = text.MeasureWidth(creator);
			text.DrawText(renderer, creator,
				creatorRightX - cw, yPx + 4, creatorC);
		}
	}

	if(scrollOffset + maxVisibleRows < (int)rows.size()){
		text.DrawText(renderer, "...", nameX,
			tableY + maxVisibleRows * rowH, Colours::WHITE);
	}

	// ── Footer hint ───────────────────────────────────────────────
	std::string hint = "TAB switch focus  |  ENTER open  |  ESC back";
	text.DrawTextCentered(renderer, hint, centerX, h - 25, Colours::WHITE);

	// Dropdown overlays last so they sit on top of the table.
	filterDropdown.DrawOverlay(r, text);
	sortDropdown.DrawOverlay(r, text);
}

} // namespace SatyrAV
