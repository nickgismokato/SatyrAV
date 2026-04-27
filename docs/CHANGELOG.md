# Changelog

All notable changes to **SatyrAV** are documented here. Versions are listed newest first.

## Version 1.6.4 — shipped

Bug-fix release. Four critical UX bugs cleared up: command-list scroll could clip the last command when an earlier visible row word-wrapped, slave text never wrapped at all, dropdown menus came back already-open after ESC + return, and re-entering a project showed the previous session's slave content rendered against the wrong (global) font size.

### Bugfixes
- **Command-list scroll now wrap-aware.** [MasterWindow::DrawNavigator](src/render/MasterWindow.cpp) computed its bottom-scroll clamp as `cmdCount - maxLines` — i.e. one row per command — so any visible command that wrapped onto a continuation row pushed the last command off the bottom of the panel. The clamp now walks backward from the last command, accumulating wrapped row counts, and uses the smallest index whose "from-here-to-end" total still fits as the maximum legal `cmdScroll`. Worked through the example in the bug report (20 commands, command 3 wraps to 2 rows): pre-fix, command 20 was unreachable; post-fix, the scroll backs up one row further when needed and command 20 is fully visible.
- **Word-wrap on the slave display.** Long lines on the slave used to render past the right edge of the targeted display because `SlaveWindow::DrawText` had no wrapping path at all. Added [`WrapEntryLines`/`TokenizeForWrap`/`MeasureLine`](src/render/SlaveWindow.cpp) — greedy fit on word boundaries against a budget of `targetWidth - 2 * margin` (margin ≈ 2.5%), preserving per-run bold/italic/colour/transparency across the wrap. Mid-word breaks are explicitly avoided: a single word longer than the budget keeps its style and overflows visually rather than being chopped. The draw loop was reshaped around per-entry visual lines so default-stack, `pos()`, animated `move()`, and `textD` subtitle anchoring all see the wrapped layout. Subtitle stacking (1.6.1) was extended to multi-line entries — the most-recently-pushed subtitle's *last* line still sits at `SubtitlePosY`, every earlier line (within or before that entry) moves one line height higher.
- **Dropdown menus no longer freeze open after ESC + return.** [OptionsScreen::OnEnter](src/screens/OptionsScreen.cpp), [LoadProjectScreen::OnEnter](src/screens/LoadProjectScreen.cpp), and [NewProjectScreen::OnEnter](src/screens/NewProjectScreen.cpp) now `Close()` every dropdown they own. Previously the dropdown's `expanded=true` flag survived the screen exit (because `Quit` switched screens immediately, without closing the active dropdown), so the next visit drew the panel already-open until the user moved focus onto it. Affects all five dropdowns on the Options screen, both dropdowns on Load Project, and the Revy-type dropdown on New Project.
- **Slave content no longer carries over between project sessions.** [PerformanceScreen::OnEnter / OnExit](src/screens/PerformanceScreen.cpp) now call `slaveUI.ClearAll()` + `slaveUI.StopParticlesAll()` + `media.StopAll()`. The bug surfaced as a font-size regression: text rendered at the project font size persisted on the slave through the project exit, then was redrawn against the global font size set by `OnEnter`. Clearing on both sides means the slave is always blank at the moment a project re-opens — covers text, images, GIF animations, particles, audio, and video — and the next scene's `ApplySceneOptionsCascade` is the only thing that ever puts content back.

---

## Version 1.6.3 — shipped

Final sub-version of the 1.6 cycle. Audio routing, project audio hotkeys, parser-warning surfacing via a new Overview Debug popup, a process-wide log file, and a baseline VSCode extension for `.ngk` syntax highlighting.

