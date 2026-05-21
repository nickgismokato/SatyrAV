# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

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
- (**Critical — moved from 1.6**) Transparent-layer handling for `.png` files. Today the alpha channel in a PNG is composited onto the slave background as normal. Add a per-project `[Options]` toggle (default **off**) that switches the slave to a transparent composite path — the underlying pixels show through PNG alpha instead of being filled by the scene background.
	- *Why deferred from 1.6:* SDL2 has no per-pixel-alpha window flag; `SDL_WINDOW_TRANSPARENT` is SDL3-only. A SDL2 implementation would require hand-rolling a D3D11 + DirectComposition swapchain to replace SDL2's slave renderer (and a separate ARGB-visual path on Linux/X11). After the 2.0.0 SDL3 migration this becomes a one-flag change on window creation, so it rides naturally with 2.1.
	- Still needs investigation of how this interacts with the targeted-display black-fill and with the capture-mirror window (capture cards drop alpha; may need a chroma-key or NDI-with-alpha output path).

---

## Planned for 2.0.0

**General notes about this major update:** This update will essentially be a big backend update, which means the functionality of the software shouldn't change. By upgrading to `SDL3` we will get a backend that can handle more complex backend tasks, be more optimised and also add in features we wants but is limited in `SDL2`. Like full hardware encoding/decoding. 

- Update `SDL2`&rarr;`SDL3`.
	- Utilize `SLD3 GPU API` to more efficiently utilize D3D11 and also add support for Vulkan and OpenGL. Furthermore, it is the hope that there is better support for integrated GPU's.
	- Utilize `SLD3 Filysystem API`. This will probably mainstream the file pipeline and also make it more maintainable. More research for this specific `API` is needed.
	- Utilize `SDL3 Audio Streams` to have a proper audio buffer and a gaining option for audio playback.
	- Utilize `SDL3 Logical Audio Devices` to handle audio devices.
	- Utilize `SDL3 Process API` to handle child processes simple and within the `SDL` environment.
	- Maybe utilize `SDL3 Async Windowing` to handle all other displayes better. Require `SDL3.2.0`.
		-An Async video is something we already have, essentially, but this will be a dedicated version which should remain thread safe if called on the main thread.
	- Utilize `SDL3 System Tray API` to create custom system and tray icons/menus.
	- Utilize `SDL3 Time API` to handle time different from ticks and and performance counters.
		- This means we can have a clock inside our project for easier tell of time. Also add a stopwatch command for scenes, to time individual scenes.
	- All new `SDL3` features can be found here: https://wiki.libsdl.org/SDL3/NewFeatures. Will have to investigate if more features is needed or some shouldn't be used at all.
- Redo hardware playback of videos for full hardware encoding.
	- Remove the copy of VRAM&rarr;RAM in the final step, making it essential zero copy when the GPU have gotten the data.
- Make it clear in the debug if Hardware encoding/decoding is enabled or if it has fallen back to Software encoding/decoding

---

## Planned for 1.8.0

- Make a hierarchy for pictures such that you can command what pictures should be in front and what shouldn't.
	- Currently the last displayed image will automatically be in front of all previous pictures.
- Make a fastforward-goback scroller for videos.
	- Probably should just go forward or backward with 5 seconds interval using key `,`/`.`.
	- A restart from the beginning function is also needed.
- Add `pathMove` command to dictate multiple movements of objects.
	- A option for repeating movement will also be added.
- (**Critical**) Add **UnitTesting** to more reliable testing of backend.
- Deal with the many bugs of the Windowed version of the `Slave Display`
- Create a better first time `example.ngk` when creating a new project
	- This should have two "akter".
	- This should have four scenes:
		1. One for text showcase
		2. One for picture showcase
		3. One for particle showcase
		4. One for group/macro showcase
- (**Critical**) Make sure all created `schema.toml` include all project options and also are at default value.
	- Remove some bloated comments
- Create a true folder chooser for folder picker inside option.
	- This means when pressing the folder choose for where projects should be, a folder "pop-up" should be opened and from here the user should be able to choose which folder they want to use for storage of their projects.
- In `docs/building.md` create a troubleshoot section for most common problems.
- (**Critical**) Add simple $\LaTeX$ symbols for Greek letters.
- (**Critical**) Add a fourth windows inside our main display when you have chosen a scene. This will be a `Cue Notes` screen.
	- It should essentially look like our command list but we cannot interact with it.
	- It should follow the command screen as we scroll up or down.
	- Inside it, there is specific notes to a specific command and it should always be on the right of the command.
	- A new `[Cue]` option should be given inside the `.ngk` files, where you can write notes. Here is an example:
	```txt
	[Cue]
	Cue1 Play audio when Actor leaves the scene
	Cue2 Stop audio when Actor begin screaming from outside the scene

	[Commands]
	show backgroundImage.png
	play Music.mp3 \Cue1
	stop \Cue2
	```
	- The Cue will have to makes "newlines" if the cue notes is to big.
- Add typewriter effect to write text to secondary display one character at a time.
	- Command: `typewrite(MS_PER_CHAR, "text")`

---

## 1.7.0 — shipped

Alternate quote delimiters (`"…"` / `'…'` / `´…´`) so text can contain quotation marks, plus a live `countdown` command with optional `en` / `da` language labels and a companion `clearCountdown`. See [CHANGELOG.md](CHANGELOG.md) for details.

---

## Planned for 1.6 (split into sub-versions)

The 1.6 cycle is large, so it ships as four sub-versions. Order is dependency- and risk-driven, not pure priority. PNG transparency (originally Critical for 1.6) has been moved to **2.1.0** because SDL2 has no per-pixel-alpha window path; see that section for the full rationale.

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