# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

> **Current release: 2.1.0-betaE** (beta). Clear-screen key, per-scene outline, show time log and clock, `use` / `fontSize`, repeating animations and a see-through slave. The rest of **2.1.0** is below.

The lists, with the exception of the bug list, is in no particular order. Some have the (**Critical**) on them which means they are important and should be looked at first in the update version, before the others, if it makes sense.

Bugs aren't tied to specific versions. It is just a list of all known bugs which we currently have to fix at some point. Elements on the bug lists might not be bugs but specific things we have to check such that we know this won't become a bug later on, or may be a current bug we haven't tested for.

---

## Planned for 2.3.0
- include `SDL_camera.h` for direct camera input which is used sometimes.
- Add full Vulcan. No OpenGL support since it is essentially deprecated at this point for modern hardware.
- (**Critical**) Add `TexReader.cpp` functionality to read $\TeX$ files and parse it into `.ngk` files.
	- All `.ngk` files should be handled by having the `[backstage]` created by the parser.
    - A sketch should be empty for any commands. This will have to be added manually later.
    - A song should read and its lines should be added as `text "<the line>"` with `clear` in between a few lines. This is the default `.tex` file:
        ```tex
        \documentclass[a4paper,11pt]{article}

        \usepackage{revy}
        \usepackage[utf8]{inputenc}
        \usepackage[T1]{fontenc}
        \usepackage[danish]{babel}
        
        \revyname{TestRevyen}
        \revyyear{2026}
        \version{0.1}
        \eta{$4$ minutter}
        \status{Ikke færdig}
        
        \title{Titel}
        \author{Forfatter}
        \melody{Kunstner: ``Originaltitel''}

        \begin{document}
        \maketitle

        \begin{roles}
        \role{S}[Skuespiller] Rolleforklaring
        \end{roles}

        \begin{song}
          \sings{S} Denne sang rimer knapt
        
          \sings{S}[omkv] Ringe akt
        
        \end{song}
        \end{document}
        ```
    - The `.tex` file has a `\begin{roles}...\end{roles}`. This is where we find our actors.
    - The `\begin{song}...\end{song}` defines if it is a song. If it is a sketch it has `\begin{sketch}...\end{sketch}`.
    - The `\title{}` should be the title of the sketch/scene and should also be the name of the file. So in this case, the ngk file should look like this:
        ```txt
        [Options]
        bc = black
        
        [Backstage]
        Name         = Titel
        ExpectedTime = 4m0s
        Actors       = ["Skuespiller"]
        Ninjas       = [""]

        [Commands]
        text "Denne sang rimer knapt"
        
        clear
        text "Ringe akt"
        
		#End of song
        clear
        ```
    - A proper parser has to be created since some write minutes, minutter, min, sekunder, seconds, etc. So we have to make sure to be able to understand what is actually written. Some also just write `$n$ minutter` which indicate that they have no idea. Sometimes they also include the `$` and sometimes they do not. Also if any of these information is missing, we just have to give default values.
        - If title is missing we just write: "No title found"
        - If eta is missing we just write: "69m:00"
        - If Actors is missing, we write no actor names and the same with nijas.
        - If for some reason our parser cannot read it, or don't understand it, these above values is also the default values for this case.

---