### Added
- **Audio output device picker.** New `Config::audioDeviceName` (string; empty = system default) persisted as `[audio] device` in `config.toml`. [MediaPlayer::OpenAudioDeviceFor](src/render/MediaPlayer.cpp) honours it via `SDL_OpenAudioDevice(name, ...)`, with a single-retry fallback to `nullptr` if the saved device is unplugged at boot. [OptionsScreen](src/screens/OptionsScreen.cpp) gets a new `Audio Output` dropdown enumerating `SDL_GetNumAudioDevices(0)` / `SDL_GetAudioDeviceName(i, 0)`; option 0 is `Default`. Lets production setups route SatyrAV sound to a specific HDMI / USB interface independent of the system default.
- **F1–F12 audio hotkeys** for one-tap playback during a performance. New `[Hotkeys]` table in `schema.toml` (`F1 = "knock.mp3"`, …) parsed into `RevySchema::functionKeyAudio`. `InputAction::FunctionKey` collapses all twelve keys into one action with a numeric side channel on `InputHandler` (`GetLastFunctionKey()`); suppressed in text-input mode so typing in a search field doesn't fire a hotkey. [CueEngine::GetFunctionKeyAudio](src/core/CueEngine.cpp) returns the bound filename; [PerformanceScreen](src/screens/PerformanceScreen.cpp) resolves through the same path lookup as `play` (so `"knock.mp3"` picks up `<project>/sound/knock.mp3`). New projects get a commented `[Hotkeys]` example block via [Project::GenerateDefaultSchema](src/core/Project.cpp). Help popup advertises the new row.
- **Hotkey toggle behaviour.** Pressing the *same* F-key while its audio is still playing stops the sound instead of restarting it — useful for long ambient cues (knocking, alarm) where the performer wants one key to drive both start and stop. Different F-key while one is playing replaces the running sound transparently. Tracked via a single `currentHotkeyNum` field on the screen.
- **Global logger.** New [core/Logger.{hpp,cpp}](src/core/Logger.cpp) singleton: per-day rotating file at `<SatyrAV>/log/satyrav-YYYY-MM-DD.log`, mirrored to stderr so dev-time visibility is unchanged. Mutex-guarded so the decode-thread sites in `MediaPlayer` can log safely. printf-style `Debugf`/`Infof`/`Warnf`/`Errorf` API; per-process file-level filter (default `Info`). New `Platform::GetDefaultLogsDir()` resolves `~/Documents/SatyrAV/log` on Windows / `~/satyrav/log` elsewhere. Initialised right after `Config::Load` and torn down in `App::Shutdown` with explicit `starting`/`shutting down cleanly` markers.
- **Overview Debug popup** (`D → O` chord while a project is open, mirroring `D → S` for the rect editor). New [ui/OverviewPopup.{hpp,cpp}](src/ui/OverviewPopup.cpp) walks every scene of the active project on open and reports per-scene parser warnings, the count of executable commands, and any `show` / `play` references that don't resolve to a file on disk. Modal — ESC closes, UP/DOWN scrolls, everything else is swallowed. Cached at open time so projects with many scenes don't re-scan per frame.
- **Parser warnings surfaced in Scene state.** [SceneParser.cpp::ParseSingleCommand](src/core/SceneParser.cpp) now flags bareword keywords that don't match any known command (typo of `txet "..."` for `text`, etc.) by setting `Command::unknownKeyword`; the per-scene parse loop lifts those into `Scene::warnings` recursively (covers compound + loop bodies). Behaviour fallback is unchanged — a flagged command still parses as `Text` so the scene runs — but the warning is now visible to the author via Overview Debug.
- **VSCode `.ngk` syntax extension.** New `VSCodeExt/` folder at repo root with `package.json`, `language-configuration.json`, `syntaxes/ngk.tmLanguage.json`, `README.md`, `.vscodeignore`. Grammar covers section headers, every command keyword (incl. all 1.6.1 text variants and `textD`), modifier wrappers (incl. `bold` / `it`), particle types, distribution constants, group / loop block keywords, strings, numbers, line comments. README documents three install paths — copy-folder (recommended, no toolchain), F5 dev-load, and `vsce package` (with workarounds for Node 18 hitting `vsce` 3.x's Node-20 requirement).

### Changed
- Debug popup footer hint now reads `Press S: Display rect    O: Overview Debug` to advertise the new chord alongside the existing one.
- New `[Hotkeys]` example block emitted by `GenerateDefaultSchema` so users discover the feature when poking around a fresh project's `schema.toml`.

### Bugfixes
- `MediaPlayer::OpenAudioDeviceFor` now retries with `nullptr` (system default) when the configured device name is no longer present — a USB interface unplugged between sessions would previously fail the open and silence all audio until the user fixed `config.toml` by hand.

---

Third sub-version of the 1.6 cycle. Load Project page redesigned around metadata: live search, Revy-type filter, sort, and grouped two-column rendering with creator authorship. Schema gains a `Creator` field so authorship is persisted. One pre-existing UX wart fixed along the way (typing `H`/`D` in any text field no longer trips the popup toggles).

### Added
- New `Load Project` screen layout in [LoadProjectScreen.{hpp,cpp}](src/screens/LoadProjectScreen.cpp). Full rewrite around a `ProjectInfo` struct populated from a single-pass `schema.toml` scan (no `.ngk` parsing, so the load is fast even on large project archives). New interleaved `LoadRow` model — group headers and project rows in one vector; navigation skips headers automatically. `MoveSelection`, `JumpSelectionToTop`/`Bottom`, plus a custom multi-column table render replace the old flat `ListView`.
- Live search field — case-insensitive substring match against the project's full Revy name, applied on every keystroke.
- `Revy` filter dropdown — auto-built from the live scan as `"All"` plus every distinct group alphabetically. Selecting `Kemi` (or any other revy) collapses the table to that revy only. Up/Down arrows flip the option live without expanding; ENTER toggles the panel. Selection survives a re-scan when the group still exists.
- `Sort` dropdown — `Year` (default, descending), `Recent` (folder mtime descending), `Name` (A→Z). Sort applies *within* each group so the section layout stays stable across modes.
- Grouped table rendering: each group gets a header row (`MatRevy`, `KemiRevy`, …) and projects underneath as two columns — full Revy name on the left, creator on the right (right-aligned). Empty creator (legacy projects authored before 1.6.2) renders as a dim em-dash so the column stays visually consistent.
- TAB cycles focus between Search → Filter → Sort → List. ESC defocuses to the list, then to MainMenu when already there. ENTER on the search field jumps to the list. Visible focus via existing widget border-colour flip.
- New `creatorName` and `year` (parsed) fields on [RevySchema](src/core/Types.hpp). [SchemaParser.cpp](src/core/SchemaParser.cpp) reads `[RevyData].Creator` (default empty) and extracts a trailing 4-digit year from `[RevyData].Revy` for sorting purposes. [Project.cpp::GenerateDefaultSchema](src/core/Project.cpp) writes `Creator = "<name>"` so new projects persist authorship; [Project.cpp::Load](src/core/Project.cpp) mirrors `creator` and `year` up to `ProjectData` so existing top-level callers see the persisted values.

### Changed
- Group key derivation strips both a trailing 4-digit year (e.g. `MatRevy 2025` → `MatRevy`) and a trailing `Jubi` suffix (e.g. `MatRevyJubi` → `MatRevy`), so jubilee editions and regular years share a single group.

### Bugfixes
- Pressing `H` or `D` while a text field has focus no longer trips the global Help/Debug popup toggles. The handler in [App.cpp](src/App.cpp) now suppresses those shortcuts while `InputHandler::IsTextInputMode()` is true. Affected the new Load Project search field most visibly, but the same trap existed on the New Project creator field since 1.0.

---

## Version 1.6.1 — shipped

Second sub-version of the 1.6 cycle. Real italic / bold typography on the slave display, plus a `textD` translation/subtitle command for bilingual revues. Built around four loaded font faces (regular / italic / bold / bold-italic) with a scene → project → global cascade and a shared `FONTS` directory so projects can reference fonts by short filename.

### Added
- Shared `FONTS` directory parallel to the projects folder. New `Platform::GetDefaultFontsDir()` resolves to `~/Documents/SatyrAV/FONTS` on Windows and `~/satyrav/FONTS` elsewhere; created automatically by [Config::EnsureDefaults](src/core/Config.cpp). Persisted as `paths.fonts` in `config.toml`. New `Config::ResolveFontPath(name)` accepts either an absolute path (passes through unchanged, drive-letter-aware on Windows) or a bare filename (resolved against `fontsDir`). Used by [App::Init](src/App.cpp) for the global font slots and by [PerformanceScreen::ApplySceneOptionsCascade](src/screens/PerformanceScreen.cpp) for project / scene overrides.
- Bundled DejaVu Sans family in `assets/fonts/` — `DejaVuSans.ttf`, `DejaVuSans-Oblique.ttf`, `DejaVuSans-Bold.ttf`, `DejaVuSans-BoldOblique.ttf`. Plus DejaVu Sans Mono (4 weights) and `DejaVuMathTeXGyre.ttf` for future code / math features. The CMake `install(DIRECTORY assets/ ...)` rule and the NSIS installer's `File /r "..\assets\*.*"` already pick these up — no changes needed to either. [App::Init](src/App.cpp) falls each unset font slot through to the bundled file via `FindAsset("fonts/...")`, so `textbf` / `textit` work out of the box on a fresh install. Last-resort fallback to OS DejaVu (Linux) or Arial (Windows) is preserved if `assets/fonts/` is somehow missing.
- Four loaded font faces in [TextRenderer.cpp](src/render/TextRenderer.cpp) — regular, italic, bold, bold-italic — keyed by `(bold ? 2 : 0) | (italic ? 1 : 0)`. New `Init` signature taking three optional style paths; new `SetSlaveFontPaths(regular, italic, bold, boldItalic)` to swap the slave-side path set on scene change. New private `PickSlaveFont(bold, italic)` walks bold-italic → bold → italic → regular for graceful degradation when only some variants are configured. `DrawTextSlave` / `DrawTextSlaveRotated` / `DrawTextCenteredSlave` / `MeasureSlave` all gained optional `(bool bold, bool italic)` params (default false) so existing call sites keep working.
- Per-segment style flags on [TextRun](src/core/Types.hpp) (`bool bold`, `bool italic`). [SlaveWindow::DrawText](src/render/SlaveWindow.cpp) passes them to measure / draw so bold's wider glyph metrics propagate correctly through the multi-run line layout — a regular `MeasureSlave` would have shifted later runs.
- `textbf "..."` / `textit "..."` / `textbfCont "..."` / `textitCont "..."` line-level keywords. Parser sets the line default style on every produced run via OR with any per-run flag set by inline wrappers, so `textbf "A " + it("B")` renders A bold and B bold-italic.
- Inline `bold(content)` / `it(content)` segment wrappers in [SceneParser::ParseModifiers](src/core/SceneParser.cpp), parsed alongside the existing `color(...)` / `trans(...)` / `rotate(...)` modifiers. `text "Plain " + bold("X") + " " + it("Y") + " mixed"` produces four runs with per-segment styling.
- `textD "primary", "secondary"` command for bilingual subtitles. Top-level comma split (whitespace-tolerant via the existing quote-aware `SplitArgs`). Both sides accept the full `+` / `bold(...)` / `it(...)` / `color(...)` machinery — secondary text lives in `Command::subtitleRuns`. [PerformanceScreen](src/screens/PerformanceScreen.cpp) pushes the primary as a normal centred line and the secondary as a separate bottom-anchored entry with project-configured italic / colour / transparency / posY defaults applied before any per-run override. Single-arg form (`textD "..."`) collapses to plain `Text` semantics. Multiple `textD` entries stack: the most-recently pushed subtitle sits at the configured anchor, older ones move one line up each — pre-pass in [SlaveWindow::DrawText](src/render/SlaveWindow.cpp) assigns per-entry `subtitleBottomY` to keep the layout stable across paint cycles.
- Project `[Options]` keys in [SchemaParser.cpp](src/core/SchemaParser.cpp): `FontItalic`, `FontBold`, `FontBoldItalic` (paths, bare or absolute), and the four `textD` defaults — `SubtitleItalic` (default `true`), `SubtitleColour` (named or `#RRGGBB`, default `#AAAAAA`), `SubtitleTransparency` (percent 0–100, default 0), `SubtitlePosY` (percent of canvas height, default 90.0). Per-scene `[Options]` keys `fontItalic` / `fontBold` / `fontBoldItalic` for scene-level overrides. New CueEngine accessors `GetEffectiveFontPath(bold, italic)`, `GetSubtitle{Italic,Colour,Transparency,PosY}` expose the cascade.
- New helper [PerformanceScreen::ApplySceneOptionsCascade](src/screens/PerformanceScreen.cpp) centralises the scene-change update for font size, outline, font-style face paths, background colour, and capitalize. Three previous in-line copies of this cascade collapsed into one call; the new font-paths step would otherwise have had to be added in three places.

### Changed
- Wordwrap in [MasterWindow::DrawNavigator](src/render/MasterWindow.cpp) and the slave text path now route through the existing measure helpers so adding bold (which widens glyphs) doesn't push later runs off-screen.

### Bugfixes
- `SetSlaveFontPaths` no longer wipes the Config-level italic / bold / bold-italic slots when a project doesn't override them. Empty path on any slot now means "keep existing" (matching the regular slot's behaviour); without this guard, every project-load left only the regular face loaded, and every styled run silently fell back to regular regardless of what the user had set in `config.toml`.

