# SatyrAV

An AV cue controller for SaTyR revyer. Dual-display output with keyboard-first
navigation through acts, scenes, and commands.

**Current version:** 2.0.1 — see [docs/CHANGELOG.md](docs/CHANGELOG.md) for the full change history.

Full documentation lives in [docs/](docs/README.md).

## Features

### Performance and navigation

- Dual-display output: an operator master screen with a cue navigator, and a
  fullscreen slave on the secondary display for the audience.
- Keyboard-first navigation through acts, scenes, and commands. `ENTER` fires a
  cue and advances; `U` reloads every `.ngk` in the current project without
  leaving the performance screen.
- Mark-line log *(2.0.1)*: press `O` during a show to note the last cue's file,
  line and code in the project's `fix-it/ErrorInFiles-<date>.LOG`, so problems
  spotted live can be fixed afterwards.
- `comment "..."` cues show in the master navigator as non-executable rows and
  are skipped during navigation.
- **Cue Notes column** *(1.8.4)*: a fourth, read-only column right of the
  command list showing stage-management notes from the scene's `[Cue]`
  section. Scrolls in lockstep with the commands, wraps long notes, and never
  reaches the audience display. Appears only for scenes that define notes.

### Scene format

- `.ngk` scene files grouped into acts, scenes, and commands.
- `run{ ... }` blocks fire every sub-command on a single cue row — the
  multi-line equivalent of the `&` compound operator.
- `[Macro]` blocks define named command blocks expanded inline from
  `[Commands]` via `macro <Name>`. Expansion happens at parse time.
- `loop(N){ ... }` for repeated command blocks.
- `group NAME{ ... }` tags every piece of slave content produced by the body so
  it can be dropped later with `clear NAME` without disturbing anything else
  on screen.
- Fine-grained clears: `clear NAME`, `clearText`, `clearImages`, `clearAll`,
  and `clearCountdown`.
- Quoted text accepts three interchangeable delimiter pairs — `"..."`,
  `'...'`, and `´...´` — so a line can itself contain quotation marks.
- `[Cue]` section *(1.8.4)*: named operator notes, claimed by a command with a
  trailing `\Name` and shown in the master window's Cue Notes column.
- New projects are generated with two acts and four tutorial scenes — one per
  engine layer (text, pictures, particles, groups/macros) — plus a
  `schema.toml` that lists every project option at its default *(1.8.0)*.

### Content rendering

- Static images (`.png`, `.jpg`).
- Animated GIFs: `show foo.gif` plays every frame at the file's delays. All
  position, rotation, spin, move, size, and transparency modifiers apply to
  the animation as a whole.
- Video playback with D3D11VA hardware decode on Windows (H.264 / HEVC via the
  GPU decode engine) and a multi-threaded software fallback on Linux or
  unsupported codecs. *(2.0.0)* On Windows the decoded frame stays on the GPU
  all the way to the screen (zero-copy), with the previous read-back route as
  the automatic fallback.
- *(2.0.0)* Overlapping sound: F-key sound effects and `play` audio tracks
  play over each other and over a video's own sound. An opt-in `AudioFade`
  option makes a video's fade times fade its sound as well as its picture.
- Preload-driven playback: videos in the current, previous, and next scene are
  decoded into a ring buffer ahead of time so the first frames go out
  instantly when `play` fires. Configurable RAM budget
  (`preload_budget_mb` in `[video]`, default 4 GB) split in command-priority
  order, with a per-video cap (`preload_max_seconds`, default 60). See
  [docs/video-pipeline.md](docs/video-pipeline.md).
- Styled text with `+` concatenation in `text` and `textCont` — multiple
  styled segments on a single line, each with its own `colour` and `trans`.
  Line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the
  whole line.
- `textCont "..."` appends to the most recently pushed text entry across cues,
  so a line can be revealed piece by piece.
- `countdown(SECONDS)` shows a live `MM:SS` timer at the top of the slave that
  ticks down in real time, with an optional `"en"` / `"da"` language label.
  Holds at `00:00` until cleared; scene-scoped, so it never bleeds into the
  next scene.
