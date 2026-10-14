#include "screens/PerformanceScreen.hpp"
#include "App.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"
#include "core/TextPlan.hpp"
#include <filesystem>

namespace SatyrAV{

static std::string ResolvePath(const std::string& base, const std::string& arg){
	std::string fullPath = base + "/" + arg;
	if(std::filesystem::exists(fullPath)) return fullPath;
	const char* SUBDIRS[] = {"pictures", "movies", "sound", nullptr};
	for(int i = 0; SUBDIRS[i]; i++){
		std::string tryPath = base + "/" + SUBDIRS[i] + "/" + arg;
		if(std::filesystem::exists(tryPath)) return tryPath;
	}
	return fullPath;
}

void PerformanceScreen::OnEnter(){
	media.Init();
	engine.SetSlaveCallback([this](const Command& cmd){ OnSlaveCommand(cmd); });

	auto& config = Config::Instance();
	app->GetTextRenderer().SetSlaveFontSize(config.fontSize);

	// (1.6.4) Drop any text / images / particles left over from a previous
	// project session. The slaveUI member is reused across project opens, so
	// without this the previous project's text would survive an ESC + re-
	// enter and be redrawn against the fresh global font size — looking
	// like the project font size silently reverted. ClearAll preserves
	// smooth-stopping particle systems by design (see SlaveWindow), but
	// nothing should be smooth-stopping immediately after project entry.
	slaveUI.ClearAll();
	slaveUI.StopParticlesAll();
	media.StopAll();

	activeColumn = 0;
	primedAkt = -1;
	primedScene = -1;
	primedCmd = -1;

	// (2.0.1) Nothing has run in this session yet.
	haveLastExecuted = false;
	markStatus.clear();
	// (2.0.2) ...so the first scene that runs is news to backstage too.
	backstageAkt = -1;
	backstageScene = -1;
}

void PerformanceScreen::OnExit(){
	// (1.6.4) Drop slave content explicitly here too — protects against
	// future code paths that bring slaveUI back into view without first
	// going through OnEnter (e.g. a future "preview" or "edit" mode that
	// also draws the slave layers).
	slaveUI.ClearAll();
	slaveUI.StopParticlesAll();
	media.Shutdown();
	cache.Clear();
	app->GetRenderer().CloseSlave();
	// (2.0.2) Stop posting once the project is closed. Not Shutdown(): that
	// would wait for a request in flight, and leaving a project must not
	// hang on the network. The worker is reused by the next project.
	backstage.Configure("");
}

void PerformanceScreen::SetProject(const ProjectData& project){
	projectBasePath = project.projectPath;
	targetedDisplay = project.schema.targetedDisplay;

	// (2.0.1) The fix-it folder for the `O` mark-line log. Opening a
	// project only makes the folder; the day's file waits for a mark.
	if(!MarkLog::EnsureFolder(projectBasePath)){
		Logger::Instance().Warnf("Could not create the fix-it folder in %s",
			projectBasePath.c_str());
	}

	// (2.0.2) Backstage screen: on when schema.toml has a [Backstage] Url
	// and Enabled is not false.
	if(!project.schema.backstageEnabled && !project.schema.backstageUrl.empty()){
		Logger::Instance().Infof("Backstage: switched off in schema.toml (Enabled = false)");
		backstage.Configure("");
	} else{
		backstage.Configure(project.schema.backstageUrl);
	}

	// If the project has never had a targeted rect saved, default to
	// filling the production-slave canvas (intendedSlave dims in windowed
	// mode, slave bounds in fullscreen). Using canvas dims here keeps
	// the rect's coord space consistent with how it's interpreted by
	// CompositeTargetedDisplay — otherwise a windowed-mode default would
	// inherit the live window size, which would shrink/distort once the
	// composite letterboxes back into intendedSlave space.
	auto& r = app->GetRenderer();
	if(targetedDisplay.width <= 0 || targetedDisplay.height <= 0){
		targetedDisplay.width   = r.GetCanvasWidth();
		targetedDisplay.height  = r.GetCanvasHeight();
		targetedDisplay.centerX = r.GetCanvasWidth() / 2;
		targetedDisplay.centerY = r.GetCanvasHeight() / 2;
		targetedDisplay.rotation = 0.0f;
	}
	engine.LoadProject(project);
}

void PerformanceScreen::HandleInput(InputAction action){
	switch(action){
		case InputAction::NavRight:   NavigateRight(); break;
		case InputAction::NavLeft:    NavigateLeft(); break;
		case InputAction::NavUp:      NavigateUp(); break;
		case InputAction::NavDown:    NavigateDown(); break;
		case InputAction::JumpUp:     NavigateUp(JUMP_STEP); break;
		case InputAction::JumpDown:   NavigateDown(JUMP_STEP); break;
		case InputAction::JumpTop:    JumpToTop(); break;
		case InputAction::JumpBottom: JumpToBottom(); break;
		case InputAction::Execute:    ExecuteCommand(); break;
		case InputAction::StepBack:   StepBack(); break;
		case InputAction::ToggleVideo: media.ToggleVideoPause(); break;
		case InputAction::ToggleMusic: media.ToggleAudioPause(); break;
		case InputAction::ReloadScenes: ReloadScenes(); break;
		case InputAction::MarkLine:  MarkLastExecutedLine(); break;
		case InputAction::FunctionKey:{
			// (1.6.3) F1..F12 audio hotkey. Same key pressed twice while
			// the same hotkey audio is still playing → stop (toggle).
			// Useful for long ambient loops (knocking, alarm) where the
			// performer drives both start and stop with one key.
			// (2.0.0) A different F-key no longer replaces the running
			// sound: `PlayAudio` now layers sounds, so several keys can
			// play at once and over a video. Note that this still tracks
			// only the last key pressed — see the "maybe" item about F-key
			// toggling under 2.1.0 in docs/TODO.md before changing it.
			int num = app->GetInput().GetLastFunctionKey();
			std::string fname = engine.GetFunctionKeyAudio(num);
			if(fname.empty()) break;
			std::string full = ResolvePath(projectBasePath, fname);

			if(currentHotkeyNum == num && media.IsAudioPlaying()){
				media.StopAudio(full);
				lastCommandStr = "F" + std::to_string(num) + ": stop " + fname;
				lastCommandPath = full;
				currentHotkeyNum = 0;
			} else{
				media.PlayAudio(full);
				lastCommandStr = "F" + std::to_string(num) + ": play " + fname;
				lastCommandPath = full;
				currentHotkeyNum = num;
			}
			break;
		}
		case InputAction::Quit:
			app->SwitchScreen(ScreenID::MainMenu);
			break;
		default: break;
	}
}

void PerformanceScreen::Update(float dt){
	deltaTime = dt;
	media.Update();
	UpdateDebugInfo();
}

void PerformanceScreen::Draw(Renderer& r, TextRenderer& text){
	r.ClearMaster(Colours::BACKGROUND);

	masterUI.DrawHeader(r, text, engine);

	auto* renderer = r.GetMaster();
	std::string title = engine.GetRevyName();
	text.DrawTextCentered(renderer, title,
		r.GetMasterWidth() / 2, 68, Colours::ORANGE);

	masterUI.DrawNavigator(r, text, engine,
		primedAkt, primedScene, primedCmd, activeColumn);

	// Status bar at bottom
	int statusY = r.GetMasterHeight() - 25;
	if(awaitingSceneAdvance){
		auto* next = engine.GetNextScene();
		std::string msg = next
			? ">> Press ENTER for next scene: " + next->name + " >>"
			: ">> THE END — Press ENTER >>";
		text.DrawTextCentered(renderer, msg,
			r.GetMasterWidth() / 2, statusY, Colours::LIME_GREEN);
	} else if(awaitingFirstCommand){
		text.DrawTextCentered(renderer, "Scene ready — Press ENTER to begin",
			r.GetMasterWidth() / 2, statusY, Colours::ORANGE);
	}

	// (2.0.1) Confirmation for an `O` mark, just above the status line so
	// it never hides the "Press ENTER" prompts.
	if(!markStatus.empty() && SDL_GetTicks() / 1000.0f < markStatusUntil){
		text.DrawTextCentered(renderer, markStatus,
			r.GetMasterWidth() / 2, statusY - 28,
			markStatusOk ? Colours::LIME_GREEN : Colours::ORANGE);
	}

	// Slave display — text always on top of media.
	// All content layers render into the offscreen targeted-display texture,
	// which is then composited into the physical slave (with the area outside
	// the targeted rect painted solid black).
	if(r.GetSlave()){
		// (2.0.0) The offscreen target is always at **canvas** (slave-logical)
		// resolution, never the TargetedDisplay rect.
		//
		// It used to be sized to the rect, which meant shrinking the rect
		// shrank the surface every content layer rendered into. The physical
		// slave still looked sharp — that small texture is composited into a
		// correspondingly small area, roughly 1:1 — but the capture mirror
		// stretches the same texture back to full screen, so it showed a
		// genuine upscale of pixels that were never rendered. The capture is
		// meant to mirror the *pre*-targeted-display composite at slave
		// resolution, independent of the rect.
		//
		// CompositeTargetedDisplay scales this full-size target down into the
		// rect, so the physical slave output is unchanged.
		int tw = r.GetCanvasWidth();
		int th = r.GetCanvasHeight();
		if(r.EnsureSlaveTarget(tw, th)){
			r.BeginSlaveContent();
			// Slave glyphs render at full canvas size and are scaled down with
			// everything else at composite time. The old pre-shrink (scaling
			// text by rect/canvas so it survived the capture's upscale) was
			// compensating for the resolution loss above; with the target at
			// full resolution there is nothing to compensate for, and glyphs
			// are supersampled down rather than rasterised small.
			slaveUI.Update(r, media, deltaTime);
			slaveUI.DrawText(r, text);
			r.EndSlaveContent();
			// (1.5) Readback the pre-targeted composite for the capture
			// mirror. No-op if capture isn't open. Must happen before
			// CompositeTargetedDisplay so the recording is clean of the
			// letterbox/rotation transform.
			r.UpdateCaptureFromSlaveTarget();
			r.CompositeTargetedDisplay(targetedDisplay);
		} else{
			// Fallback: draw straight to slave if target creation failed.
			slaveUI.Update(r, media, deltaTime);
			slaveUI.DrawText(r, text);
		}
		r.PresentSlave();
	}
}

void PerformanceScreen::NavigateRight(){
	if(activeColumn == 0 && primedAkt >= 0){
		activeColumn = 1;
		engine.SelectAkt(primedAkt);
		if(primedScene < 0) primedScene = savedScene;
	} else if(activeColumn == 1 && primedScene >= 0){
		activeColumn = 2;
		engine.SelectScene(primedScene);
		if(primedCmd < 0) primedCmd = savedCmd;
		// Never land on a comment row when entering the command column.
		int snapped = engine.NextExecutableIndex(primedAkt, primedScene, primedCmd);
		if(snapped < 0) snapped = engine.LastExecutableIndex(primedAkt, primedScene);
		if(snapped >= 0) primedCmd = snapped;

		// Apply options cascade
		ApplySceneOptionsCascade();

		// Preload media for current + adjacent scenes
		PreloadForCurrentScene();
	}
}

void PerformanceScreen::ApplySceneOptionsCascade(){
	auto& tr = app->GetTextRenderer();
	auto& cfg = Config::Instance();
	tr.SetSlaveFontSize(engine.GetEffectiveFontSize());
	tr.outlineThickness = engine.GetTextOutline();
	// (1.6.1) Re-resolve every face path; bare filenames in scene/project
	// options resolve against the shared FONTS directory so authors can
	// reference fonts by short name (`FontBold = "Inter-Bold.ttf"`).
	// Empty means the Config-level face, so a scene without a font of its
	// own never inherits the previous scene's (2.0.2) — see
	// TextRenderer::SetSlaveFontPaths.
	tr.SetSlaveFontPaths(
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(false, false)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(false, true)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(true,  false)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(true,  true)));
	slaveUI.SetBackgroundColour(engine.GetEffectiveBackgroundColour());
	slaveUI.SetCapitalize(engine.GetEffectiveCapitalize());
	// (2.0.1) Scene -> project -> white default for uncoloured text.
	slaveUI.SetDefaultTextColour(engine.GetEffectiveTextColour());
	// (2.0.2) Scene FixedTextCentre if set, else the project's.
	slaveUI.SetFixedTextCentre(engine.GetEffectiveFixedTextCentre());
	// (2.0.0) Re-evaluated alongside the other cascaded options, so a scene
	// that opts into audio fading takes effect as soon as it is entered.
	media.SetAudioFade(engine.GetEffectiveAudioFade());
}