---

## Version 1.6.0 — shipped

First sub-version of the 1.6 cycle. Foundation pass: slave render correctness and the cheap accumulated bugs. Subtitle/text features (1.6.1), Load Project rework (1.6.2), and audio routing + diagnostics + tooling (1.6.3) follow. PNG transparency was originally planned for 1.6 but moved to 2.1 — SDL2 has no per-pixel-alpha window flag, and a workaround would mean hand-rolling D3D11 + DirectComposition; it falls out for free once the 2.0 SDL3 migration lands.

### Added
- `MediaPlayer::HasVideo()` in [MediaPlayer.cpp](src/render/MediaPlayer.cpp). True while a video is loaded — including when paused. Distinct from `IsVideoPlaying()`, which gates on `!videoPaused`.
- `Renderer::GetCanvasWidth/Height()` in [Renderer.cpp](src/render/Renderer.cpp). Returns the rect-authoring-space dims — `intendedSlaveW/H` in windowed-dev mode, slave bounds in fullscreen — so callers that work in `TargetedDisplay` coords don't pick up the live windowed-slave size by mistake.
- Footer hint in the debug popup: "Press S to edit Display rect" ([DebugPopup.cpp](src/ui/DebugPopup.cpp)). Documents the existing `D`→`S` chord; the planned `O` Overview Debug lands in 1.6.3.
- Troubleshooting section in [docs/building.md](docs/building.md) covering DLL placement, asset path, `config.toml` path quoting (1.5.1), the SDL2 cmake-prefix rewrite step, and blank-slave diagnosis.

