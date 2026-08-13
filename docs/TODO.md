# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

> **Current release: 1.8.4.** The 1.8 cycle is complete — all five sub-versions have shipped. The next planned work is **2.0.0**, the SDL3 backend migration.

The lists, with the exception of the bug list, is in no particular order. Some have the (**Critical**) on them which means they are important and should be looked at first in the update version, before the others, if it makes sense.

Bugs aren't tied to specific versions. It is just a list of all known bugs which we currently have to fix at some point. Elements on the bug lists might not be bugs but specific things we have to check such that we know this won't become a bug later on, or may be a current bug we haven't tested for.

---

## Planned for 2.2.0

- Add an application-window capture source (Windows only) so we can stream another running program onto the slave display. Video only — no audio capture.
	- **Backend.** Windows Graphics Capture (WGC) via `GraphicsCaptureItem::CreateFromHWND` &rarr; `Direct3D11CaptureFramePool` &rarr; `FrameArrived`, sharing the existing D3D11 device with SDL/FFmpeg so the captured BGRA texture stays on the GPU. Requires Win11 (no yellow capture border).
	- **Cue integration.** New `capture "<window-title>"` keyword in `SceneParser`, new `CommandType::Capture`, dispatched through `CueEngine` &rarr; `SlaveWindow`. Tear-down via a `stopCapture` command (or fold into existing clear semantics — decide during design).
	- **Slave draw layer.** New layer in `SlaveWindow` between the video layer and the images layer. **Must honour the `[TargetedDisplay]` rect** so the captured window can be moved / scaled / rotated like images via `D → S`.
	- **Window identity.** Case-insensitive **substring match on the window title** with a fallback message if not found. Easy for operators; revisit if it proves too fuzzy in practice.
	- **Picker UI.** New popup that enumerates top-level windows (`EnumWindows`, filtered to visible + non-empty title, excludes SatyrAV's own HWNDs) so the operator can pick at scene-edit time. The chosen title substring is written back into the `.ngk`.
	- **Phased landing:** (1) backend `WindowCaptureSource` class, verified against a debug texture; (2) `SlaveWindow` draw layer with rect support; (3) `SceneParser` / `CueEngine` wiring; (4) picker popup.
	- **Why deferred from 1.7:** WGC needs WinRT ABI headers (`<windows.graphics.capture.h>` full method tables, `<windows.graphics.directx.direct3d11.interop.h>`, working `__uuidof` on templated `ITypedEventHandler`, etc.). The Debian mingw-w64 13 sysroot used for the current cross-compile only ships partial forward-decls — every workaround we tried hit a new gap (no `<wrl/event.h>`, no `<wrl/implements.h>`, `__declspec(uuid)` silently ignored, missing `IDirect3D11CaptureFramePool` method tables, missing `WindowsCreateStringReference` decl). Lands cleanly once the toolchain catches up — naturally that's the SDL3 / D3D11 reshuffle in 2.0.0 + MSYS2 (or equivalent full Windows SDK) by 2.2.0.

---

## Planned for 2.1.0

- (**Critical**) Add an output file like a `.srt` or `.ass` file that creates a time log when a show is running. This can help with time alignment in post production.
	- This is a style example:
		```txt
		01/12-2026 | 19:30:126 | Started MatRevy2026
		01/12-2026 | 19:32:634 | Akt 1 | Scene TeXIntro | play TeXIntro.mp4
		01/12-2026 | 19:57:238 | Akt 1 | Scene RandomSketch | show funnyImage.png
		...
		```
	- The file should be named the project name and the date it has been created and then a number (or something similar) if you have started the show more than once during the day.
	- The exact details of what it should show has not been decided. A design run through may be needed. But essentially everytime we change a scene or show something, like an image, movie or text.
- OSC Receiver, i.e. listen for OSC messages from lightning desk to control cues. Mainly interested in QLab and maybe GrandMA. 
- Expose HTTP/WebSocket endpoint to send out message of which Scene and akt we are currently on.
- Create full physics model for particle systems, accelerated with hardware
- A full scanner of a project to see if any scenes inside its `.ngk` files have wrong options/notations/commands. A full scanner to check for existence of assets should also be implemented.
- *(moved from 1.8.0)* Make a fastforward-goback scroller for videos.
	- Probably should just go forward or backward with 5 seconds interval using key `,`/`.`.
	- A restart from the beginning function is also needed.
	- *Why deferred:* needs `av_seek_frame` + decode-ring flush inside `VideoPlayback` — the exact code 2.0.0 rebuilds for zero-copy decode. The 2.0.0 redo leaves the pipeline seek-capable; this item is the user-facing commands on top of it.
- *(moved from 1.8.0)* Create a true folder chooser for the folder picker inside Options.
	- When pressing the folder-choose control for where projects are stored, a native folder dialog should open so the user can pick the storage folder.
	- *Why deferred:* SDL3 ships [`SDL_ShowOpenFolderDialog`](https://wiki.libsdl.org/SDL3/SDL_ShowOpenFolderDialog) (cross-platform). Doing this pre-SDL3 means hand-rolling a Win32 COM `IFileDialog` path that would then be thrown away — exactly the double work to avoid.
- *(moved from 2.0.0 Time API)* Add an in-project clock for easy time-of-day display, and a per-scene stopwatch command to time individual scenes. Pairs naturally with the `.srt`/`.ass` time-log above.
- *(deferred from 1.8.0)* Extend the unit-test suite to the render / audio / `MediaPlayer` layer, once the SDL3 backend has settled. The SDL-independent core suite ships in **1.8.1**; see that section.
- (**Critical — moved from 1.6**) Transparent-layer handling for `.png` files. Today the alpha channel in a PNG is composited onto the slave background as normal. Add a per-project `[Options]` toggle (default **off**) that switches the slave to a transparent composite path — the underlying pixels show through PNG alpha instead of being filled by the scene background.
	- *Why deferred from 1.6:* SDL2 has no per-pixel-alpha window flag; `SDL_WINDOW_TRANSPARENT` is SDL3-only. A SDL2 implementation would require hand-rolling a D3D11 + DirectComposition swapchain to replace SDL2's slave renderer (and a separate ARGB-visual path on Linux/X11). After the 2.0.0 SDL3 migration this becomes a one-flag change on window creation, so it rides naturally with 2.1.
	- Still needs investigation of how this interacts with the targeted-display black-fill and with the capture-mirror window (capture cards drop alpha; may need a chroma-key or NDI-with-alpha output path).

---

## Planned for 2.0.0

**General notes about this major update:** A backend-only release — user-facing functionality should not change, with one deliberate exception (see *Audio* below). Migrating to `SDL3` gives a renderer that auto-selects D3D11/D3D12/Vulkan/Metal/OpenGL (better integrated-GPU coverage), a real audio-stream abstraction, and the interop needed for zero-copy video. The SDL3 feature list was reviewed and trimmed — see *Decided against* for what was evaluated and dropped, so it doesn't get re-litigated later.

> **Terminology:** earlier drafts said "hardware encoding/decoding". SatyrAV never encodes video — it only decodes and plays. Every item below concerns hardware **decode** and the GPU upload path.

### Core migration

- Update `SDL2` &rarr; `SDL3`, migrating the existing 2D renderer (`SDL_Render`).
	- Staying on `SDL_Render` (**not** the new low-level `SDL_GPU`) already delivers D3D11/D3D12/Vulkan/Metal/OpenGL backend selection and better integrated-GPU support for free. See *Decided against* for why `SDL_GPU` was rejected.
	- **Async window state.** SDL3 window operations (move / resize / fullscreen) are asynchronous on some backends. Every place that sets slave / capture-mirror window geometry and then reads it back (`GetCanvasWidth/Height`, slave bounds) must call [`SDL_SyncWindow`](https://wiki.libsdl.org/SDL3/SDL_SyncWindow) (SDL 3.2.0+) — or set the global sync hint — or it will read stale geometry. This is a migration-correctness task, not a new feature.
	- **Filesystem.** Optionally adopt the `SDL3 Filesystem API` where it simplifies code, but `std::filesystem` (C++17) already covers the current file pipeline — opportunistic, not a headline item.
	- All new `SDL3` features: https://wiki.libsdl.org/SDL3/NewFeatures.
- **Zero-copy video decode.** Redo the hardware playback path so the final `VRAM` &rarr; `RAM` &rarr; `VRAM` readback is eliminated — keep decoded frames in VRAM and hand the D3D11VA output texture to the renderer via D3D11 interop. This is the headline backend win (see [video-pipeline.md](video-pipeline.md) *Known limitations*).
	- Leave the rebuilt pipeline **seek-capable** so the 2.1.0 fast-forward / rewind / restart commands can be added without re-architecting it.
- **Audio.** Adopt `SDL3 Audio Streams` (proper buffering + per-stream gain) and `SDL3 Logical Audio Devices`. Together these remove the close/reopen-device churn and the hand-rolled `audioBuffer` ring.
	- *Deliberate functionality change (confirmed in-scope for 2.0.0):* this enables **overlapping audio** — F-key SFX can play on top of video audio, which the one-device-at-a-time model cannot do today.
- Re-test the windowed-mode Slave Display bugs after the migration — SDL3's reworked window handling may resolve them outright; fix any that remain. *(moved from 1.8.0 — the bugs live in window/renderer creation code SDL3 rewrites, and windowed mode is a dev-only convenience)*
- Make it clear in the debug overlay whether hardware decode is active or it has fallen back to software decode.

### Decided against

Evaluated during the scoping review and dropped from 2.0.0:

- **`SDL3 GPU API`** — a new *low-level* API (command buffers, pipelines, hand-written shaders) with Vulkan/D3D12/Metal backends only — **no D3D11**. Adopting it means rewriting the whole render layer, and since hardware decode is D3D11VA (D3D11 textures), `SDL_GPU` cannot consume decoder output without an interop copy — it works *against* the zero-copy goal. The `SDL_Render` migration already delivers the multi-backend benefit.
- **`SDL3 Process API`** — no part of SatyrAV spawns child processes (FFmpeg is linked as libraries). Add later if a concrete need appears.
- **`SDL3 System Tray API`** — SatyrAV is a foreground operator tool; no roadmap feature needs a tray icon. It is a small, isolated feature that can be added in any release if wanted — it does not need to be coupled to the migration.
- **`SDL3 Time API` as a feature** — `<chrono>` (C++17) already provides calendar time. The in-project clock and per-scene stopwatch *command* are user features, moved to 2.1.0.

---

## 1.8 — shipped (five sub-versions)

**The 1.8 cycle is complete; 1.8.4 is the current release.** It shipped as five sub-versions ordered **easiest &rarr; hardest**, each landing and being verified on its own before the next began. Nothing in it depended on the 2.0.0 SDL3 migration — see the *Decided against* / move notes in the 2.0.0 and 2.1.0 sections for what was pulled out of this cycle along the way.

Next up is **2.0.0**, the SDL3 backend migration.

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

Most critical first. Bugs scheduled into a specific sub-version are listed there instead of here.

- Run doesn't work as intended in macro environment. 
- Scaling via the Debug menu will make the third output screen/monitor have less resolution when zooming in too much.
	- Essentially, making the secondary target display too small, will make the output for the third monitor more "pixelated". This, I think, is because of how the resolution is rendered from the second to the third screen. The optimal rendering would be that the third screen display the secondary screen "pre" target display, essentially the resolution of the secondary display.
- Maybe only one image can be shown at a time. This need to be tested.
- Not a bug, but we have to check what bit our movies is compatible with. 8/10 and 16 bit should all be suported.