- `typewrite(MS_PER_CHAR, TEXT)` *(1.8.2)* reveals a line one character at a
  time. Takes the full `text` grammar; advancing the cue snaps a half-typed
  line to complete so nothing is left mid-word on the slave.
- LaTeX symbol macros in displayed text *(1.8.2)*: all Greek letters plus ~45
  mathematical symbols and `\mathbb{X}` for any capital — `text "pick \delta
  \in \mathbb{R}"` renders `pick δ ∈ ℝ`. Every glyph is present in the four
  bundled DejaVu faces.
- `layer(N, content)` *(1.8.3)* gives images an explicit draw order; the
  default of 0 with ties by show order keeps pre-1.8.3 scenes identical.
- `pathMove(P1, …, PN, DURATION_MS[, MODE], content)` *(1.8.3)* moves content
  through multiple waypoints, with `once`, `loop`, or `pingpong` repeat.
- Canvas-relative slave text: text scales with the targeted-display rect in
  lockstep with images, videos, GIFs, and particles.

### Particle system

- Motion models: `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`
  (random walk), `OSCILLATION` (sinusoidal X with per-particle phase),
  `ORBIT` (circular motion around screen centre), `NOISE` (2D value-noise
  flow field), `LIFECURVE` (linear motion with a triangular scale + alpha
  envelope).
- `trail(N, MODE, content)` modifier with three looks: `GHOST` (distance-gated
  fade and shrink), `STRETCH` (single velocity-aligned quad per particle, real
  motion-blur streaks), and `GLOW` (Ghost geometry with additive blending).
  `trailGlow(N, content)` is sugar for `trail(N, GLOW, content)`.
- `bounce(true, content)` — particles reflect off the four screen edges with
  an 0.8 energy-loss coefficient per hit, plus constant downward gravity
  (≈ 900 px/s²) and a rest threshold so settled particles pile up on the floor
  and fade in place.
- Per-particle-type tunables in scene `[Options]`: `<TYPE>.SPEED`,
  `<TYPE>.DENS`, `<TYPE>.X.DIST = NAME(p1, p2)`, and velocity-shaping
  `<TYPE>.VX.DIST` / `<TYPE>.VY.DIST`. Distributions: `UNIFORM`, `NORMAL`,
  `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`. Project-level defaults under
  `[Particle.<TYPE>]` in `schema.toml`. Cascade is scene → project → built-in
  defaults.
- `stopParticleCont [GROUP]` stops spawning but lets live particles finish
  their animation. Sticky — later `stop` / `clear` / `clearAll` no longer cut
  the ending short.
- Multiple concurrent ungrouped particle systems coexist without a `group`
  wrapper. `clear`, `stop`, and `stopParticleCont` still act on every
  ungrouped system as a unit.
- `randomImages(folder)` combined with `particle()` picks a fresh random image
  per spawn.

### Animation and modifiers

- `move(FROM, TO, DURATION_MS, content)` — linear tween of position over time.
  `FROM` / `TO` accept `[x,y]` percent pairs or side letters `N`/`S`/`E`/`W`
  (spawns off-screen against that edge).
- `spin(degPerSec, content)` — continuous rotation. Composes with static
  `rotate()`.
- `scale(content, [w,h])` — pixel-dimension alias of `size()` with the
  argument order matching `random()` and the other content-first modifiers.
- `pathMove(P1, P2, …, PN, DURATION_MS[, MODE], content)` *(1.8.3)* — the
  multi-waypoint generalisation of `move()`, using the same endpoint
  vocabulary. `DURATION_MS` covers the whole path, split evenly per leg;
  `MODE` is `once` (default), `loop`, or `pingpong`.
- `layer(N, content)` *(1.8.3)* — explicit draw order for images. Higher is
  nearer the front; negatives sit behind unlayered content.

### Display and output

- Targeted-display rectangle inside the physical slave, with a hidden size
  menu (Debug → `S`, adjust with `Z/X/Q/E/WASD`, `SHIFT` = 10× finer steps,
  `C` = reset, `ESC` = save).
- Capture-mirror window (third OS window) for feeding a capture card or
  recorder. A borderless fullscreen output mirrors the slave's logical content
  — same composite, no targeted-display rect, no rotation, no letterbox — at
  slave-logical resolution. Display picker in `Options`, with a hard guard
  against picking the slave's own display.