### Changed
- `K`-pause now freezes the last decoded frame on the slave instead of going black. [SlaveWindow.cpp::DrawMedia](src/render/SlaveWindow.cpp) gates the video-texture draw on `HasVideo()` rather than `IsVideoPlaying()`. The YUV ring upload in `UpdateVideoFrame` is already paused-aware (skips uploads while paused), so the texture retains its last frame and the slave keeps presenting it. `K` again resumes from that frame.
- Long commands in the master command list wrap onto continuation rows instead of being truncated with `...`. [MasterWindow.cpp::DrawNavigator](src/render/MasterWindow.cpp) tracks a real y cursor, sizes the primed-row highlight to span every wrapped row, and only prints the line number on the first row. Wrap uses `TextRenderer::MeasureWidth` with binary-search fitting and word-aware breaks (the previous `cmdColW / 7` char-budget heuristic underestimated proportional-font widths and never wrapped in practice).
- Renamed `Shift+ENTER` action from `PrevCommand` to `StepBack` across [InputHandler](src/input/InputHandler.hpp), [PerformanceScreen](src/screens/PerformanceScreen.cpp), and the [HelpPopup](src/ui/HelpPopup.cpp). Help text now reads "Step back (cursor only — no undo)". The action only rewinds the prime cursor; in-flight audio/video/text remain live. True undo (snapshot/replay or per-command inverses) was deferred — not currently a production need.

