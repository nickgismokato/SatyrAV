#include "screens/PerformanceScreen.hpp"
#include "App.hpp"
#include "core/Config.hpp"
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

	activeColumn = 0;
	primedAkt = -1;
	primedScene = -1;
	primedCmd = -1;
}

void PerformanceScreen::OnExit(){
	media.Shutdown();
	cache.Clear();
	slaveUI.StopParticles();
	app->GetRenderer().CloseSlave();
}

void PerformanceScreen::SetProject(const ProjectData& project){
	projectBasePath = project.projectPath;
	targetedDisplay = project.schema.targetedDisplay;

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

	// Slave display — text always on top of media.
	// All content layers render into the offscreen targeted-display texture,
	// which is then composited into the physical slave (with the area outside
	// the targeted rect painted solid black).
	if(r.GetSlave()){
		int tw = targetedDisplay.width  > 0 ? targetedDisplay.width  : r.GetCanvasWidth();
		int th = targetedDisplay.height > 0 ? targetedDisplay.height : r.GetCanvasHeight();
		if(r.EnsureSlaveTarget(tw, th)){
			r.BeginSlaveContent();
			// (1.5) Canvas-relative text: scale slave glyph rendering by
			// the ratio of the current TargetedDisplay height to the full
			// slave height. At default (full-size rect) scale == 1.0 and
			// behaviour matches pre-1.5; at smaller rects text shrinks
			// proportionally, which keeps its apparent size constant on
			// the capture mirror after SDL_RenderSetLogicalSize stretches
			// the canvas back up.
			text.SetSlaveScale(r.GetSlaveContentScale());
			slaveUI.Update(r, media, deltaTime);
			slaveUI.DrawText(r, text);
			text.SetSlaveScale(1.0f); // Reset for master-UI text paths.
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
	// Empty regular keeps the existing loaded face — see
	// TextRenderer::SetSlaveFontPaths comment.
	tr.SetSlaveFontPaths(
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(false, false)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(false, true)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(true,  false)),
		cfg.ResolveFontPath(engine.GetEffectiveFontPath(true,  true)));
	slaveUI.SetBackgroundColour(engine.GetEffectiveBackgroundColour());
	slaveUI.SetCapitalize(engine.GetEffectiveCapitalize());
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

	// Normal: execute current command
	engine.SelectAkt(primedAkt);
	engine.SelectScene(primedScene);
	engine.SetCommandIndex(primedCmd);
	engine.ExecuteCurrentCommand();

	// Advance to the next executable command (skip comments). If nothing
	// executable remains in this scene, flag for scene advance.
	int nextExec = engine.NextExecutableIndex(primedAkt, primedScene, primedCmd + 1);
	if(nextExec >= 0){
		primedCmd = nextExec;
	} else{
		awaitingSceneAdvance = true;
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

	// Clear slave for new scene
	slaveUI.ClearText();
	slaveUI.ClearImage();
	slaveUI.StopParticles();
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
			case CommandType::Play:        return "play " + c.argument;
			case CommandType::Stop:        return c.argument.empty() ? "stop" : "stop " + c.argument;
			case CommandType::StopParticleCont: return c.argument.empty() ? "stopParticleCont" : "stopParticleCont " + c.argument;
			case CommandType::Show:        return "show " + c.argument;
			case CommandType::Loop:        return "loop(...)";
			case CommandType::Compound:    return "compound";
			case CommandType::Comment:     return "# " + c.argument;
		}
		return "???";
	};
	lastCommandStr = CommandStr(cmd);
	lastCommandPath.clear();

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
				media.PlayVideo(fullPath, renderer, tw, th);
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
				media.StopAudio(cmd.argument);
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
}

} // namespace SatyrAV
