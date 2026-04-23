# Architecture

SatyrAV is a single-process, dual-display application built with C++17, SDL2, and FFmpeg. It follows a screen-based architecture where a central `App` class owns the main loop and routes between screen objects.

## Module overview

### App (`src/App.hpp/cpp`)

The application entry point. Owns the `Renderer`, `TextRenderer`, `InputHandler`, and all `Screen` instances. The main loop in `App::Run()` polls SDL events, dispatches input actions, calls the active screen's `Update` and `Draw`, renders popups on top, and presents the master window. The slave window is presented by `PerformanceScreen` directly since it only exists during project playback.

### Screens (`src/screens/`)

Each screen implements the `Screen` interface: `OnEnter`, `OnExit`, `HandleInput`, `Update`, `Draw`. The active screen is set by `App::SwitchScreen(ScreenID)`. Available screens:

- **SplashScreen** — two-phase startup animation (SaTyR logo, then SatyrAV logo with credits). Auto-advances after 3+3 seconds or on any key press.
- **MainMenuScreen** — three options: New project, Load project, Options. Shows the SaTyR logo.
- **NewProjectScreen** — form for revy type, year, creator name. Creates the project directory structure with `schema.toml` and an example scene.
- **LoadProjectScreen** — lists `.toml` files found in the configured projects directory. Select and press ENTER to load.
- **OptionsScreen** — font colour, font size, projects directory, secondary display index. Saves to platform config path.
- **PerformanceScreen** — the main workspace. Three-column navigator (akts → scenes → commands), cue execution, slave display management.

### Core (`src/core/`)

- **CueEngine** — holds the loaded `ProjectData`, manages selection state (current akt, scene, command index), and fires a callback when a command is executed. Provides the options cascade (`GetEffectiveFontSize`, `GetEffectiveBackgroundColour`, `GetEffectiveCapitalize`, `GetTextOutline`, `GetEffectiveParticleTuning`). Particle tunings cascade scene → project → built-in defaults per field.
- **SchemaParser** — reads `schema.toml` using toml++ and produces a `RevySchema` struct with project-level options and the akt/scene structure.
- **SceneParser** — reads `.ngk` scene files. Handles section parsing (`[Options]`, `[Preload]`, `[Macro]`, `[Commands]`), block constructs (`run{}`, `loop(){}`, `group NAME{}`), compound commands (`&`), macro definition + inline expansion (`macro <Name>`), the `+` concatenation operator inside `text` / `textCont`, and the full modifier chain (`color`, `trans`, `rotate`, `move`, `spin`, `pos`, `size`, `scale`, `random`, `particle`, `randomImages`, `trail`, `trailGlow`, `bounce`). Per-particle-type tunables (`<TYPE>.SPEED`, `<TYPE>.DENS`, `<TYPE>.X.DIST`) are parsed out of `[Options]`. Group blocks tag every body command with the group name so the slave can later clear that group selectively.
- **Project** — directory creation, `schema.toml` generation, example scene generation.
- **Config** — singleton holding global settings (font size, paths, display index). Loads/saves from the platform config path.
- **Platform** — platform-specific default paths. Linux: `~/.config/satyrav/config.toml` and `~/satyrav/projects/`. Windows: `%APPDATA%\SatyrAV\config.toml` and `Documents\SatyrAV\projects\`.
- **Types** — all shared data types: `Colour`, `Command`, `CommandType`, `RenderModifiers`, `ParticleType`, `Scene`, `Akt`, `RevySchema`, `ProjectData`.

### Render (`src/render/`)

- **Renderer** — manages SDL windows and renderers for master and slave displays. `InitMaster` creates the control window. `InitSlave` creates the fullscreen projector window on the configured display.
- **TextRenderer** — TTF text rendering with separate font sizes for master and slave. Handles text outline rendering via `TTF_SetFontOutline`.
- **MasterWindow** — draws the header, revy title, and three-column navigator with line numbers, scrolling, and truncation.
- **SlaveWindow** — manages the projector output: background colour, text entries (centered or positioned), a list of active images with render modifiers, and a list of active particle systems. Each text/image/particle entry carries an optional group tag; ungrouped `show` / `particle()` replace previous ungrouped content (old single-image behaviour), while grouped entries always append so a group can stack multiple hearts, rains, etc. Layers draw in order: background → video → images → particles → text.
- **MediaPlayer** — threaded video/audio playback using FFmpeg. Owns a map of per-path `VideoPlayback` decode contexts so videos referenced by the current prev/curr/next scene window can be **preloaded in parallel with image preload**, moving all decode work off the playback critical path. On Windows, uses **D3D11VA hardware decode** as the primary path — the GPU's video decode engine handles H.264/HEVC directly, sharing SDL's D3D11 device so decoder and renderer operate on the same GPU. Falls back to multi-threaded software decode on Linux or unsupported codecs. Each `VideoPlayback` owns its own FFmpeg state, decode thread, NV12 ring, per-video audio buffer, and SDL streaming texture. The active video drives a single shared `SDL_AudioDevice`. A 4 GB budget (configurable) is split in command-priority order across clips in the current scene window. See [video-pipeline.md](video-pipeline.md) for full details.
- **MediaCache** — pre-loads image textures and tracks all preloaded file sizes. Supports 3-scene preloading (previous, current, next) with eviction of out-of-scope assets. Animated `.gif` files are decoded frame-by-frame via `IMG_LoadAnimation` and exposed to the slave as a texture array plus per-frame delays.
- **ParticleSystem** — CPU particle emitter. 10 motion models: `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, and *(1.4)* `BROWNIAN`, `OSCILLATION`, `ORBIT`, `NOISE`, `LIFECURVE`. Supports trails *(Ghost / Stretch / Glow, 1.4.1)*, edge-bounce reflection, per-type speed/density/X-distribution tunables sampled per spawn, and a sticky smooth-stop mode that lets live particles finish after `stopParticleCont`.

### Input (`src/input/`)

- **InputHandler** — translates raw SDL keyboard events into `InputAction` enum values. Handles modifier keys (SHIFT for previous command and jump navigation, CTRL for top/bottom jumps).

### UI (`src/ui/`)

- **Widget** — base class with position, size, focus state.
- **ListView** — scrollable text list with selection highlight.
- **TextField** — single-line text input with cursor.
- **Dropdown** — expandable option selector.
- **Popup** — base class for draggable, opaque popup windows with title bar.
- **HelpPopup** — shows all keyboard controls, contact email, version. Context-aware (shows different controls when in a project vs. in menus).
- **DebugPopup** — two-column display of runtime info: scene context, last command with path/exists check, slave display resolution, font cascade values, cache stats, video buffer status.

## Data flow

1. User presses ENTER on a command in `PerformanceScreen`
2. `ExecuteCommand()` calls `CueEngine::ExecuteCurrentCommand()`
3. CueEngine looks up the `Command` struct and calls the slave callback
4. `OnSlaveCommand()` dispatches by `CommandType` — text goes to `SlaveWindow`, media goes to `MediaPlayer`, images go through `MediaCache`
5. On the next frame, `SlaveWindow::Update()` clears the slave, draws video/images/particles, then `DrawText()` renders text on top
6. `App::Run()` presents the master window (with popups overlaid) and the slave window

## Threading model

The main thread handles all SDL rendering, input, and UI. A single background thread runs the FFmpeg decode loop for video playback. Audio is driven by SDL's audio callback on its own internal thread. Communication between threads uses SDL mutexes and condition variables.