### Bugfixes
- Windowed-slave display rect now behaves identically to fullscreen. Root cause: [DisplaySizePopup](src/ui/DisplaySizePopup.cpp) and the auto-rect-default in [PerformanceScreen::SetProject](src/screens/PerformanceScreen.cpp) used live slave-window dims, but `TargetedDisplay` rects are interpreted in production-slave (`intendedSlaveW/H`) pixel space. Three visible symptoms all resolved by switching the relevant call sites to `GetCanvasWidth/Height`:
  - `C` reset fills the full virtual slave instead of the live window's dims (a sub-rect inside the letterbox).
  - Images fill the visible (non-black-bar) area — previously the rect was undersized so the inner fit-to-rect compounded with the outer letterbox into a double-letterbox.
  - `pos` percentages are window-aspect-independent. In a portrait window, a `C` reset previously inherited the portrait dims and mapped `pos%` against them; now the rect is canvas-sized and `pos%` lands in the same relative spot regardless of the dev window's aspect.

---

## Version 1.5.1 - shipped
### Bugfixes
- Fixed `Naeste`&rarr;`Næste` inside `MasterWindow.cpp`

---

## Version 1.5.0 — shipped

### Added
- `<TYPE>.VX.DIST = NAME(p1, p2)` and `<TYPE>.VY.DIST = NAME(p1, p2)` scene/project tunables in [SceneParser.cpp](src/core/SceneParser.cpp) and [SchemaParser.cpp](src/core/SchemaParser.cpp). Reshape how each motion model samples its spawn velocity within its built-in `[vmin, vmax]` range via a new `SampleV` helper in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). `UNIFORM` (default) reproduces 1.4 behaviour exactly. Applies to `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `BROWNIAN`, `LIFECURVE` (both axes) and `OSCILLATION` (vy only). Formula-driven types (`ORBIT`, `NOISE`) and polar `SCATTER` ignore these.
- Gravity + settling on `bounce(true, …)` in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). Enabling bounce now also enables a constant downward gravity (≈ 900 px/s²) on every integrating particle type. Particles bounce off the floor with the existing 0.8 loss coefficient; once both |vx| and |vy| fall below the rest threshold, the particle locks in place (velocity and rotation zeroed) and pile-up forms on the floor. Settled particles still tick their lifetime, so piles fade out naturally. `ORBIT` and `NOISE` ignore gravity because their position is formula-driven.
- Windowed slave mode for single-monitor development. New `Slave Window Mode` dropdown in [OptionsScreen.cpp](src/screens/OptionsScreen.cpp) (Fullscreen / Windowed (dev)); persists as `display.slave_windowed` in `config.toml`. When on, [Renderer::InitSlave](src/render/Renderer.cpp) opens the slave as a resizable, decorated 1280×720 window on the primary display instead of fullscreen on the configured secondary display. `GetSlaveWidth/Height` and `CompositeTargetedDisplay` live-query the current drawable size so user-driven resizes propagate into the letterbox without needing an `SDL_WINDOWEVENT` pipeline. Saved TargetedDisplay rect coords stay in production-slave pixel space — the windowed path tracks `intendedSlaveW/H` (the configured slave display's bounds) and letterboxes that virtual slave inside the live window, so fullscreen and windowed render the same rect at proportional scale. Targeted-display rect, rendering, and input routing are identical to fullscreen; production setups leave the toggle off.
- Capture-mirror window (third OS window) for feeding a capture card / recorder. New `Capture Display` dropdown in [OptionsScreen.cpp](src/screens/OptionsScreen.cpp) (default **Off**); persists as `display.capture_enabled` + `display.capture_display` in `config.toml`. When a project is opened, [Renderer::InitCapture](src/render/Renderer.cpp) opens a borderless fullscreen-desktop window on the selected display at the slave's logical resolution, with `SDL_RenderSetLogicalSize` handling any upscale to the native panel. Each frame, [Renderer::UpdateCaptureFromSlaveTarget](src/render/Renderer.cpp) reads `slaveTarget` back to a CPU ARGB8888 buffer and uploads it to a streaming capture-side texture, producing a clean mirror of the **pre-targeted-display** composite — no rect crop, no rotation, no letterbox. Hard guards refuse if the picked display equals the slave's display; soft guard refuses while `slave_windowed` is on (setting is preserved so flipping back to fullscreen auto-restores the last choice). The capture window closes automatically when leaving the performance screen.
- Canvas-relative slave text sizing in [TextRenderer.cpp](src/render/TextRenderer.cpp). New `slaveScale` multiplier applied to every glyph draw / measure path on the slave (`DrawTextSlave`, `DrawTextCenteredSlave`, `DrawTextSlaveRotated`, `MeasureSlave`, `GetSlaveLineHeight`). [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) calls `text.SetSlaveScale(r.GetSlaveContentScale())` around slave-UI text draws so text scales in lockstep with `slaveTarget` dims — images, videos, GIFs, and particles already do, because they're canvas-relative. Result: the capture mirror shows constant-size text regardless of how the TargetedDisplay rect is resized via the `D`→`S` popup. Master-side UI is unaffected (scale is reset to 1.0 between slave and master passes).

### Changed
- Multiple concurrent ungrouped particle systems. [SlaveWindow.cpp](src/render/SlaveWindow.cpp) `StartParticles` / `StartParticlesPool` no longer drop existing ungrouped systems on a fresh start — ungrouped systems coexist like grouped ones. `clear`, `stop`, and `stopParticleCont` still act on every ungrouped system as a unit; smooth-stopping systems remain protected from later hard-stops.

### Bugfixes
- `config.toml` paths containing backslashes (`C:\Users\…`) no longer wedge the loader. [Config.cpp](src/core/Config.cpp) `Save()` now writes path values as TOML **literal** strings (`'…'`) instead of basic strings (`"…"`), so `\U`, `\n`, and friends aren't interpreted as escape sequences. Previously `toml::parse_file` would throw on reload and every config value silently reverted to its default.
- `DisplaySizePopup` sub-pixel precision stall. With `SHIFT+W/A/S/D` (0.1× move precision) and `SHIFT+Z/X` (scale precision), per-frame deltas at 120 Hz rounded to zero so the rect never moved or resized. [DisplaySizePopup.cpp](src/ui/DisplaySizePopup.cpp) now keeps float accumulators for move (`moveAccumX/Y`) and float mirrors of width/height for scale (`fWidth/fHeight`), draining whole pixels into the integer `TargetedDisplay` fields each frame. Scale precision bumped from 0.1× to 0.25× so `SHIFT+Z/X` feels responsive at small rect sizes.

---

## Version 1.4.1 — shipped

### Changed
- `trail(N, content)` reworked in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). Past samples are now distance-gated (no stacking on slow particles), aged in wall-time, and faded with exponential decay + age-shrink scale instead of the old index-based ramp. Result: smoother streaks, less smearing.
- `trail(N, MODE, content)` — optional middle argument selects the look:
  - `GHOST` (default) — distance-gated past frames with exponential alpha decay and age-shrink scale.
  - `STRETCH` — single elongated quad per particle, rotated along the motion direction. Reads as real motion blur.
  - `GLOW` — Ghost with additive blending. Best for bright sprites.

### Added
- `trailGlow(N, content)` modifier — sugar for `trail(N, GLOW, content)`. Same geometry as Ghost, but `SDL_BLENDMODE_ADD` on the trail samples for a bloom look. Live particle still draws with normal alpha blend.

---

## Version 1.4.0 — shipped

### Added
- Animated GIF support — [MediaCache.cpp](src/render/MediaCache.cpp) decodes every frame to a texture via `IMG_LoadAnimation`; [SlaveWindow.cpp](src/render/SlaveWindow.cpp) advances the current frame by wall time. All position, rotation, spin, move, size, and transparency modifiers apply to the whole animation. New `ShowAnimation` path on the slave; the dispatcher in [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) routes `.gif` shows through it.
- Five new particle motion models in [ParticleSystem.cpp](src/render/ParticleSystem.cpp):
  - `BROWNIAN` — random-walk velocity drift, no directional gravity, mild damping.
  - `OSCILLATION` — sinusoidal X around a per-particle anchor column with linear Y descent. Each particle has its own amplitude, frequency, and phase.
  - `ORBIT` — circular motion around the screen centre at a random radius and angular velocity, random direction per particle.
  - `NOISE` — velocity steered by a 2D value-noise flow field; nearby particles follow similar currents.
  - `LIFECURVE` — linear motion with a triangular scale + alpha envelope (grow + fade in, shrink + fade out).
- `trail(N, content)` modifier — keeps each particle's last `N` transforms in a ring buffer and renders them behind the live particle with alpha ramped up toward the head.
- `bounce(true|false, content)` modifier — reflects particles off the four screen edges with an 0.8 energy-loss coefficient on the reflected velocity component.
- Per-particle-type `[Options]` tunables in scenes ([SceneParser.cpp](src/core/SceneParser.cpp)) and project-level `[Particle.<TYPE>]` tables in `schema.toml` ([SchemaParser.cpp](src/core/SchemaParser.cpp)). Cascade is scene → project → built-in defaults via `CueEngine::GetEffectiveParticleTuning`.
  - `<TYPE>.SPEED` — velocity multiplier (also scales angular velocity for `ORBIT` and oscillation frequency for `OSCILLATION`).
  - `<TYPE>.DENS` / `<TYPE>.DENSITY` — spawn-rate multiplier.
  - `<TYPE>.X.DIST = NAME(p1, p2)` — distribution for spawn X position. Supported: `UNIFORM`, `NORMAL`, `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`. Box-Muller for normals, Marsaglia-Tsang for gamma.
- `stopParticleCont [GROUP]` command — stops a particle system from spawning while letting every live particle finish its animation. The smooth-stop is sticky: subsequent `stop` / `clear` / `clearAll` commands no longer cut the ending short. Drained systems are GC'd from the slave's particle list automatically.
- `+` concatenation operator inside `text` and `textCont` — multiple styled segments on a single line. Per-segment modifiers (`color`, `trans`) override entry-level defaults; line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the whole line, **leftmost wins** when more than one segment sets the same one. New `TextRun` struct on `Command::runs`; the slave's `DrawText` measures and renders runs left-to-right.
- `textCont "…"` command — appends its argument to the most recently pushed text entry across cues, so a line can be revealed piece by piece. Handles `\n` overflow by pushing fresh entries.

### Changed
- `Particle` carries four scratch floats (`auxA`–`auxD`) for per-model state (oscillation amp/freq/phase/baseX, orbit radius/angularVel/angle, etc.) and a trail ring buffer (`std::vector<TrailSample>` + `trailHead`).
- `RenderModifiers` gained `trailLength`, `bounceEdges`, `speedMul`, `densityMul`, and a `DistSpec xDist`. The dispatcher in [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) stamps the resolved tuning onto a local copy of `mods` before calling `StartParticles` so `ParticleSystem` doesn't have to re-cascade.
- Particle `Draw` applies per-particle `scale` with centred scaling so animated scale (used by `LIFECURVE` and the trail) doesn't pull particles toward their top-left corner.
- `SlaveWindow::StopParticles`, `StopParticlesAll`, `ClearGroup`, and `ClearAll` now skip systems already in smooth-stop mode.

### Bugfixes
- *(none recorded for 1.4)*

---

## Version 1.3 — shipped

### Added
- `scale(content, [w,h])` modifier in [SceneParser.cpp](src/core/SceneParser.cpp) — pixel-dimension alias of `size()` with the argument order matching `random()` and the other content-first modifiers.
- `[Macro]` section in `.ngk` files for reusable command blocks. Expanded at parse time via `macro <Name>` in `[Commands]`. No parameters, no nesting.
- `group <Name>{ … }` block — named, addressable bucket of commands whose outputs can be cleared independently.
- `clear <Name>` — drop one group by name.
- `clearText` — drop only the text layer across all groups.
- `clearImages` — drop only the image layer across all groups.
- `clearAll` — drop text, images, and every group at once.
- `randomImages(folderName)` now picks a fresh random image on **every particle spawn** when combined with `particle()`, instead of picking once and reusing the same file.
- `U` key reloads all scene `.ngk` files in the current project without leaving it.
- New Project screen additions:
  - `SaTyR` as a selectable revy type.
  - Jubi toggle — folder becomes `<Revy>Jubi_<year>` (e.g. `MatRevyJubi_2019`).
  - "Other" revy type reveals a custom-name field — folder becomes `<CustomName>[Jubi]_<year>`.
- [docs/commands.md](docs/commands.md) — full reference of every section, command, block and modifier with syntax, behaviour, and an example per entry.
- [CHANGELOG.md](CHANGELOG.md) — this file.

### Changed
- Default generated example scene in [Project.cpp](src/core/Project.cpp) now uses the absolute-path resolver, so `show satyr-logo.png` and `[Preload] satyr-logo.png` work without the `pictures/` prefix.

### Bugfixes
- *(none yet in 1.3)*

---

## Version 1.2 — shipped

### Added
- Animation modifiers in [SceneParser.cpp](src/core/SceneParser.cpp) and [SlaveWindow.cpp](src/render/SlaveWindow.cpp):
  - `move(FROM, TO, DURATION_MS, content)` — linear tween of position over time. `FROM`/`TO` accept `[x,y]` percent pairs or side letters `N`/`S`/`E`/`W` (spawns off-screen against that edge).
  - `spin(degPerSec, content)` — continuous rotation at N degrees/second. Composes with the existing static `rotate()`.
- `run{ … }` block — multi-line equivalent of the `&` compound operator. One cue, one navigator row, all sub-commands fire at once.
- `comment "text"` command — visible in the master navigator as `# <text>`, never executable, skipped during navigation, excluded from the line-number count.
- Targeted-display rectangle — slave content renders into a configurable sub-rect (width, height, centre, rotation) inside the physical secondary display. Everything outside the rect is solid black.
- Hidden size menu at Debug popup (`D`) → `S`. Adjust with `Z`/`X` (scale), `Q`/`E` (rotate), `W`/`A`/`S`/`D` (move centre). Hold `SHIFT` for 10× precision. `C` resets to default (fill physical display). `ESC` persists to `schema.toml` and closes.
- `[Display]` section in `schema.toml` (Width, Height, CenterX, CenterY, Rotation) — per-project persistence of the targeted-display rect.