void PerformanceScreen::PreloadForCurrentScene(){
	auto* slaveR = app->GetRenderer().GetSlave();
	if(!slaveR) return;

	int aktIdx = primedAkt;
	int sceneIdx = primedScene;

	// Current scene preload paths
	auto* curr = engine.GetScene(aktIdx, sceneIdx);
	std::vector<std::string> currPaths = curr ? curr->preloadPaths : std::vector<std::string>{};

	// Previous scene
	std::vector<std::string> prevPaths;
	if(sceneIdx > 0){
		auto* prev = engine.GetScene(aktIdx, sceneIdx - 1);
		if(prev) prevPaths = prev->preloadPaths;
	}

	// Next scene
	std::vector<std::string> nextPaths;
	int sceneCount = engine.GetSceneCountForAkt(aktIdx);
	if(sceneIdx + 1 < sceneCount){
		auto* next = engine.GetScene(aktIdx, sceneIdx + 1);
		if(next) nextPaths = next->preloadPaths;
	} else{
		// Check first scene of next akt
		auto* nextAkt = engine.GetAkt(aktIdx + 1);
		if(nextAkt && !nextAkt->scenes.empty()){
			nextPaths = nextAkt->scenes[0].preloadPaths;
		}
	}

	// Clear any image the slave is currently showing before the cache evicts old textures.
	// SlaveWindow holds a raw SDL_Texture* from the cache; if Evict() destroys it first
	// the pointer dangles and the next draw crashes. Clearing here is safe — if the current
	// scene needs an image, the play command will set it again on execution.
	slaveUI.ClearImage();

	cache.PreloadForSceneTransition(prevPaths, currPaths, nextPaths, projectBasePath, slaveR);

	// Build priority-ordered list of video paths for MediaPlayer preload.
	// Order: current scene (in cmd order) → next scene → previous scene.
	auto IsVideoExt = [](const std::string& arg){
		auto dot = arg.find_last_of('.');
		if(dot == std::string::npos) return false;
		std::string ext = arg.substr(dot);
		return ext == ".mp4" || ext == ".avi" || ext == ".mkv" ||
			ext == ".webm" || ext == ".mov";
	};
	auto CollectVideoPaths = [&](const Scene* scene, std::vector<std::string>& out){
		if(!scene) return;
		for(const auto& cmd : scene->commands){
			if(cmd.type == CommandType::Play && IsVideoExt(cmd.argument)){
				out.push_back(ResolvePath(projectBasePath, cmd.argument));
			}
		}
	};
	std::vector<std::string> videoPriority;
	CollectVideoPaths(curr, videoPriority);
	{
		const Scene* next = nullptr;
		if(sceneIdx + 1 < sceneCount){
			next = engine.GetScene(aktIdx, sceneIdx + 1);
		} else{
			auto* nextAkt = engine.GetAkt(aktIdx + 1);
			if(nextAkt && !nextAkt->scenes.empty()) next = &nextAkt->scenes[0];
		}
		CollectVideoPaths(next, videoPriority);
	}
	if(sceneIdx > 0){
		CollectVideoPaths(engine.GetScene(aktIdx, sceneIdx - 1), videoPriority);
	}
	// Video decode output is sized to the targeted rect so decode cost
	// scales with what's actually visible, not the physical projector.
	int tw = targetedDisplay.width  > 0 ? targetedDisplay.width
		: app->GetRenderer().GetCanvasWidth();
	int th = targetedDisplay.height > 0 ? targetedDisplay.height
		: app->GetRenderer().GetCanvasHeight();
	media.PreloadVideosForScenes(videoPriority, slaveR, tw, th);
}

