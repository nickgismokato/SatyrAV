# Architecture

SatyrAV is a single-process, dual-display application built with C++17, SDL2, and FFmpeg. It follows a screen-based architecture where a central `App` class owns the main loop and routes between screen objects.

## Module overview

### App (`src/App.hpp/cpp`)

The application entry point. Owns the `Renderer`, `TextRenderer`, `InputHandler`, and all `Screen` instances. The main loop in `App::Run()` polls SDL events, dispatches input actions, calls the active screen's `Update` and `Draw`, renders popups on top, and presents the master window. The slave window is presented by `PerformanceScreen` directly since it only exists during project playback.

### Screens (`src/screens/`)

Each screen implements the `Screen` interface: `OnEnter`, `OnExit`, `HandleInput`, `Update`, `Draw`. The active screen is set by `App::SwitchScreen(ScreenID)`. Available screens:

- **SplashScreen** — two-phase startup animation (SaTyR logo, then SatyrAV logo with credits). Auto-advances after 3+3 seconds or on any key press.
- **MainMenuScreen** — three options: New project, Load project, Options. Shows the SaTyR logo.
- **NewProjectScreen** — form for revy type, year, creator name. Creates the project directory structure with `schema.toml` and an example scene. *(1.6.2)* Creator is persisted into `[RevyData].Creator` so the Load Project screen can show authorship.
- **LoadProjectScreen** *(rewritten 1.6.2)* — scans `projectsDir` for any sub-folder containing `schema.toml` and parses its metadata once per `OnEnter`. Renders a grouped two-column table (full Revy name / persisted creator) with a live search field, a Revy filter dropdown (auto-populated from the live scan; collapses Jubi + non-Jubi into a shared group), and a sort dropdown (Year / Recent / Name). `TAB` cycles focus across the four input regions; the project list itself skips group-header rows on Up/Down so the cursor always lands on a project.
- **OptionsScreen** — font colour, font size, projects directory, secondary display index, slave window mode, capture mirror display, and *(1.6.3)* audio output device. Saves to platform config path.
- **PerformanceScreen** — the main workspace. Three-column navigator (akts → scenes → commands), cue execution, slave display management. *(1.6.3)* Dispatches `InputAction::FunctionKey` for project audio hotkeys and tracks the currently-playing hotkey so a second press toggles it off.

### Core (`src/core/`)

