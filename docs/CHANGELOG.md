# Changelog

All notable changes to **SatyrAV** are documented here. Versions are listed newest first.

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