void PerformanceScreen::NavigateLeft(){
	if(activeColumn == 2){
		savedCmd = primedCmd;
		activeColumn = 1;
	} else if(activeColumn == 1){
		savedScene = primedScene;
		activeColumn = 0;
	}
}

void PerformanceScreen::NavigateUp(int steps){
	awaitingSceneAdvance = false;
	switch(activeColumn){
		case 0: primedAkt   = std::max(0, primedAkt - steps); break;
		case 1: primedScene = std::max(0, primedScene - steps); break;
		case 2:{
			int target = std::max(0, primedCmd - steps);
			// Snap backward to the nearest executable; if none above, snap forward.
			int snapped = engine.PrevExecutableIndex(primedAkt, primedScene, target);
			if(snapped < 0) snapped = engine.NextExecutableIndex(primedAkt, primedScene, target);
			if(snapped >= 0) primedCmd = snapped;
			break;
		}
	}
}

void PerformanceScreen::NavigateDown(int steps){
	switch(activeColumn){
		case 0:
			primedAkt = std::min(engine.GetAktCount() - 1, primedAkt + steps);
			break;
		case 1:{
			int max = engine.GetSceneCountForAkt(primedAkt) - 1;
			primedScene = std::min(max, primedScene + steps);
			break;
		}
		case 2:{
			int max = engine.GetCommandCountForScene(primedAkt, primedScene) - 1;
			int target = std::min(max, primedCmd + steps);
			// Snap forward to the nearest executable; if none below, snap backward.
			int snapped = engine.NextExecutableIndex(primedAkt, primedScene, target);
			if(snapped < 0) snapped = engine.PrevExecutableIndex(primedAkt, primedScene, target);
			if(snapped >= 0) primedCmd = snapped;
			break;
		}
	}
}

