#pragma once
#include "screens/Screen.hpp"
#include "core/CueEngine.hpp"
#include "core/MarkLog.hpp"
#include "core/BackstageSender.hpp"
#include "core/ShowLog.hpp"
#include "core/Subtitles.hpp"
#include "core/Types.hpp"
#include "render/MasterWindow.hpp"
#include "render/SlaveWindow.hpp"
#include "render/MediaPlayer.hpp"
#include "render/MediaCache.hpp"
#include "ui/ListView.hpp"
#include <unordered_map>

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
	// Read-only access to the loaded show.
	const CueEngine& GetCueEngine() const{ return engine; }

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
	// (1.6.1) Push the engine's effective font size, outline, font-style
	// face paths, background colour, and capitalize flag to the renderers
	// for the current scene. Called from every site that changes scene.
	void ApplySceneOptionsCascade();

	// Debug tracking
	std::string lastCommandStr;
	std::string lastCommandPath;


	// (2.0.1) Mark-line log (`O`). The last executed cue is captured when
	// it runs; `O` appends it to fix-it/ErrorInFiles-<date>.LOG and shows
	// a short confirmation on the master.
	void MarkLastExecutedLine();

	// (2.1.0) BACKSPACE: wipe the slave the way `clearAll` does, at any
	// time, without needing a cue for it. Video and sound are left alone
	// (K / M handle those).
	void ClearScreen();
	// `,` / `.` / SHIFT+`,`: seek the playing video. Logged, not a cue.
	void SeekVideoKey(double delta, const char* key, bool toStart = false);
	MarkedLine lastExecuted;
	bool haveLastExecuted = false;
	std::string markStatus;
	bool markStatusOk = false;
	float markStatusUntil = 0.0f;

	// (2.0.2) Backstage screen. NotifyBackstage is called wherever the show
	// moves into a scene (the ENTER scene advance, and the first cue run in
	// a scene the show was not in); it sends at most once per scene change,
	// tracked by the akt/scene last notified.
	void NotifyBackstage(int aktIdx, int sceneIdx);
	BackstageSender backstage;
	int backstageAkt   = -1;
	int backstageScene = -1;

	// (2.1.0) Show time log (see core/ShowLog.hpp) and the clock /
	// stopwatches drawn on the master when Options turns them on. Both
	// start with the first cue of the session: before that there is no
	// show to time.
	void StartShowIfNeeded();
	// Logs "ended scene (M:SS)" for the scene being left and "started
	// scene" for the new one, and restarts the scene stopwatch. Does
	// nothing when the scene is the one already logged.
	void EnterSceneForLog(int aktIdx, int sceneIdx);
	// One event in the show log, under the scene last entered.
	void LogEvent(const std::string& event);
	void EndShow();
	ShowTime CurrentShowTime() const;
	double SecondsSince(double startTicks) const;
	void DrawClock(Renderer& r, TextRenderer& text);

	// (2.1.0) Font files. A name as written in a scene (`use X.ttf`, or the
	// Font options) is looked for in the project's `fonts/` and `font/`
	// folders, then the project folder, then the shared FONTS folder; a
	// path with a folder in it is tried against the project first.
	// Answers are cached until scenes are reloaded or the project changes.
	std::string ResolveFontFile(const std::string& name);
	void ResolveRunFonts(std::vector<TextRun>& runs);
	std::unordered_map<std::string, std::string> fontFileCache;
	// Open every font and size the current scene's text uses, so the first
	// line in a `use` font or `fontSize` doesn't wait for the file.
	void PreloadSceneFonts();

	// (2.1.0) Running [Animation]s. Each starts a pass (its show/text
	// command with a pathMove) every `every` seconds until its repeat
	// count or time limit is reached. Passes are tagged with the group
	// kAnimationGroupPrefix + name and remove themselves when their run
	// ends (RenderModifiers::lifetimeSec).
	//   soft stop (`stop NAME`, `clear`, `clearImages`): no new passes;
	//     the ones on screen finish.
	//   hard cut (`clear NAME`, `clearAll`, BACKSPACE, scene change): the
	//     passes on screen go too.
	struct RunningAnimation{
		AnimationDef def;
		double startSec = 0;
		double nextPassSec = 0;
		int passes = 0;
	};
	std::vector<RunningAnimation> runningAnimations;
	static const char* const kAnimationGroupPrefix;
	void StartAnimation(const AnimationDef& def);
	void StopAnimation(const std::string& name, bool hard);
	void StopAllAnimations(bool hard);
	void UpdateAnimations();
	void DispatchSlaveCommand(const Command& cmd);

	ShowLogWriter showLog;
	bool   showStarted     = false;
	double showStartTicks  = 0;  // SDL_GetTicksNS-based seconds
	double sceneStartTicks = 0;
	int    logAkt   = -1;
	int    logScene = -1;
	std::string logAktLabel;     // "Akt 1"
	std::string logSceneLabel;   // "Scene BandIntro"
	double actStartTicks   = 0;

	// Per-act .srt / .ass (core/Subtitles.hpp), next to the .log. Open
	// while the show log is on and an act is running.
	ActSubtitleWriter actSubs;
	std::string showLogBase;     // the run's path without extension
	// Pictures and video coming and going, for the .log, across the
	// whole show (not cut at act boundaries like the subtitle files).
	SubtitleTrack logMedia;
	void OpenActSubtitles(int aktIdx);
	// Hands the slave's current text, pictures and video to the act's
	// subtitles and the log. Every frame while the show log is open.
	void UpdateShowLogMedia();
	void SnapshotSlave(std::vector<SubLine>& lines, std::vector<SubMedia>& mediaOut);
};

} // namespace SatyrAV