- **CueEngine** — holds the loaded `ProjectData`, manages selection state (current akt, scene, command index), and fires a callback when a command is executed. Provides the options cascade (`GetEffectiveFontSize`, `GetEffectiveBackgroundColour`, `GetEffectiveCapitalize`, `GetTextOutline`, `GetEffectiveParticleTuning`). *(1.6.1)* Adds `GetEffectiveFontPath(bold, italic)` for the four font-style face paths and `GetSubtitle{Italic,Colour,Transparency,PosY}` for `textD` defaults. *(1.6.3)* `GetFunctionKeyAudio(num)` returns the bound F-key audio filename. Particle tunings cascade scene → project → built-in defaults per field; font slots cascade scene → project → Config (and Config falls back to bundled DejaVu).
- **SchemaParser** — reads `schema.toml` using toml++ and produces a `RevySchema` struct with project-level options and the akt/scene structure. *(1.6.1)* Reads `Options.FontItalic`/`FontBold`/`FontBoldItalic` plus the four `Subtitle*` keys. *(1.6.2)* Reads `RevyData.Creator` and extracts a trailing 4-digit year for sort. *(1.6.3)* Reads the `[Hotkeys]` table.
- **SceneParser** — reads `.ngk` scene files. Handles section parsing (`[Options]`, `[Preload]`, `[Macro]`, `[Commands]`), block constructs (`run{}`, `loop(){}`, `group NAME{}`), compound commands (`&`), macro definition + inline expansion (`macro <Name>`), the `+` concatenation operator inside every text command, and the full modifier chain. *(1.6.1)* Parses the `textbf` / `textit` / `textbfCont` / `textitCont` / `textD` keywords and the inline `bold(...)` / `it(...)` segment wrappers. *(1.6.3)* When a leading bareword doesn't match any known keyword the parser still falls through to `Text` for runtime safety, but stamps `Command::unknownKeyword` and lifts those into a per-scene `Scene::warnings` list (recursing through compound + loop bodies) so Overview Debug can surface typos. Group blocks tag every body command with the group name so the slave can later clear that group selectively.
- **Project** — directory creation, `schema.toml` generation, example scene generation. *(1.6.2)* Persists `Creator`. *(1.6.3)* Emits a commented `[Hotkeys]` example block in new schemas so users discover the feature.
- **Config** — singleton holding global settings (font size, paths, display index, audio device, font-variant paths, fonts dir). Loads/saves from the platform config path. *(1.6.1)* `ResolveFontPath(name)` accepts either an absolute path or a bare filename (resolved against `fontsDir`). *(1.6.3)* `audioDeviceName` selects the SDL output device.
- **Logger** *(1.6.3)* — process-wide singleton that mirrors every message to both stderr (always) and a per-day rotating file at `<SatyrAV>/log/satyrav-YYYY-MM-DD.log`. Mutex-guarded so the decode-thread sites in `MediaPlayer` can log safely. printf-style `Debugf`/`Infof`/`Warnf`/`Errorf` API; per-process file-level filter (default `Info`).
- **Platform** — platform-specific default paths. Linux: `~/.config/satyrav/config.toml`, `~/satyrav/projects/`, `~/satyrav/FONTS/`, `~/satyrav/log/`. Windows: `Documents\SatyrAV\config.toml`, `Documents\SatyrAV\projects\`, `Documents\SatyrAV\FONTS\`, `Documents\SatyrAV\log\`.
- **Types** — all shared data types: `Colour`, `Command`, `CommandType`, `RenderModifiers`, `ParticleType`, `Scene`, `Akt`, `RevySchema`, `ProjectData`. *(1.6.1)* `TextRun` gains `bold` / `italic`; `RevySchema` gains font-variant paths and subtitle defaults; `Scene` gains font-variant override paths. *(1.6.2)* `RevySchema` gains `creatorName` and parsed `year`. *(1.6.3)* `Command` gains `unknownKeyword`; `Scene` gains `warnings`; `RevySchema` gains `functionKeyAudio` map.

### Render (`src/render/`)

- **Renderer** — manages SDL windows and renderers for master and slave displays. `InitMaster` creates the control window. `InitSlave` creates the fullscreen projector window on the configured display. *(1.6.0)* `GetCanvasWidth/Height` returns the rect-authoring-space dims (intendedSlave in windowed-dev mode, slave bounds in fullscreen) — used by everything that interprets `TargetedDisplay` coordinates so windowed and fullscreen behave identically.
- **TextRenderer** — TTF text rendering with separate font sizes for master and slave. Handles text outline rendering via `TTF_SetFontOutline`. *(1.6.1)* Holds a 4-slot face table per side (regular / italic / bold / bold-italic) keyed by `(bold ? 2 : 0) | (italic ? 1 : 0)`. `Init` takes the three optional style paths; `SetSlaveFontPaths(...)` swaps the slave-side path set on scene change with empty-means-keep-existing semantics. `PickSlaveFont(bold, italic)` walks bold-italic → bold → italic → regular for graceful degradation. `DrawTextSlave` / `DrawTextSlaveRotated` / `DrawTextCenteredSlave` / `MeasureSlave` accept optional `(bool bold, bool italic)` so the multi-run line layout in `SlaveWindow` picks the matching face per segment.
- **MasterWindow** — draws the header, revy title, and three-column navigator with line numbers, scrolling, and truncation. *(1.6.0)* Long commands wrap to continuation rows via real pixel-width measurement (`MeasureWidth`) with binary-search fitting and word-aware breaks; the primed-row highlight spans every wrapped row.
- **SlaveWindow** — manages the projector output: background colour, text entries (centered or positioned), a list of active images with render modifiers, and a list of active particle systems. Each text/image/particle entry carries an optional group tag; ungrouped `show` / `particle()` replace previous ungrouped content (old single-image behaviour), while grouped entries always append so a group can stack multiple hearts, rains, etc. Layers draw in order: background → video → images → particles → text. *(1.6.1)* `SlaveTextEntry::subtitleAnchor` flag and `PushSubtitleRuns(...)` add a bottom-anchored centred-X layout for `textD` subtitles; a pre-pass in `DrawText` assigns per-subtitle `subtitleBottomY` so multiple subtitles stack upward instead of overlapping.
- **MediaPlayer** — threaded video/audio playback using FFmpeg. Owns a map of per-path `VideoPlayback` decode contexts so videos referenced by the current prev/curr/next scene window can be **preloaded in parallel with image preload**, moving all decode work off the playback critical path. On Windows, uses **D3D11VA hardware decode** as the primary path — the GPU's video decode engine handles H.264/HEVC directly, sharing SDL's D3D11 device so decoder and renderer operate on the same GPU. Falls back to multi-threaded software decode on Linux or unsupported codecs. Each `VideoPlayback` owns its own FFmpeg state, decode thread, NV12 ring, per-video audio buffer, and SDL streaming texture. *(1.6.0)* New `HasVideo()` returns true while a video is loaded (including paused) so `K`-pause holds the last decoded frame on screen instead of going black. *(1.6.3)* `OpenAudioDeviceFor` honours `Config::audioDeviceName` (with single-retry fallback to system default if the named device is unplugged at boot). The active video drives a single shared `SDL_AudioDevice`. A 4 GB budget (configurable) is split in command-priority order across clips in the current scene window. See [video-pipeline.md](video-pipeline.md) for full details.
- **MediaCache** — pre-loads image textures and tracks all preloaded file sizes. Supports 3-scene preloading (previous, current, next) with eviction of out-of-scope assets. Animated `.gif` files are decoded frame-by-frame via `IMG_LoadAnimation` and exposed to the slave as a texture array plus per-frame delays.
- **ParticleSystem** — CPU particle emitter. 10 motion models: `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, and *(1.4)* `BROWNIAN`, `OSCILLATION`, `ORBIT`, `NOISE`, `LIFECURVE`. Supports trails *(Ghost / Stretch / Glow, 1.4.1)*, edge-bounce reflection, per-type speed/density/X-distribution tunables sampled per spawn, and a sticky smooth-stop mode that lets live particles finish after `stopParticleCont`.