void PerformanceScreen::JumpToTop(){
	switch(activeColumn){
		case 0: primedAkt = 0; break;
		case 1: primedScene = 0; break;
		case 2:{
			int first = engine.FirstExecutableIndex(primedAkt, primedScene);
			primedCmd = first >= 0 ? first : 0;
			break;
		}
	}
}

void PerformanceScreen::JumpToBottom(){
	switch(activeColumn){
		case 0: primedAkt = engine.GetAktCount() - 1; break;
		case 1: primedScene = engine.GetSceneCountForAkt(primedAkt) - 1; break;
		case 2:{
			int last = engine.LastExecutableIndex(primedAkt, primedScene);
			int fallback = engine.GetCommandCountForScene(primedAkt, primedScene) - 1;
			primedCmd = last >= 0 ? last : fallback;
			break;
		}
	}
}

void PerformanceScreen::ExecuteCommand(){
	if(activeColumn != 2 || primedCmd < 0) return;

	// State 1: Just arrived at new scene — first ENTER primes cmd 0
	if(awaitingFirstCommand){
		awaitingFirstCommand = false;
		return;
	}

	// State 2: Last command was executed, waiting to advance scene
	if(awaitingSceneAdvance){
		awaitingSceneAdvance = false;
		AdvanceToNextScene();
		return;
	}

	// (1.8.2) Snap any still-revealing `typewrite` line to fully typed
	// before the next cue fires. An operator who advances early gets the
	// complete line rather than a half-written word frozen on the slave.
	// Done here rather than inside the dispatch so it applies whatever the
	// next command turns out to be — including a `clear`.
	slaveUI.CompleteTypewriters();

	// Normal: execute current command
	engine.SelectAkt(primedAkt);
	engine.SelectScene(primedScene);
	// (2.0.2) A cue in a scene the show was not in yet — the first scene of
	// the night, or one the operator jumped to with the arrow keys — moves
	// the show there. Later cues in the same scene send nothing.
	NotifyBackstage(primedAkt, primedScene);
	engine.SetCommandIndex(primedCmd);
	engine.ExecuteCurrentCommand();

	// (2.0.1) Remember what just ran, for the `O` mark-line log. Copied,
	// so a later U reload cannot change what a mark points at.
	if(const Scene* sc = engine.GetCurrentScene()){
		if(const Command* c = engine.GetCurrentCommand()){
			lastExecuted = MarkLog::MarkFor(*sc, *c);
			haveLastExecuted = true;
		}
	}

	// Advance to the next executable command (skip comments). If nothing
	// executable remains in this scene, flag for scene advance.
	int nextExec = engine.NextExecutableIndex(primedAkt, primedScene, primedCmd + 1);
	if(nextExec >= 0){
		primedCmd = nextExec;
	} else{
		awaitingSceneAdvance = true;
	}
}

void PerformanceScreen::NotifyBackstage(int aktIdx, int sceneIdx){
	if(!backstage.Enabled()) return;
	if(aktIdx == backstageAkt && sceneIdx == backstageScene) return;
	backstageAkt = aktIdx;
	backstageScene = sceneIdx;

	auto lineup = Backstage::BuildLineup(engine.GetAkts(), aktIdx, sceneIdx);
	const Scene* scene = engine.GetScene(aktIdx, sceneIdx);
	const std::string sceneName = scene ? scene->name : std::string("?");
	if(lineup.empty()){
		// Skipped scenes are left out of the lineup, so the backstage
		// screen keeps showing the last real one.
		Logger::Instance().Infof("Backstage: scene '%s' has no [Backstage] block "
			"— nothing sent", sceneName.c_str());
		return;
	}
	backstage.Post(Backstage::FormatBody(lineup), lineup.front()->name);
}

