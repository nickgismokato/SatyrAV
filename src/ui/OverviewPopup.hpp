#pragma once
#include "ui/Popup.hpp"
#include "core/Types.hpp"
#include <string>
#include <vector>

namespace SatyrAV{

class CueEngine; // fwd

// (1.6.3) Modal Overview Debug popup. Reachable via the `D → O` chord
// while the Debug popup is open and the user is inside a project.
// Walks every scene of the active project and reports:
//   • parser warnings (e.g. unknown keywords) per scene
//   • missing media files referenced by `show` / `play` commands
//
// The walk runs once on `Open()` and the results are cached until the
// popup is closed — projects with many scenes don't re-scan on every
// frame. ESC dismisses; UP/DOWN scrolls; nothing else is interactive.
class OverviewPopup : public Popup{
public:
	void Draw(Renderer& r, TextRenderer& text) override;

	// `Open(engine, projectBasePath)` performs the walk and shows the
	// popup. `projectBasePath` is needed so the missing-file check can
	// resolve `show`/`play` arguments against the project's media folders.
	void Open(const CueEngine& engine, const std::string& projectBasePath);
	void Close();

	void ScrollUp(int steps = 1);
	void ScrollDown(int steps = 1);

private:
	// Per-scene findings. Empty `warnings` and `missingPaths` means the
	// scene is clean — we still emit a row for it so the user can see
	// total scene count at a glance.
	struct SceneEntry{
		std::string aktSceneLabel; // e.g. "Akt 2 / GrpExample"
		std::string fileName;       // .ngk filename (sans extension)
		int commandCount = 0;
		std::vector<Scene::Warning> warnings;
		std::vector<std::string> missingPaths; // pretty-printed: "<cmdtype>: <path>"
	};

	std::vector<SceneEntry> entries;
	int scroll = 0; // first visible entry index
	int totalWarnings = 0;
	int totalMissing  = 0;
};

} // namespace SatyrAV
