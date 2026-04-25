#pragma once
#include "core/Types.hpp"
#include <functional>

namespace SatyrAV{

// Callback for when the slave display needs updating
using SlaveUpdateFn = std::function<void(const Command&)>;

class CueEngine{
public:
	void LoadProject(const ProjectData& project);
	// Re-parses every scene .ngk from disk while preserving the current
	// akt/scene/command selection (command index is clamped to the new
	// command count, falling back to the nearest executable row). Leaves
	// the schema itself (akt structure) alone.
	void ReloadScenes();
	void SetSlaveCallback(SlaveUpdateFn callback);

	// Navigation
	void SelectAkt(int index);
	void SelectScene(int index);
	void SetCommandIndex(int index);
	void ExecuteCurrentCommand();

	// Accessors
	int GetCurrentAktIndex() const;
	int GetCurrentSceneIndex() const;
	int GetCurrentCommandIndex() const;
	int GetAktCount() const;
	int GetSceneCount() const;
	int GetCommandCount() const;

	const Akt* GetCurrentAkt() const;
	const Scene* GetCurrentScene() const;
	const Command* GetCurrentCommand() const;
	const Scene* GetNextScene() const;
	bool IsLastScene() const;

	// Data access by index (for navigator drawing)
	const Akt* GetAkt(int index) const;
	const Scene* GetScene(int aktIdx, int sceneIdx) const;
	const Command* GetCommand(int aktIdx, int sceneIdx, int cmdIdx) const;
	int GetSceneCountForAkt(int aktIdx) const;
	int GetCommandCountForScene(int aktIdx, int sceneIdx) const;
	const std::string& GetRevyName() const;
	int GetRevyYear() const;

	// Executable-command navigation (skips CommandType::Comment rows).
	// Returns -1 when no executable command matches the query.
	int FirstExecutableIndex(int aktIdx, int sceneIdx) const;
	int LastExecutableIndex(int aktIdx, int sceneIdx) const;
	// Search from `from` inclusive, in the given direction.
	int NextExecutableIndex(int aktIdx, int sceneIdx, int from) const;
	int PrevExecutableIndex(int aktIdx, int sceneIdx, int from) const;

	// Options cascade: scene -> project -> global
	int GetEffectiveFontSize() const;
	Colour GetEffectiveBackgroundColour() const;
	int GetTextOutline() const;
	int GetProjectFontSize() const;
	int GetSceneFontSize() const;
	bool GetEffectiveCapitalize() const;

	// (1.6.1) Cascade for the four font-style face paths. The regular
	// path falls back to the Config-level path if scene/project leave it
	// empty; the styled paths return empty when nothing is configured for
	// that variant — TextRenderer treats empty as "use regular".
	std::string GetEffectiveFontPath(bool bold, bool italic) const;

	// (1.6.1) Project-level subtitle defaults for `textD`. These are
	// project-only (no scene override yet); built-in defaults live on
	// RevySchema. Read every time a subtitle is pushed so live edits to
	// schema.toml take effect on the next ReloadScenes.
	bool   GetSubtitleItalic() const;
	Colour GetSubtitleColour() const;
	float  GetSubtitleTransparency() const;
	float  GetSubtitlePosY() const;

	// (1.4) Resolve the effective particle tuning for `type` by walking
	// scene → project → built-in defaults. Only fields the author explicitly
	// set override the level below; everything else falls through.
	ParticleTuning GetEffectiveParticleTuning(ParticleType type) const;

	// Media control
	void StopAllMedia();
	void ToggleVideo();
	void ToggleMusic();

private:
	ProjectData projectData;
	SlaveUpdateFn slaveCallback;

	int currentAktIdx     = -1;
	int currentSceneIdx   = -1;
	int currentCmdIdx     = -1;

	void ExecuteCommand(const Command& cmd);
};

} // namespace SatyrAV