void PerformanceScreen::MarkLastExecutedLine(){
	markStatusUntil = SDL_GetTicks() / 1000.0f + 3.0f;
	if(!haveLastExecuted){
		markStatus = "Nothing to mark yet: no cue has run";
		markStatusOk = false;
		return;
	}

	const std::string where = lastExecuted.fileName + " line "
		+ std::to_string(lastExecuted.lineNumber);
	std::string path;
	if(MarkLog::Append(projectBasePath, lastExecuted, &path)){
		markStatus = "Marked " + where + " in fix-it";
		markStatusOk = true;
		Logger::Instance().Infof("Marked %s in %s", where.c_str(), path.c_str());
	} else{
		markStatus = "Could not write the fix-it log";
		markStatusOk = false;
		Logger::Instance().Warnf("Could not write the mark-line log in %s",
			MarkLog::FolderFor(projectBasePath).c_str());
	}
}

void PerformanceScreen::StepBack(){
	if(activeColumn != 2) return;

	int prevExec = engine.PrevExecutableIndex(primedAkt, primedScene, primedCmd - 1);
	if(prevExec >= 0){
		primedCmd = prevExec;
	} else{
		// No executable command above — go back to previous scene's last executable.
		if(primedScene > 0){
			primedScene--;
			engine.SelectAkt(primedAkt);
			engine.SelectScene(primedScene);
			int last = engine.LastExecutableIndex(primedAkt, primedScene);
			if(last < 0){
				int cmdCount = engine.GetCommandCountForScene(primedAkt, primedScene);
				last = cmdCount > 0 ? cmdCount - 1 : 0;
			}
			primedCmd = last;

			// Apply options cascade for the scene we're returning to
			ApplySceneOptionsCascade();
			PreloadForCurrentScene();
		}
		// Could also go back to previous akt's last scene, but leave that for now
	}
}

void PerformanceScreen::AdvanceToNextScene(){
	int sceneCount = engine.GetSceneCountForAkt(primedAkt);

	if(primedScene + 1 < sceneCount){
		// Next scene in same akt
		primedScene++;
	} else{
		// First scene in next akt
		if(primedAkt + 1 < engine.GetAktCount()){
			primedAkt++;
			primedScene = 0;
		} else{
			return; // Already at the very end
		}
	}

	// Prime the first executable command; skip leading comments.
	int first = engine.FirstExecutableIndex(primedAkt, primedScene);
	primedCmd = first >= 0 ? first : 0;
	awaitingFirstCommand = true;

	// Update engine state
	engine.SelectAkt(primedAkt);
	engine.SelectScene(primedScene);

	// Apply options cascade for new scene
	ApplySceneOptionsCascade();

	// (2.0.2) The show has moved into the next scene: update backstage.
	NotifyBackstage(primedAkt, primedScene);

	// Clear slave for new scene
	slaveUI.ClearText();
	slaveUI.ClearImage();
	slaveUI.StopParticles();
	// (1.7) Countdowns are scene-scoped — a scene change drops them along
	// with the rest of the slave content so a timer can't bleed into the
	// next scene.
	slaveUI.ClearCountdowns();
	media.StopAll();

	// Preload for new context
	PreloadForCurrentScene();
}

void PerformanceScreen::ReloadScenes(){
	// Pull every scene .ngk back from disk. The engine clamps its own
	// current-command index; we mirror that by clamping the navigator's
	// primed indices so the user's view doesn't point at empty rows.
	engine.ReloadScenes();

	if(primedAkt >= 0){
		int sceneCount = engine.GetSceneCountForAkt(primedAkt);
		if(primedScene >= sceneCount) primedScene = sceneCount - 1;

		if(primedScene >= 0){
			int cmdCount = engine.GetCommandCountForScene(primedAkt, primedScene);
			if(primedCmd >= cmdCount) primedCmd = cmdCount - 1;
			if(primedCmd >= 0){
				int snap = engine.NextExecutableIndex(primedAkt, primedScene, primedCmd);
				if(snap < 0) snap = engine.PrevExecutableIndex(primedAkt, primedScene, primedCmd);
				if(snap >= 0) primedCmd = snap;
			}
		}
	}

	// Surface the reload in the debug popup so the user can confirm it fired.
	lastCommandStr = "(scenes reloaded)";
	lastCommandPath.clear();
}