## Planned for 2.2.0
- Add powerpoint animetions transitions for picture to picture. Essentially making it possible to run a slideshow like in powerpoint.
- (*Maybe defer to later*) Add messenger/other communication app default template to mimic communication.
- Add an application-window capture source (Windows only) so we can stream another running program onto the slave display. Video only — no audio capture.
	- **Backend.** Windows Graphics Capture (WGC) via `GraphicsCaptureItem::CreateFromHWND` &rarr; `Direct3D11CaptureFramePool` &rarr; `FrameArrived`, sharing the existing D3D11 device with SDL/FFmpeg so the captured BGRA texture stays on the GPU. Requires Win11 (no yellow capture border).
	- **Cue integration.** New `capture "<window-title>"` keyword in `SceneParser`, new `CommandType::Capture`, dispatched through `CueEngine` &rarr; `SlaveWindow`. Tear-down via a `stopCapture` command (or fold into existing clear semantics — decide during design).
	- **Slave draw layer.** New layer in `SlaveWindow` between the video layer and the images layer. **Must honour the `[TargetedDisplay]` rect** so the captured window can be moved / scaled / rotated like images via `D → S`.
	- **Window identity.** Case-insensitive **substring match on the window title** with a fallback message if not found. Easy for operators; revisit if it proves too fuzzy in practice.
	- **Picker UI.** New popup that enumerates top-level windows (`EnumWindows`, filtered to visible + non-empty title, excludes SatyrAV's own HWNDs) so the operator can pick at scene-edit time. The chosen title substring is written back into the `.ngk`.
	- **Phased landing:** (1) backend `WindowCaptureSource` class, verified against a debug texture; (2) `SlaveWindow` draw layer with rect support; (3) `SceneParser` / `CueEngine` wiring; (4) picker popup.
	- **Why deferred from 1.7:** WGC needs WinRT ABI headers (`<windows.graphics.capture.h>` full method tables, `<windows.graphics.directx.direct3d11.interop.h>`, working `__uuidof` on templated `ITypedEventHandler`, etc.). The Debian mingw-w64 13 sysroot used for the current cross-compile only ships partial forward-decls — every workaround we tried hit a new gap (no `<wrl/event.h>`, no `<wrl/implements.h>`, `__declspec(uuid)` silently ignored, missing `IDirect3D11CaptureFramePool` method tables, missing `WindowsCreateStringReference` decl). Lands cleanly once the toolchain catches up — naturally that's the SDL3 / D3D11 reshuffle in 2.0.0 + MSYS2 (or equivalent full Windows SDK) by 2.2.0.
- *(deferred from 2.1.0)* Create full physics model for particle systems, accelerated with hardware
    - While adding the physics models we should also look at how we could optimize it. Currently it lacks if to many "particels" is displayed. Especially on older hardware. `SIMD` comes to mind and also maybe using Broad-Phase Collision Detection.
- *(deferred from 2.1.0)* OSC Receiver, i.e. listen for OSC messages from lightning desk to control cues. Mainly interested in QLab and maybe GrandMA. 
---

## Planned for 2.1.0

What is left of 2.1.0 after the beta. The next phase (**F**) is the video seek commands and the folder chooser.

- *(moved from 1.8.0)* Make a fastforward-goback scroller for videos.
	- Probably should just go forward or backward with 5 seconds interval using key `,`/`.`.
	- A restart from the beginning function is also needed.
	- *Why deferred:* needs `av_seek_frame` + decode-ring flush inside `VideoPlayback` — the exact code 2.0.0 rebuilds for zero-copy decode. The 2.0.0 redo leaves the pipeline seek-capable; this item is the user-facing commands on top of it.
	- *Status after 2.0.0:* the plumbing exists and is tested on all three decode paths — `MediaPlayer::SeekVideo(seconds)`, `GetVideoPosition()` and `GetVideoDuration()`. What is left for this item is the commands, the key bindings, and one behaviour to decide: a seek made while paused currently shows its new picture only when playback resumes.
	- *Keys after the beta:* `,` and `.` are still free; `BACKSPACE` is now taken by the clear-screen key.
- *(moved from 1.8.0)* Create a true folder chooser for the folder picker inside Options.
	- When pressing the folder-choose control for where projects are stored, a native folder dialog should open so the user can pick the storage folder.
	- *Why deferred:* SDL3 ships [`SDL_ShowOpenFolderDialog`](https://wiki.libsdl.org/SDL3/SDL_ShowOpenFolderDialog) (cross-platform). Doing this pre-SDL3 means hand-rolling a Win32 COM `IFileDialog` path that would then be thrown away — exactly the double work to avoid.
	- *Layout note:* the Options screen rows were tightened to 70 px in the beta to fit *Clock on Master*; a chooser button must still fit on a 720-high master.
- Make sure the capture window is an unscaled 1-1 copy of the slave window.
	- *Status after the beta:* by the code, the capture copies the slave canvas pixel for pixel (before the targeted-display rect, as intended since 2.0.0) and is only scaled when the capture screen's resolution differs from the canvas. The Debug popup's new `Capture:` line shows `1:1` (green) or the scaling (orange). **Left: check it on the show hardware.**
- A full scanner of a project to see if any scenes inside its `.ngk` files have wrong options/notations/commands. A full scanner to check for existence of assets should also be implemented.
    - A shortcut to open specific window should be implemented. Just like how the debug window work. 
    - The scan should be ran only when this window is open to minimize the amount of processing power needed during a show.
    - It should report the physical line in the specific files that are wrong. 
	- *Starting point:* the Overview popup (`D → O`) already lists unknown keywords, missing `show` / `play` files, bad `[Backstage]` blocks, bad `fontSize` / `use` lines, `[Animation]` mistakes and missing animation images, each with its line. The scanner can grow from it.
	- It must know the beta's new syntax: `use`, `fontSize`, `[Animation]` and its keys, `TextOutline` / `DisableTextOutline` / `TransparentBackground` in scenes.
- Update the VSCode `.ngk` extension for the beta's syntax: `use`, `fontSize`, the `[Animation]` section and its keys, and the options `DisableTextOutline` and `TransparentBackground`.
- *(deferred from 1.8.0)* Extend the unit-test suite to the render / audio / `MediaPlayer` layer, once the SDL3 backend has settled. The SDL-independent core suite ships in **1.8.1**; see that section.
	- *Starting point:* the beta's features were checked with off-screen renders of the real `Renderer`, `SlaveWindow` and `TextRenderer` (SDL's `offscreen` video driver). Those harnesses could become the first render tests. Not covered by anything yet: the `[Animation]` runner and the show time log's wiring, which live in `PerformanceScreen`.

---

## 2.1.0-betaE — shipped (beta)

The first beta of 2.1.0, built in five phases and tested after each: **A** `2.1.0-betaA`, **B** `-betaB`, **C** `-betaC`, **D** `-betaD`, **E** `-betaE`. See [CHANGELOG.md](CHANGELOG.md) for the detail.

- **A — Quick fixes.**
	- `BACKSPACE` clears the slave like `clearAll` *(was Critical)*.
	- `TextOutline` per scene and `DisableTextOutline = true` *(both were Critical)*.
	- Debug popup `Capture:` line for the 1:1 check (hardware test still open, see above).
	- F-key toggling: settled — both odd cases were real and are fixed; each key now checks its own sound. Also fixed: playing a sound while `M` had paused sound left the paused sounds silent.
- **B — Show time log, clock and stopwatches** *(was Critical)*. `showlog/<Revy>-YYYY-MM-DD-N.log` and `.srt` per run, from the first cue: scenes with their length, every cue, and the operator keys. *Clock on Master* in Options (off by default): time of day, scene and show stopwatches.
- **C — Fonts.**
	- `use FONT` / `use default` and `fontSize N` / `fontSize default` as comment rows that apply to the text below them in the scene file, scene-scoped.
	- Several fonts and sizes on one line with `textCont`, on a shared baseline.
	- Known bug fixed: fonts in the project's `font/` (and `fonts/`) folder now load, for `use` and for the `Font` options.
- **D — Repeating animations** *(was Critical)*. `[Animation]` section with `Name{ key = value }` blocks started by `show NAME`; any `show` / `text` expression; `repeat`, `for`, `every`, `randomX` / `randomY` in slave pixels; soft stop (`stop NAME`, `clear`, `clearImages`) and hard cut (`clear NAME`, `clearAll`, `BACKSPACE`, next scene).
- **E — See-through slave** *(was Critical, moved from 1.6)*. `TransparentBackground = true` per scene or project: the empty slave shows what runs underneath on that screen. The window is only opened see-through for projects that use it. Capture shows black there.
	- The open questions from the TODO were settled as: the black fill outside the targeted rect becomes see-through too in such scenes; the capture mirror shows black (no chroma-key or NDI-with-alpha path).

---

## 2.0.2 — shipped

A bug fix and two revy requests. See [CHANGELOG.md](CHANGELOG.md) for the detail.

- **Fixed:** a scene's font no longer carries into the scenes after it.
- **Backstage screen** *(moved forward from 2.1.0)* — each scene change sends the current scene and the next two (from each scene's `[Backstage]` block) to the backstage ticker, in the background, logged, with a status line in the Debug popup. `[Backstage] Url` / `Enabled` in `schema.toml`; on in new projects.
- **Fixed text centre** — `FixedTextCentre = true` lays out centred text for all its lines up to the next clear, so lines don't jump as they appear. On in new projects.

---

## 2.0.1 — shipped

Bug-fix release for 2.0.0, plus two additions from the AV team. See [CHANGELOG.md](CHANGELOG.md) for the detail.

- **`TextColour`** — a default text colour per scene or per project, in any colour name, `#RRGGBB` or `#RGB`.
- **Mark-line log** — `O` during a show writes the last executed cue's file, line and code to `fix-it/ErrorInFiles-<date>.LOG` in the project folder; one file per day, appended across restarts, created on the day's first mark.
- **One name per option, one set of colours.** Each option has one documented name, the same in a scene and in `schema.toml`, in any case; the docs say `colour`. Older spellings (`bc`, `cap`, `color(...)`, …) still work, undocumented. Every colour option takes the same colours, except that a subtitle's `grey` stays `#AAAAAA`.
- **Background colour cascades** scene → project → black, like the font. A scene that sets none now follows the project.
- **Fixed:** videos freezing on their first frame with no audio device; `stop FILE` doing nothing; `K` pausing F-key sounds and `M` pausing a video's sound.
- Windowed dev mode re-tested on the SDL3 build: works as it should.

---

## 2.0.0 — shipped

The SDL2 → SDL3 backend migration, plus the work that depended on it. See [CHANGELOG.md](CHANGELOG.md) for the detail.

- **SDL3.** The whole render, input and audio layer moved to SDL3 on the existing 2D renderer. Display selection, window creation and text input were adapted without changing any config file or `schema.toml`.
- **Zero-copy hardware video.** On Windows a decoded frame goes from the decoder to the screen without leaving the GPU; the old read-back path remains as the automatic fall-back. Controlled by `[video] zero_copy` in `config.toml`.
- **Overlapping audio** — the one deliberate change in behaviour. F-key sounds play over video sound and over each other. Plus an opt-in `AudioFade` option that makes `play` fade times fade the sound as well as the picture.
- **Seek support in the video pipeline** — plumbing only; the commands are the 2.1.0 fast-forward item.
- **Diagnostics.** Failures are written to the log instead of a console that does not exist on Windows, and the debug overlay shows which decode path is in use and how many sounds are playing.
- **Fixed:** the capture mirror losing resolution when the targeted-display rect is shrunk.

Not finished in 2.0.0, and carried under [Known bugs](#known-bugs): the capture mirror's stutter, `stop FILE`, `K`-pause silencing F-key sounds, video size on the fall-back paths, and the last frames of a clip being dropped. Windowed dev mode was not re-tested on the SDL3 build. *(2.0.1 fixed `stop FILE` and the `K`-pause problem, and windowed dev mode has since been tested and works.)*

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

Project scaffolding & docs. Generated `schema.toml` now carries every project option at its default value, a new project ships with two akter and four tutorial scenes (text / picture / particle / group-macro), and `docs/building.md` gained a grouped troubleshooting section. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.8.1 — shipped

Unit testing. doctest suite over the SDL-independent core (`SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project`, `Types`), opt-in via `-DSATYR_BUILD_TESTS=ON` and off by default. Also buildable without SDL2 / FFmpeg. See [CHANGELOG.md](CHANGELOG.md) for details and for what is deliberately left untested.

### 1.8.2 — shipped

Text rendering features. LaTeX symbol macros in displayed text (all Greek plus ~45 maths symbols and `\mathbb{X}`), and the `typewrite(MS_PER_CHAR, TEXT)` command. Also fixed two parser bugs that shared a root cause: `#RRGGBB` hex colours and `&` inside quoted text. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.8.3 — shipped

Image object control. New `layer(N, content)` modifier gives images an explicit draw order (default `0`, ties by show order, so existing scenes are unchanged), and `pathMove(P1, …, PN, MS[, MODE], content)` moves content through multiple waypoints with `once` / `loop` / `pingpong` repeat. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.8.4 — shipped

Cue Notes window. New `[Cue]` section in `.ngk` files holds named operator notes; a command claims one with a trailing `\Name`. A fourth, read-only column in the master window draws each note beside its command, scrolling in lockstep and wrapping long notes onto extra rows. See [CHANGELOG.md](CHANGELOG.md) for details.

---

## 1.7.0 — shipped

Alternate quote delimiters (`"…"` / `'…'` / `´…´`) so text can contain quotation marks, plus a live `countdown` command with optional `en` / `da` language labels and a companion `clearCountdown`. See [CHANGELOG.md](CHANGELOG.md) for details.

---

## 1.6 — shipped (seven sub-versions)

The 1.6 cycle was large, so it shipped as several sub-versions. Order is dependency- and risk-driven, not pure priority. PNG transparency (originally Critical for 1.6) has been moved to **2.1.0** because SDL2 has no per-pixel-alpha window path; see that section for the full rationale.

### 1.6.0 — shipped

Foundation pass. Slave render correctness + accumulated cheap bugs. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.1 — shipped

Real italic / bold typography on the slave; `textD` translation/subtitle command; shared `FONTS` directory; bundled DejaVu Sans family. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.2 — shipped

Load Project page redesign — search, Revy-type filter, sort, grouped table with creator column. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.3 — shipped

Audio device picker, F1–F12 audio hotkeys (with toggle behaviour), global logger, Overview Debug popup with parser warnings, and a baseline VSCode `.ngk` syntax extension. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.4 — shipped

Bug-fix release. Wrap-aware command-list scrolling, word-wrap on the slave display, dropdowns no longer freeze open after ESC + return, and project re-entry now starts the slave from a clean state. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.5 — shipped

Single bug-fix release: ungrouped `show` calls now stack on screen instead of replacing each other. See [CHANGELOG.md](CHANGELOG.md) for details.

### 1.6.6 — shipped

Bug-fix release plus one small feature: `play` videos gained optional fade-in / fade-out times, `#` inside quoted text no longer truncates the cue, and `.ngk` filenames with `æøå` now open on Windows. See [CHANGELOG.md](CHANGELOG.md) for details.

---

## Known bugs

Most critical first.

- (**Critical**) **The capture mirror stutters during video and particles.** Which screens are affected has depended on which physical monitor each window is on.
	- **History.** Before 2.0.0 the report was the other way round: the slave stuttered and the capture mirror looked smooth. Zero-copy video made the slave much smoother, so decoding was a real part of the slave's problem and that part is fixed. What remains shows on the capture mirror.
	- **Two suspects, from reading the frame loop. Neither is confirmed — this needs measurement, not more reasoning.**
		1. **A full GPU→CPU read-back every frame.** `Renderer::UpdateCaptureFromSlaveTarget` calls `SDL_RenderReadPixels` over the whole canvas — about 33 MB per frame at 4K — and the GPU must drain before it returns. The capture window is a separate renderer on a separate D3D11 device, which is why the frame has to cross through system RAM at all. Since zero-copy landed this is the leading suspect.
		2. **Three serialised vsync waits per frame.** `PresentSlave` ([PerformanceScreen.cpp](../src/screens/PerformanceScreen.cpp)), then `PresentMaster` and `PresentCapture` ([App.cpp](../src/App.cpp)) all run on the main thread with vsync on. The monitors are not the same refresh rate, so the loop waits on three independent vblanks that drift in and out of phase.
	- **Before fixing, instrument.** A frame-time breakdown in the debug overlay and the log: total frame time, read-back time, and each present's blocking time separately. "Sometimes stutters" is exactly the shape of bug where fixing the wrong suspect looks like success for ten minutes.
	- **Likely remedies once measured:** hand the frame between the two D3D11 devices as a shared texture instead of reading it back (2.0.0's thread-safe, shared-device work makes this realistic), and vsync on the slave only — tearing matters on the audience output, while the master is an operator screen and a capture card samples at its own rate.
	- The particle half of the original report has not been looked at separately. If particles still stutter on the slave with no video playing, that is its own bug with its own cause.
- **On the fall-back video paths the picture is decoded at the size of the targeted-display rect.** Left over from the 2.0.0 capture-mirror resolution fix. `PerformanceScreen` still passes the rect's size to `PlayVideo` and `PreloadVideosForScenes`, so on the read-back and software paths a shrunk rect gives a low-resolution video that the capture mirror then stretches back up. The zero-copy path is not affected (it always keeps the source size), so on Windows with hardware decode this only shows for 10-bit clips or with `zero_copy = false`; on Linux it always applies. Fix: pass the canvas size at those two call sites. The cost is more CPU on those paths when the rect is shrunk.
- **The last frames of every clip are dropped.** The decoder is never flushed at end of file, so whatever it is still holding is lost. With hardware decode that is a few frames; with software decode it is up to one frame per decoder thread, which on a many-core machine can be most of a second. Predates 2.0.0. Fix: send the decoder an end-of-stream packet and drain it before the decode thread parks.
- Run doesn't work as intended in macro environment.
- Maybe only one image can be shown at a time. This need to be tested.
- Not a bug, but we have to check what bit our movies is compatible with. 8/10 and 16 bit should all be suported.
	- *State in 2.0.0:* 8-bit H.264 is the tested case and uses the zero-copy path. A 10-bit source is deliberately kept on the read-back path, where it is converted down to 8-bit for display, so it should play but without its extra depth. That has **not** been tested with a real 10-bit file, and 16-bit has not been looked at.