### Changed
- [Project.cpp](src/core/Project.cpp) `Save()` now round-trips `schema.toml` via toml++, preserving `[Options]`, `[RevyData]` and `[Structure.*]` while refreshing the `[Display]` section. Note: stand-alone `#` comments in the schema are not preserved on save.
- Slave layer stack now renders into an offscreen `SDL_Texture` sized to the targeted rect. [MediaPlayer.cpp](src/render/MediaPlayer.cpp) sizes the YUV decode target to the targeted rect rather than the physical display, keeping decode cost proportional to visible area.
- [TextRenderer.cpp](src/render/TextRenderer.cpp) gained `DrawTextSlaveRotated` / `MeasureSlave` so spinning text composes cleanly with `SDL_RenderCopyEx`.

### Bugfixes
- *(none recorded for 1.2)*

---

## Version 1.1 — shipped

### Added
- Hardware-accelerated video playback path in [MediaPlayer.cpp](src/render/MediaPlayer.cpp) — FFmpeg decode with YUV → SDL texture upload on the GPU.

### Changed
- Slave video rendering switched from software blit to GPU texture streaming, cutting CPU load for 1080p/4K clips.

### Bugfixes
- Long debugging pass on the hardware-video path: decoder lifecycle, colour-space mapping, resize handling, and teardown ordering across multi-cue scenes all stabilised.

---

## Version 1.0 — shipped

Initial release. All major functionality of the cue program in place.

### Added
- Dual-window architecture — master control window + slave projector window driven by the same cue engine.
- `.ngk` scene files with `[Options]`, `[Preload]` and `[Commands]` sections.
- Executable commands: `text`, `clear`, `show`, `play`, `stop`.
- Compound cues via `&` (e.g. `clear & text "Hi"`).
- `loop(count, clearEvery){ … }` block.
- Modifier chain on text/images: `color`, `trans`, `rotate`, `pos`, `size`, `random`, `particle`, `randomImages`.
- Project model ([Project.cpp](src/core/Project.cpp)) with `schema.toml` describing akter (acts) and their scene order.
- New Project screen with revy-type picker and year selector.
- Master navigator UI showing the active scene, current cue, and surrounding commands with line numbers.
- Debug popup (`D`) reporting window sizes, current scene, and last-executed command.
- Help popup (`H`) listing keyboard shortcuts.
- Project-wide `[Options]` (font, font size, text outline, background colour) overridable per scene.
- Asset resolver that finds files in `pictures/`, `movies/`, `sound/` subdirectories from bare names.