- Windowed-slave dev mode: `Options` → `Slave Window Mode` → `Windowed (dev)`
  opens the slave as a resizable 1280×720 window on the primary display
  instead of fullscreen on the secondary. Saved targeted-display rect coords
  stay in production pixel space — the virtual slave is letterboxed inside
  the live window, so scenes look identical in both modes at proportional
  scale.

### Project management

- New-project wizard: selectable revy types (including `SaTyR`), a `Jubi`
  toggle (folder becomes `<Revy>Jubi_<year>`), and a custom-name field under
  `Other`.
- Project `schema.toml` defines revy-wide options and particle defaults; scene
  `[Options]` overrides per scene. *(2.0.1)* An option has the same name in
  both places, in any case, and every colour option — including the new
  `TextColour` default text colour — takes the same colour names, `#RRGGBB`
  and `#RGB`.

## Building

Full build instructions, including cross-compilation, are in
[docs/building.md](docs/building.md).

### Linux

SatyrAV needs SDL3 (3.2 or newer), SDL3_ttf and SDL3_image *(2.0.0)*. Debian 13
packages them:

```bash
sudo apt install cmake g++ libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev

mkdir build && cd build
cmake ..
make -j$(nproc)
```

Ubuntu 24.04 does not package SDL3. There, build it from source into
`~/sdl3-prefix` first and add `-DCMAKE_PREFIX_PATH=$HOME/sdl3-prefix` to the
`cmake` line, as described in [docs/building.md](docs/building.md).

### Cross-compile for Windows (MinGW via WSL, Debian or Ubuntu)

```bash
sudo apt install mingw-w64 mingw-w64-tools p7zip-full wget
chmod +x setup-mingw-deps.sh
rm -rf ~/mingw-deps             # only if it still holds SDL2 from a 1.x build
./setup-mingw-deps.sh           # installs SDL3 + FFmpeg to ~/mingw-deps

mkdir build-win && cd build-win
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/MinGW-w64.cmake \
      -DMINGW_DEPS_DIR=$HOME/mingw-deps ..
make -j$(nproc)
```

The required DLLs are copied next to `satyr-av.exe` automatically. To build a
Windows installer:

```bash
makensis ../installer/satyr-av.nsi
```

### Unit tests *(1.8.1)*

Tests are **off by default** — a normal build compiles no test code and
downloads no test framework. Opt in with one flag:

```bash
cmake .. -DSATYR_BUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

The suite covers the SDL-independent core (`SceneParser`, `SchemaParser`,
`CueEngine`, `Config`, `Project`, `Types`), so it also builds standalone on a
machine with no SDL3 or FFmpeg installed:

```bash
cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/satyr-tests
```

See [tests/README.md](tests/README.md) for the layout and for what is
deliberately left untested.

## License

SatyrAV is released under the MIT License — see [LICENSE](LICENSE).

### Third-party dependencies

SatyrAV links against the following third-party libraries. Their licenses are
compatible with MIT; each is redistributed unmodified, either via system
packages (Linux) or as dynamically linked DLLs next to the executable
(Windows).

| Library | License | Role |
|---|---|---|
| [SDL3](https://www.libsdl.org/) | zlib | Window, input, rendering |
| [SDL3_ttf](https://github.com/libsdl-org/SDL_ttf) | zlib | TrueType text rendering |
| [SDL3_image](https://github.com/libsdl-org/SDL_image) | zlib | Image and GIF loading |
| [FFmpeg](https://ffmpeg.org/) — libavcodec, libavformat, libavutil, libswscale, libswresample | LGPL-2.1-or-later | Video and audio decoding, hardware acceleration |
| [toml++](https://github.com/marzer/tomlplusplus) | MIT | TOML parsing for `config.toml` and `schema.toml` |
| MinGW-w64 runtime (libstdc++-6, libgcc_s_seh-1, libwinpthread-1) | GCC Runtime Library Exception | C++ runtime on Windows builds |

FFmpeg is used as a dynamically linked library under LGPL-2.1-or-later. No
FFmpeg sources are modified, and the DLLs (or system `.so` files on Linux) are
shipped alongside the executable so they can be replaced by the user, in
accordance with the LGPL.