### Input (`src/input/`)

- **InputHandler** — translates raw SDL keyboard events into `InputAction` enum values. Handles modifier keys (SHIFT for step-back and jump navigation, CTRL for top/bottom jumps). *(1.6.3)* Emits `InputAction::FunctionKey` for `SDLK_F1..F12` with the F-number on a `lastFunctionKey` side channel; suppressed in text-input mode so typing in a search field can't fire a hotkey.

### UI (`src/ui/`)

- **Widget** — base class with position, size, focus state.
- **ListView** — scrollable text list with selection highlight.
- **TextField** — single-line text input with cursor.
- **Dropdown** — expandable option selector.
- **Popup** — base class for draggable, opaque popup windows with title bar.
- **HelpPopup** — shows all keyboard controls, contact email, version. Context-aware (shows different controls when in a project vs. in menus).
- **DebugPopup** — two-column display of runtime info: scene context, last command with path/exists check, slave display resolution, font cascade values, cache stats, video buffer status. *(1.6.0+)* Footer hint advertises the `S` (Display rect) and `O` (Overview Debug) sub-popup chords.
- **DisplaySizePopup** — modal `D → S` editor for the targeted-display rect (move / scale / rotate with `WASD`/`Z`/`X`/`Q`/`E`, `SHIFT` for fine precision, `C` to reset).
- **OverviewPopup** *(1.6.3)* — modal `D → O` scene-health report. Walks every scene of the active project on `Open()` and renders parser warnings (unknown keywords) and missing media files referenced by `show` / `play`. Cached at open time so projects with many scenes don't re-scan per frame; UP/DOWN scrolls, ESC closes.

## Data flow

1. User presses ENTER on a command in `PerformanceScreen`
2. `ExecuteCommand()` calls `CueEngine::ExecuteCurrentCommand()`
3. CueEngine looks up the `Command` struct and calls the slave callback
4. `OnSlaveCommand()` dispatches by `CommandType` — text goes to `SlaveWindow`, media goes to `MediaPlayer`, images go through `MediaCache`
5. On the next frame, `SlaveWindow::Update()` clears the slave, draws video/images/particles, then `DrawText()` renders text on top
6. `App::Run()` presents the master window (with popups overlaid) and the slave window

## Threading model

The main thread handles all SDL rendering, input, and UI. A single background thread runs the FFmpeg decode loop for video playback. Audio is driven by SDL's audio callback on its own internal thread. Communication between threads uses SDL mutexes and condition variables.
