# Implemented

What was planned and shipped, newest version first — the planning history of SatyrAV. The full detail of every release is in [CHANGELOG.md](../CHANGELOG.md); what is planned next is in [planned.md](planned.md).

---

## 2.1.1 — shipped

A bug fix found in the show, plus one feature. See [CHANGELOG.md](../CHANGELOG.md) for the detail.

- **Fixed:** scene files saved on Windows (CR LF) read as empty on Linux.
- **Fixed:** `show` and `[Preload]` with pictures in folders and subfolders, and names with spaces, on Linux and Windows.
- **Wildcards in `[Preload]`** — `MEGA/*.png` for one folder, `MEGA/**/*.png` for it and every folder below.

---

## 2.1.0 — shipped

See [CHANGELOG.md](../CHANGELOG.md) for the detail.

- **Clearing the slave.** `BACKSPACE` clears the slave like `clearAll` *(was Critical)*.
- **Text outline per scene.** `TextOutline` per scene and `DisableTextOutline = true` *(both were Critical)*.
- **Capture mirror readout.** Debug popup `Capture:` line for the 1:1 check (the check on the show hardware is in [known-bugs.md](../known-bugs.md)).
- **F-key toggling** settled — both odd cases were real and are fixed; each key now checks its own sound. Also fixed: playing a sound while `M` had paused sound left the paused sounds silent.
- **Show log and subtitles for post-production** *(was Critical)*. *Show Log & Subtitles* in Options (off by default). One `.log` per show with everything — scenes and akter with their length, every cue, the operator keys, pictures and videos coming and going; one `.srt` and `.ass` per act with the text the audience saw (bold / italic; the `.ass` with font and size) and, in the `.srt`, one long cue per video and picture.
- **Clock and stopwatches.** *Clock on Master* in Options (off by default): time of day, scene and show stopwatches.
- **Fonts.**
	- `use FONT` / `use default` and `fontSize N` / `fontSize default` as comment rows that apply to the text below them in the scene file, scene-scoped.
	- Several fonts and sizes on one line with `textCont`, on a shared baseline.
	- Known bug fixed: fonts in the project's `font/` (and `fonts/`) folder now load, for `use` and for the `Font` options.
- **Repeating animations** *(was Critical)*. `[Animation]` section with `Name{ key = value }` blocks started by `show NAME`; any `show` / `text` expression; `repeat`, `for`, `every`, `randomX` / `randomY` in slave pixels; soft stop (`stop NAME`, `clear`, `clearImages`) and hard cut (`clear NAME`, `clearAll`, `BACKSPACE`, next scene).
- **See-through slave** *(was Critical, moved from 1.6)*. `TransparentBackground = true` per scene or project: the empty slave shows what runs underneath on that screen. The window is only opened see-through for projects that use it.
	- The open questions were settled as: the black fill outside the targeted rect becomes see-through too in such scenes; the capture mirror shows black (no chroma-key or NDI-with-alpha path).
- **Video seek** *(moved from 1.8.0)*. `,` / `.` move the playing video 5 s back / forward, SHIFT + `,` restarts it; a seek while paused shows the new frame at once.
- **Folder chooser** *(moved from 1.8.0)*. ENTER on *Projects Directory* in Options opens the system's folder dialog.
- **Project scanner.** `C` (or `D` → `O`) opens it, replacing the Overview popup: it reads the project from disk and reports every mistake with its file and line — `[Options]` names and values, unknown sections, missing files and fonts, `schema.toml` problems, scenes no akt lists.
- **VSCode extension** 0.5.0 knows the 2.1.0 syntax.
- **Render tests** *(deferred from 1.8.0)*. `satyr-render-tests` runs the real renderer, video player and app off-screen: slave pixels, a test clip's seeking, and a show driven by key presses, including the `[Animation]` runner and the show log.
- **Documentation.** `TODO.md` split into `plans/planned.md`, `plans/implemented.md` and `known-bugs.md`; user guides in `docs/howto/`, developer references in `docs/dev/`; version tags removed from the guides and references.

---

