#pragma once
#include "screens/Screen.hpp"
#include "core/CueEngine.hpp"
#include "core/Types.hpp"
#include "render/MasterWindow.hpp"
#include "render/SlaveWindow.hpp"
#include "render/MediaPlayer.hpp"
#include "render/MediaCache.hpp"
#include "ui/ListView.hpp"

namespace SatyrAV{

class PerformanceScreen : public Screen{
public:
	void OnEnter() override;
	void OnExit() override;
	void HandleInput(InputAction action) override;
	void Update(float deltaTime) override;
	void Draw(Renderer& r, TextRenderer& text) override;

	void SetProject(const ProjectData& project);

	// Accessors for the display-size popup — the popup lives at App level
	// but needs to mutate the screen's targeted-display rect and push a
	// test pattern onto the slave.
	TargetedDisplay& GetTargetedDisplay(){ return targetedDisplay; }
	SlaveWindow& GetSlaveWindow(){ return slaveUI; }
	const std::string& GetProjectPath() const{ return projectBasePath; }

private:
	CueEngine engine;
	MasterWindow masterUI;
	SlaveWindow slaveUI;
	MediaPlayer media;
	MediaCache cache;

	// Navigator state
	int activeColumn = 0;
	int primedAkt    = -1;
	int primedScene  = -1;
	int primedCmd    = -1;

	// Saved positions for returning to a column
	int savedScene   = 0;
	int savedCmd     = 0;

	// Project context
	std::string projectBasePath;
	TargetedDisplay targetedDisplay;
	float deltaTime = 0.0f;
	bool awaitingFirstCommand = false; // True after auto-advancing to new scene
	bool awaitingSceneAdvance = false; // True after executing last command in scene

	void NavigateRight();
	void NavigateLeft();
	void NavigateUp(int steps = 1);
	void NavigateDown(int steps = 1);
	void JumpToTop();
	void JumpToBottom();
	void ExecuteCommand();
	// Step the prime cursor backward one executable command. Does NOT undo
	// side effects (audio/video/text already shown stay live); see TODO note
	// against `Shift+ENTER` — true undo was deferred in 1.6.0.
	void StepBack();
	void AdvanceToNextScene();
	// Re-parse every scene .ngk from disk (U key). Keeps the current
	// position and the in-memory cache; media/particles are left running
	// so a reload mid-scene is unobtrusive.
	void ReloadScenes();

	void OnSlaveCommand(const Command& cmd);
	void UpdateDebugInfo();
	void PreloadForCurrentScene();

	// Debug tracking
	std::string lastCommandStr;
	std::string lastCommandPath;
};

} // namespace SatyrAV