void PerformanceScreen::OnSlaveCommand(const Command& cmd){
	// Build debug string
	auto CommandStr = [](const Command& c) -> std::string{
		switch(c.type){
			case CommandType::Text:        return "text \"" + c.argument + "\"";
			case CommandType::TextCont:    return "textCont \"" + c.argument + "\"";
			case CommandType::TextBf:      return "textbf \"" + c.argument + "\"";
			case CommandType::TextIt:      return "textit \"" + c.argument + "\"";
			case CommandType::TextBfCont:  return "textbfCont \"" + c.argument + "\"";
			case CommandType::TextItCont:  return "textitCont \"" + c.argument + "\"";
			case CommandType::TextD:{
				std::string out = "textD \"" + c.argument + "\"";
				if(!c.subtitleRuns.empty()){
					std::string sub;
					for(auto& r : c.subtitleRuns) sub += r.text;
					out += ", \"" + sub + "\"";
				}
				return out;
			}
			case CommandType::Clear:       return c.argument.empty() ? "clear" : "clear " + c.argument;
			case CommandType::ClearText:   return "clearText";
			case CommandType::ClearImages: return "clearImages";
			case CommandType::ClearAll:    return "clearAll";
			case CommandType::Play:{
				// (1.6.6) Mirror the navigator's fade-spec formatting for the
				// debug-popup "last command" line.
				std::string out = "play " + c.argument;
				if(c.fadeInMs != 0 || c.fadeOutMs != 0){
					out += " " + std::to_string(c.fadeInMs) +
						", " + std::to_string(c.fadeOutMs);
				}
				return out;
			}
			case CommandType::Stop:        return c.argument.empty() ? "stop" : "stop " + c.argument;
			case CommandType::StopParticleCont: return c.argument.empty() ? "stopParticleCont" : "stopParticleCont " + c.argument;
			case CommandType::Show:        return "show " + c.argument;
			case CommandType::Loop:        return "loop(...)";
			case CommandType::Compound:    return "compound";
			case CommandType::Comment:     return "# " + c.argument;
			case CommandType::Countdown:{
				std::string out = "countdown(" + std::to_string(c.countdownSeconds);
				if(!c.countdownLabel.empty()) out += ", \"" + c.countdownLabel + "\"";
				out += ")";
				return out;
			}
			case CommandType::ClearCountdown: return "clearCountdown";
			case CommandType::Typewrite:
				return "typewrite(" + std::to_string(c.typeMsPerChar)
					+ ", \"" + c.argument + "\")";
		}
		return "???";
	};
	lastCommandStr = CommandStr(cmd);
	lastCommandPath.clear();

	// (2.0.2) Fixed text centre: a centred line pushed onto an empty stack
	// starts a block, so plan every centred line up to the next clear.
	// `cmd` is the scene's own Command (CueEngine passes references), which
	// is how the planner finds where in the scene the block starts.
	if(TextPlan::AddsCentredLine(cmd) && engine.GetEffectiveFixedTextCentre()
		&& !slaveUI.HasCentredText()){
		if(const Scene* sc = engine.GetCurrentScene()){
			slaveUI.PlanCentredBlock(TextPlan::PlannedRuns(TextPlan::PlanBlock(*sc, &cmd)));
		}
	}

	switch(cmd.type){
		case CommandType::Text:
		case CommandType::TextBf:
		case CommandType::TextIt:
			// (1.6.1) Style-keyword variants share the Text dispatch; the
			// parser already stamped bold/italic flags onto the produced
			// runs (see ParseSingleCommand), so the slave-side path needs
			// no further work.
			if(!cmd.runs.empty()){
				slaveUI.PushTextRuns(cmd.runs, cmd.mods, cmd.groupName);
			} else{
				slaveUI.PushText(cmd.argument, cmd.mods, cmd.groupName);
			}
			break;
		case CommandType::Typewrite:
			// (1.8.2) Same push as `text`, plus the reveal rate. A rate of
			// 0 makes PushTypewriteRuns behave identically to PushTextRuns.
			if(!cmd.runs.empty()){
				slaveUI.PushTypewriteRuns(cmd.runs, cmd.mods,
					cmd.typeMsPerChar, cmd.groupName);
			}
			break;
		case CommandType::TextD:{
			// (1.6.1) Primary line — same as `text`. Pushed first so it
			// participates in the centred-stack default layout normally.
			if(!cmd.runs.empty()){
				slaveUI.PushTextRuns(cmd.runs, cmd.mods, cmd.groupName);
			} else if(!cmd.argument.empty()){
				slaveUI.PushText(cmd.argument, cmd.mods, cmd.groupName);
			}

			// Secondary translation line — only emitted if the parser
			// actually produced runs for it. Project options drive the
			// italic / colour / transparency / posY defaults; per-run
			// inline modifiers (`color(...)`, `bold(...)`, etc.) inside
			// the secondary string still win for that segment.
			if(!cmd.subtitleRuns.empty()){
				RenderModifiers subMods;
				subMods.posY = engine.GetSubtitlePosY();
				subMods.posX = 50.0f; // satisfy HasPosition() — overridden by subtitleAnchor
				subMods.transparency = engine.GetSubtitleTransparency();
				Colour subCol = engine.GetSubtitleColour();
				bool defaultItalic = engine.GetSubtitleItalic();

				std::vector<TextRun> subRuns = cmd.subtitleRuns;
				for(auto& r : subRuns){
					// Apply project italic default unless the run already
					// has italic from inline `it(...)`.
					if(defaultItalic) r.italic = true;
					// Per-run colour wins; otherwise pick up the project
					// default. Alpha 0 is "not set" by convention.
					if(r.colour.a == 0) r.colour = subCol;
				}
				slaveUI.PushSubtitleRuns(subRuns, subMods, cmd.groupName);
			}
			break;
		}
		case CommandType::TextCont:
		case CommandType::TextBfCont:
		case CommandType::TextItCont:
			if(!cmd.runs.empty()){
				slaveUI.AppendToLastText(cmd.runs, cmd.mods, cmd.groupName);
			} else if(!cmd.argument.empty()){
				TextRun r;
				r.text = cmd.argument;
				if(cmd.mods.textColour.a > 0) r.colour = cmd.mods.textColour;
				r.transparency = cmd.mods.transparency;
				// Inherit style from the keyword; per-run inline wrappers
				// already stamp themselves onto cmd.runs in the parser path.
				r.bold   = (cmd.type == CommandType::TextBfCont);
				r.italic = (cmd.type == CommandType::TextItCont);
				slaveUI.AppendToLastText({r}, cmd.mods, cmd.groupName);
			}
			break;
		case CommandType::Clear:
			if(cmd.argument.empty()){
				// Traditional `clear` — drop only ungrouped content across
				// text, images, and particles. Grouped content survives.
				slaveUI.ClearText();
				slaveUI.ClearImage();
				slaveUI.StopParticles();
				// (1.7) Countdowns ride along with the no-arg `clear`. The
				// per-layer `clearText` deliberately doesn't touch them so a
				// timer can stay on screen across text refreshes.
				slaveUI.ClearCountdowns();
			} else{
				// `clear NAME` — drop one whole group across all three layers.
				slaveUI.ClearGroup(cmd.argument);
			}
			break;
		case CommandType::ClearText:
			slaveUI.ClearTextAll();
			break;
		case CommandType::ClearImages:
			// Particles are produced by `show` + `particle()`, so they live in
			// the same logical layer as images — clearImages drops both.
			slaveUI.ClearImageAll();
			slaveUI.StopParticlesAll();
			break;
		case CommandType::ClearAll:
			slaveUI.ClearAll();
			break;
		case CommandType::Play:{
			auto* renderer = app->GetRenderer().GetSlave();
			std::string arg = cmd.argument;
			std::string ext;
			auto dotPos = arg.find_last_of('.');
			if(dotPos != std::string::npos) ext = arg.substr(dotPos);
			std::string fullPath = ResolvePath(projectBasePath, arg);
			lastCommandPath = fullPath;

			if(ext == ".mp4" || ext == ".avi" || ext == ".mkv" ||
				ext == ".webm" || ext == ".mov"){
				int tw = targetedDisplay.width  > 0 ? targetedDisplay.width
					: app->GetRenderer().GetCanvasWidth();
				int th = targetedDisplay.height > 0 ? targetedDisplay.height
					: app->GetRenderer().GetCanvasHeight();
				// (1.6.6) Forward the parsed fade-in / fade-out spec from
				// `play file.mp4 IN, OUT`. 0/0 is the default and matches
				// the pre-1.6.6 behaviour exactly.
				media.PlayVideo(fullPath, renderer, tw, th,
					cmd.fadeInMs, cmd.fadeOutMs);
			} else{
				media.PlayAudio(fullPath);
			}
			break;
		}
		case CommandType::Stop:
			if(cmd.argument.empty()){
				media.StopAll();
				slaveUI.StopParticles();
			} else{
				// (2.0.1) Resolve the name the same way `play` does: sounds are
				// identified by full path since 2.0.0, so the bare name from
				// the scene never matched and `stop FILE` did nothing.
				media.StopAudio(ResolvePath(projectBasePath, cmd.argument));
			}
			break;
		case CommandType::StopParticleCont:
			// (1.4) Empty argument → ungrouped systems; otherwise → that group.
			slaveUI.StopParticlesSmoothly(cmd.argument);
			break;
		case CommandType::Show:{
			auto* renderer = app->GetRenderer().GetSlave();
			if(!renderer) break;

			// Build the full candidate list for randomImages() — either one
			// concrete entry (no randomImages), the explicit list, or the
			// directory expansion. Every string here is absolute-or-bare; the
			// resolver below turns bare names into absolute paths.
			std::vector<std::string> candidates;
			if(!cmd.mods.randomImagePaths.empty()){
				auto& paths = cmd.mods.randomImagePaths;
				if(paths.size() == 1 && paths[0].rfind("DIR:", 0) == 0){
					std::string dirName = paths[0].substr(4);
					std::string fullDir = projectBasePath + "/pictures/" + dirName;
					if(std::filesystem::exists(fullDir)){
						for(auto& e : std::filesystem::directory_iterator(fullDir)){
							if(e.is_regular_file()) candidates.push_back(e.path().string());
						}
					}
				} else{
					candidates = paths;
				}
			} else if(!cmd.argument.empty()){
				candidates.push_back(cmd.argument);
			}

			if(candidates.empty()) break;

			auto resolveOne = [&](const std::string& p) -> std::string{
				if(p.find('/') != std::string::npos || p.find('\\') != std::string::npos)
					return p;
				return ResolvePath(projectBasePath, p);
			};

			if(cmd.mods.particleType != ParticleType::None){
				// Spawn bounds follow the targeted rect so particles stay
				// within the visible area.
				int tw = targetedDisplay.width  > 0 ? targetedDisplay.width
					: app->GetRenderer().GetCanvasWidth();
				int th = targetedDisplay.height > 0 ? targetedDisplay.height
					: app->GetRenderer().GetCanvasHeight();

				// (1.4) Resolve the effective tuning cascade for this type
				// and stamp it onto a local copy of the command modifiers
				// so ParticleSystem can read it without re-cascading.
				ParticleTuning tune = engine.GetEffectiveParticleTuning(cmd.mods.particleType);
				Command resolved = cmd;
				resolved.mods.speedMul   = tune.speed;
				resolved.mods.densityMul = tune.density;
				resolved.mods.xDist      = tune.xDist;
				resolved.mods.vxDist     = tune.vxDist;
				resolved.mods.vyDist     = tune.vyDist;

				// Load every candidate texture so the particle system can
				// pick a fresh one per spawn.
				std::vector<ParticleTex> pool;
				pool.reserve(candidates.size());
				for(const auto& p : candidates){
					std::string full = resolveOne(p);
					auto* cached = cache.Get(full);
					if(!cached) cached = cache.LoadImage(full, renderer);
					if(cached && cached->texture){
						pool.push_back({cached->texture, cached->width, cached->height});
					}
				}
				if(pool.empty()) break;

				// Pick one for lastCommandPath display (resolve first candidate).
				lastCommandPath = resolveOne(candidates[0]);

				if(pool.size() == 1){
					slaveUI.StartParticles(resolved.mods.particleType,
						pool[0].texture, pool[0].w, pool[0].h,
						tw, th, resolved.mods, resolved.groupName);
				} else{
					slaveUI.StartParticlesPool(resolved.mods.particleType,
						pool, tw, th, resolved.mods, resolved.groupName);
				}
			} else{
				// Non-particle show: pick exactly one image.
				std::string pick = candidates[rand() % candidates.size()];
				std::string full = resolveOne(pick);
				lastCommandPath = full;

				auto* cached = cache.Get(full);
				if(!cached) cached = cache.LoadImage(full, renderer);
				if(cached){
					// Animated GIFs go through the ShowAnimation path so the
					// slave can cycle frames by wall-time; everything else
					// stays on the single-texture ShowImage path.
					if(cached->isAnimation && !cached->frames.empty()){
						slaveUI.ShowAnimation(cached->frames, cached->frameDelaysMs,
							cached->width, cached->height, cmd.mods, cmd.groupName);
					} else if(cached->texture){
						slaveUI.ShowImage(cached->texture,
							cached->width, cached->height, cmd.mods, cmd.groupName);
					}
				}
			}
			break;
		}
		case CommandType::Loop:
		case CommandType::Compound:
		case CommandType::Comment:
			break;
		case CommandType::Countdown:
			slaveUI.StartCountdown(cmd.countdownSeconds, cmd.countdownLabel);
			break;
		case CommandType::ClearCountdown:
			slaveUI.ClearCountdowns();
			break;
	}
}