## 2.0.2 — shipped

A bug fix and two revy requests. See [CHANGELOG.md](../CHANGELOG.md) for the detail.

- **Fixed:** a scene's font no longer carries into the scenes after it.
- **Backstage screen** *(moved forward from 2.1.0)* — each scene change sends the current scene and the next two (from each scene's `[Backstage]` block) to the backstage ticker, in the background, logged, with a status line in the Debug popup. `[Backstage] Url` / `Enabled` in `schema.toml`; on in new projects.
- **Fixed text centre** — `FixedTextCentre = true` lays out centred text for all its lines up to the next clear, so lines don't jump as they appear. On in new projects.

---

## 2.0.1 — shipped

Bug-fix release for 2.0.0, plus two additions from the AV team. See [CHANGELOG.md](../CHANGELOG.md) for the detail.

- **`TextColour`** — a default text colour per scene or per project, in any colour name, `#RRGGBB` or `#RGB`.
- **Mark-line log** — `O` during a show writes the last executed cue's file, line and code to `fix-it/ErrorInFiles-<date>.LOG` in the project folder; one file per day, appended across restarts, created on the day's first mark.
- **One name per option, one set of colours.** Each option has one documented name, the same in a scene and in `schema.toml`, in any case; the docs say `colour`. Older spellings (`bc`, `cap`, `color(...)`, …) still work, undocumented. Every colour option takes the same colours, except that a subtitle's `grey` stays `#AAAAAA`.
- **Background colour cascades** scene → project → black, like the font. A scene that sets none now follows the project.
- **Fixed:** videos freezing on their first frame with no audio device; `stop FILE` doing nothing; `K` pausing F-key sounds and `M` pausing a video's sound.
- Windowed dev mode re-tested on the SDL3 build: works as it should.

---

## 2.0.0 — shipped

The SDL2 → SDL3 backend migration, plus the work that depended on it. See [CHANGELOG.md](../CHANGELOG.md) for the detail.

- **SDL3.** The whole render, input and audio layer moved to SDL3 on the existing 2D renderer. Display selection, window creation and text input were adapted without changing any config file or `schema.toml`.
- **Zero-copy hardware video.** On Windows a decoded frame goes from the decoder to the screen without leaving the GPU; the old read-back path remains as the automatic fall-back. Controlled by `[video] zero_copy` in `config.toml`.
- **Overlapping audio** — the one deliberate change in behaviour. F-key sounds play over video sound and over each other. Plus an opt-in `AudioFade` option that makes `play` fade times fade the sound as well as the picture.
- **Seek support in the video pipeline** — plumbing only; the commands are the 2.1.0 fast-forward item.
- **Diagnostics.** Failures are written to the log instead of a console that does not exist on Windows, and the debug overlay shows which decode path is in use and how many sounds are playing.
- **Fixed:** the capture mirror losing resolution when the targeted-display rect is shrunk.

Not finished in 2.0.0, and carried under [Known bugs](../known-bugs.md): the capture mirror's stutter, `stop FILE`, `K`-pause silencing F-key sounds, video size on the fall-back paths, and the last frames of a clip being dropped. Windowed dev mode was not re-tested on the SDL3 build. *(2.0.1 fixed `stop FILE` and the `K`-pause problem, and windowed dev mode has since been tested and works.)*

### Decided against

Evaluated during the scoping review and dropped from 2.0.0:

- **`SDL3 GPU API`** — a new *low-level* API (command buffers, pipelines, hand-written shaders) with Vulkan/D3D12/Metal backends only — **no D3D11**. Adopting it means rewriting the whole render layer, and since hardware decode is D3D11VA (D3D11 textures), `SDL_GPU` cannot consume decoder output without an interop copy — it works *against* the zero-copy goal. The `SDL_Render` migration already delivers the multi-backend benefit.
- **`SDL3 Process API`** — no part of SatyrAV spawns child processes (FFmpeg is linked as libraries). Add later if a concrete need appears.
- **`SDL3 System Tray API`** — SatyrAV is a foreground operator tool; no roadmap feature needs a tray icon. It is a small, isolated feature that can be added in any release if wanted — it does not need to be coupled to the migration.
- **`SDL3 Time API` as a feature** — `<chrono>` (C++17) already provides calendar time. The in-project clock and per-scene stopwatch *command* are user features, moved to 2.1.0.

---

## 1.8 — shipped (five sub-versions)

**The 1.8 cycle is complete.** It shipped as five sub-versions ordered **easiest &rarr; hardest**, each landing and being verified on its own before the next began. Nothing in it depended on the 2.0.0 SDL3 migration — see the *Decided against* / move notes in the 2.0.0 and 2.1.0 sections for what was pulled out of this cycle along the way.

### 1.8.0 — shipped

Project scaffolding & docs. Generated `schema.toml` now carries every project option at its default value, a new project ships with two akter and four tutorial scenes (text / picture / particle / group-macro), and `docs/building.md` gained a grouped troubleshooting section. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.8.1 — shipped

Unit testing. doctest suite over the SDL-independent core (`SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project`, `Types`), opt-in via `-DSATYR_BUILD_TESTS=ON` and off by default. Also buildable without SDL2 / FFmpeg. See [CHANGELOG.md](../CHANGELOG.md) for details and for what is deliberately left untested.

### 1.8.2 — shipped

Text rendering features. LaTeX symbol macros in displayed text (all Greek plus ~45 maths symbols and `\mathbb{X}`), and the `typewrite(MS_PER_CHAR, TEXT)` command. Also fixed two parser bugs that shared a root cause: `#RRGGBB` hex colours and `&` inside quoted text. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.8.3 — shipped

Image object control. New `layer(N, content)` modifier gives images an explicit draw order (default `0`, ties by show order, so existing scenes are unchanged), and `pathMove(P1, …, PN, MS[, MODE], content)` moves content through multiple waypoints with `once` / `loop` / `pingpong` repeat. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.8.4 — shipped

Cue Notes window. New `[Cue]` section in `.ngk` files holds named operator notes; a command claims one with a trailing `\Name`. A fourth, read-only column in the master window draws each note beside its command, scrolling in lockstep and wrapping long notes onto extra rows. See [CHANGELOG.md](../CHANGELOG.md) for details.

---

## 1.7.0 — shipped

Alternate quote delimiters (`"…"` / `'…'` / `´…´`) so text can contain quotation marks, plus a live `countdown` command with optional `en` / `da` language labels and a companion `clearCountdown`. See [CHANGELOG.md](../CHANGELOG.md) for details.

---

## 1.6 — shipped (seven sub-versions)

The 1.6 cycle was large, so it shipped as several sub-versions. Order is dependency- and risk-driven, not pure priority. PNG transparency (originally Critical for 1.6) has been moved to **2.1.0** because SDL2 has no per-pixel-alpha window path; see that section for the full rationale.

### 1.6.0 — shipped

Foundation pass. Slave render correctness + accumulated cheap bugs. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.1 — shipped

Real italic / bold typography on the slave; `textD` translation/subtitle command; shared `FONTS` directory; bundled DejaVu Sans family. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.2 — shipped

Load Project page redesign — search, Revy-type filter, sort, grouped table with creator column. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.3 — shipped

Audio device picker, F1–F12 audio hotkeys (with toggle behaviour), global logger, Overview Debug popup with parser warnings, and a baseline VSCode `.ngk` syntax extension. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.4 — shipped

Bug-fix release. Wrap-aware command-list scrolling, word-wrap on the slave display, dropdowns no longer freeze open after ESC + return, and project re-entry now starts the slave from a clean state. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.5 — shipped

Single bug-fix release: ungrouped `show` calls now stack on screen instead of replacing each other. See [CHANGELOG.md](../CHANGELOG.md) for details.

### 1.6.6 — shipped

Bug-fix release plus one small feature: `play` videos gained optional fade-in / fade-out times, `#` inside quoted text no longer truncates the cue, and `.ngk` filenames with `æøå` now open on Windows. See [CHANGELOG.md](../CHANGELOG.md) for details.