void PerformanceScreen::UpdateDebugInfo(){
	auto& dbg = app->GetDebugPopup().info;
	auto& config = Config::Instance();

	auto* akt = engine.GetCurrentAkt();
	dbg.currentAktName = akt ? ("Akt " + std::to_string(akt->number)) : "None";
	auto* scene = engine.GetCurrentScene();
	dbg.currentSceneName = scene ? scene->name : "None";

	dbg.lastCommand = lastCommandStr;
	dbg.lastCommandPath = lastCommandPath;
	dbg.lastCommandFileExists = !lastCommandPath.empty() && std::filesystem::exists(lastCommandPath);

	dbg.slaveWidth = app->GetRenderer().GetSlaveWidth();
	dbg.slaveHeight = app->GetRenderer().GetSlaveHeight();
	dbg.slaveDisplayIndex = config.slaveDisplayIndex;

	dbg.globalFontSize = config.fontSize;
	dbg.projectFontSize = engine.GetProjectFontSize();
	dbg.sceneFontSize = engine.GetSceneFontSize();

	dbg.cacheSize = cache.GetTotalSize();
	dbg.preloadedFiles = cache.GetFileCount();
	dbg.videoBufferedFrames = media.GetBufferedFrames();
	dbg.videoRingSize = media.GetVideoRingCapacity();
	// (2.0.0) IsUsingHwDecode() reports false both for software decode and
	// for "nothing loaded", so HasVideo() separates the two — otherwise an
	// idle player would read as a hardware-decode failure.
	dbg.videoDecodeMode = !media.HasVideo()
		? DecodeMode::None
		: (media.IsVideoZeroCopy() ? DecodeMode::HardwareZeroCopy
		   : (media.IsUsingHwDecode() ? DecodeMode::Hardware : DecodeMode::Software));
	dbg.activeSounds = media.GetActiveSoundCount();

	// (2.0.2) Backstage screen.
	using Look = DebugInfo::BackstageLook;
	const auto bs = backstage.GetStatus();
	switch(bs.state){
		case BackstageSender::State::Off:
			dbg.backstageLook = Look::Off;
			dbg.backstageText = "off (no Url in schema.toml)";
			break;
		case BackstageSender::State::Waiting:
			dbg.backstageLook = Look::Neutral;
			dbg.backstageText = "on, nothing sent yet";
			break;
		case BackstageSender::State::Sending:
			dbg.backstageLook = Look::Neutral;
			dbg.backstageText = "sending " + bs.label + "...";
			break;
		case BackstageSender::State::Sent:
		case BackstageSender::State::Failed:{
			const int64_t ago = (BackstageSender::NowMs() - bs.finishedMs) / 1000;
			const bool sent = bs.state == BackstageSender::State::Sent;
			dbg.backstageLook = sent ? Look::Ok : Look::Failed;
			dbg.backstageText = (sent ? "sent " : "FAILED ") + bs.label
				+ " " + std::to_string((long long)ago) + "s ago: " + bs.detail;
			break;
		}
	}
}

} // namespace SatyrAV
