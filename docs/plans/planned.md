# Planned

What is planned for SatyrAV, newest version first. When a version ships, its items move to [implemented.md](implemented.md); known problems are in [known-bugs.md](../known-bugs.md).

Items are in no particular order. (**Critical**) marks the ones to look at first in that version, when the order of the work allows it.

---

## Planned for 3.0.0
A major version: a new operator UI, show control with a backup machine, and the groundwork for a new rendering engine. SatyrAV stays a show **player**. There is no in-app cue or scene editor, now or later. Shows are still written as `.ngk` files outside the program.

- **(Critical)** Full UI overhaul. The current UI works, but it is very text based.
    - Operator-facing only: performance screen, setup and status. No editing of scenes or cues.
    - **Performance screen.** A clear cue list with previous / current / next cue and what each one does (video, text, sound, image). Timers, the "GO" area and backstage status in one place.
    - **Status at a glance.** Preloaded media, RAM / VRAM use, FPS, decode mode (zero-copy / read-back / software). Can reuse the 2.2.0 performance log's measuring points.
    - **Setup screens.** Displays, capture output, audio device, hotkeys and project loading as proper forms, lists and dropdowns instead of text menus.
    - **Scanner results** as a clickable list showing file and line, so problems can be fixed in VSCode.
    - Look at a UI library (e.g. Dear ImGui on SDL3) instead of extending the hand-made widgets in `src/ui/`. It must stay light enough for old show machines.
- **(Critical)** Show control and redundancy.
    - **OSC in and out.** MagicQ first (main controller used by SaTyR), then QLab, maybe GrandMA. Fire cues, go to scene, stop, and send our own state back. *(If the OSC receiver lands in 2.3.0, 3.0.0 builds on it.)*
    - MIDI (notes / MSC) and, maybe, timecode (LTC / MTC) to follow a lighting desk or soundtrack.
    - **Backup machine.** A second PC loads the same project and follows every cue from the main machine over the network (same scene, same cue, video at the same position). If the main machine dies, switch the projector input, or let the backup take over by itself when the main stops sending a heartbeat. Needs: a clear "main" / "backup" mode in the UI, checking that both have the same project version (file hashes), and a log of every take-over.
    - **Web remote.** "GO", stop and status from a phone or tablet on the local network. Builds on the existing backstage HTTP code. Needs a simple PIN or pairing, so nobody else on the network can fire cues.
- **Rendering engine, only after measuring.** Do not start before the 2.2.0 performance logs from the SaTyR computer have been looked at. It must still run on weak machines.
    - Idea: keep on-screen things (text, images, video, particle groups) as objects between frames instead of redrawing everything from commands every frame. Text would be drawn once and reused, which should remove the current per-frame text cost.
    - The cost is memory, so everything gets **one memory budget**: images, cached text, animation frames and video. The least recently used items get dropped first, and nothing is stored larger than the output needs.
    - **Low-spec mode** (automatic from measurements, or set by hand): fewer particles, no trails, cheaper outline, lower-resolution caches.
    - Fits with the Vulkan move (see 2.3.0), the keep-last-frame option, PowerPoint-style transitions, and later several outputs (projector, side screens, stream) showing different layers.
    - Decide after measuring: is the problem CPU (text rasterising, read-back), GPU, or vsync waits across several windows? Fix the biggest one first. That may be enough without a full rewrite.
- **`.ngk` language features, on top of the line-by-line commands.**
    - The line-by-line commands stay the core and the documented normal way of writing a show. Nothing about them changes, and old files keep working without a converter.
    - New features are optional and live in clearly marked blocks: variables, the for-loop from 2.2.0, macros with arguments, maybe simple conditions.
    - **Rule: new features only expand into existing commands at parse time**, the way `loop(N){…}` and `macro NAME` already do in `SceneParser`. The cue engine never gets new behaviour from them, so there is still only one playback path to test.
    - **Show the expansion.** The scanner or debug view can show the plain commands a block turned into, so a user can always see what it really does.
    - **Tests.** For every feature, a test in `tests/test_scene_parser.cpp` checks that the expanded commands are exactly the same as the same scene written by hand in plain commands.
    - VSCode extension: highlighting for the new blocks. Maybe later a language server for errors and completion.

---

## Planned for 2.3.0
- include `SDL_camera.h` for direct camera input which is used sometimes.
- Add full Vulcan. No OpenGL support since it is essentially deprecated at this point for modern hardware.
- *(deferred from 2.1.0)* Add powerpoint animetions transitions for picture to picture. Essentially making it possible to run a slideshow like in powerpoint.
- *(deferred from 2.2.0)* Add messenger/other communication app default template to mimic communication.
- *(deferred from 2.1.0)* Create full physics model for particle systems, accelerated with hardware
    - While adding the physics models we should also look at how we could optimize it. Currently it lacks if to many "particels" is displayed. Especially on older hardware. `SIMD` comes to mind and also maybe using Broad-Phase Collision Detection.
- *(deferred from 2.1.0)* OSC Receiver, i.e. listen for OSC messages from lightning desk to control cues. Mainly interested in QLab, MagicQ (**main controller used by SaTyR**) and maybe GrandMA.
- Per-scene option to keep a video's last frame on screen when it ends, instead of cutting to the background.
    - **Current behaviour.** When a clip finishes, `MediaPlayer::UpdateVideoFrame` erases the playback at once, so the slave falls back to `BackgroundColour` (hard cut, black by default). A fade only happens when the `play` line asks for one (`play FILE FADE_IN, FADE_OUT`). This stays the default.
    - **New option.** `[Options] HoldLastFrame = true` in a scene (any case, like the other option keys; add it to `OptionKeys`, `SceneParser`, `ProjectScanner` and the VSCode grammar). Optionally a project-level default in `schema.toml`, like `AudioFade`.
    - **Behaviour with it on.** At end of clip, free the decoder, audio stream and ring, but keep the texture holding the last presented frame and keep drawing it until the next `play`, `clear`/`clearAll`, or scene change. A fade-out on the `play` line still wins (it ends on black, so nothing is held).
    - **Use case.** Cut one long video into parts and step through them with cues, so the picture continues instead of flashing to black between parts. With the next part in `[Preload]` the switch should be seamless.
    - **Watch out for.** In zero-copy mode the texture is created by `D3D11Interop::CopyFrameToTexture` and the frames reference decoder surfaces, so the held frame must be a texture we own, not a pinned decoder surface. Also check the paused/seek path and the master window's video preview.
---

## Planned for 2.2.0
- Add a **performance log**, so slow-downs on weak show machines can be measured instead of guessed. Its data decides the design of the 3.0.0 rendering engine.
    - **Toggle in Options.** New dropdown *Performance Log* (Off / On), saved in the config as `[perflog] enabled = false` next to `[showlog]`. Off by default. When off, the only cost is one `if` per measuring point: no timing, no files.
    - **Where.** `<project>/perflog/`, named like the show log: `<Name>-YYYY-MM-DD-N-perf.csv` (one row per second) and `<Name>-YYYY-MM-DD-N-perf.log` (header, events, spikes). Nothing is created until a show is started. Written from a buffer and flushed once a second, so logging itself doesn't cause stutter, and a crash loses at most a second.
    - **Header (once, in the `.log`).** SatyrAV version, OS, CPU model and core count, RAM, GPU name, driver version and VRAM, SDL renderer driver in use, every display's resolution and refresh rate, vsync on/off, capture window on/off, slave size and `[TargetedDisplay]`, project name, and the relevant config (zero-copy allowed, preload budget, audio device).
    - **Frame timing (per second in the `.csv`).** FPS and frame time min / average / max / 95th / 99th percentile, and how many frames went over 16.7 ms and over 33.3 ms. Split by stage, as average and max ms:
        - input + cue engine update;
        - slave draw: clear, video upload/present, images and animations, text, particles, countdowns;
        - master window draw;
        - capture: read-back from GPU, convert, upload;
        - present, measured per window (master, slave, capture), to show whether vsync waits stack up across windows.
    - **Text.** Text draw calls per second, how many times text was rasterised (`TTF_RenderText_Blended`), GPU textures created and destroyed, bytes uploaded, and how many of those were outline passes. This tests the "text is redrawn every frame" suspicion directly.
    - **Video.** Active clip, decode mode (zero-copy / hardware read-back / software), source and output size, frames buffered in the ring, frames skipped to catch up, decode thread time per frame (decode, `av_hwframe_transfer_data`, `sws_scale`), audio-clock watchdog events, and the time from `play` to first frame on screen.
    - **Particles and images.** Active particles, trail samples, particle draw calls and update time; images and animation frames on screen; total draw calls issued by SatyrAV per frame.
    - **Memory and CPU.** Process RAM (working set), preload bytes used against the budget, `MediaCache` size, our own estimate of texture memory, and on Windows the real VRAM use and budget from DXGI (`QueryVideoMemoryInfo`). Process CPU % and the decode thread's CPU time.
    - **Events (in the `.log`).** Scene started, cue fired (scene + cue number and command), video started/ended, preload started/finished, so spikes can be lined up with what happened.
    - **Spikes (in the `.log`).** Any frame over 2× the refresh interval gets its own line with the full stage breakdown and the last cue fired.
    - *(Nice to have)* Show the live numbers in the debug popup too, and a small script under `tools/` that turns a `.csv` into a summary (worst seconds, average per stage) or a graph.
- **(Critical)** Add a forloop for showing pictures.
    - A big problem is when you have more than a hundred images, it can be hard to write it all into an `.ngk` file. So a looping method could be useful.
    - One way of doing it could be like this:
        ```txt
        [Commands]
        for i in (1,100) do
            show picture.[i].png
        end do
        ```
        This would then do the same as writing `show picture1.png`, `show picture2.png`, ..., `show picture100.png`. But it would ease the workload of a show developer.
    - Currently I am thinking of using the `.[i].` notation to indicate where exactly the index should be. This syntax would make it clear where we are indexing, which mean we could potentially index pictures labeled `pic4ture.png` since we can just do `pic.[i].ture.png`. And most pictures doesn't actually use naming like `.[i].`, so we won't have a name clashing problem.
    - The index should be dynamically defined, meaning we are not forced to use the index `i` but could potentially use anything like `.[toilet].`.
    - The syntax `for i in (1,100) do` means mathematically "for $i\in[1,100]\subset\mathbb{Z}$ do". **Note** that our indexes can be negative. This practice should be looked heavily down upon, but since not all users are neither programmer nor mathematicians, this need to be done.
    - I am thinking maybe there could also be some internal functionality inside the loop that can be activated when the index is equal to a value. This would allow us to create stuff like full clears during the looping. 
- **(Critical)** Package the VSCode extension cleanly: `vsce package` asks about a missing `LICENSE` and a missing `repository` field in `VSCodeExt/package.json`. Add a `LICENSE` to `VSCodeExt/` (the project's MIT licence) and `"repository": { "type": "git", "url": "https://github.com/nickgismokato/SatyrAV.git" }`, so it packages without prompts.
- *(Nice to have)* Beginner-friendly install of the VSCode extension. The current copy-folder / `vsce` methods are too fiddly for most users.
    - **Release asset.** Attach `satyrav-ngk-<version>.vsix` to each GitHub release. Users install it in VSCode via Extensions view &rarr; **⋯** &rarr; **Install from VSIX…** (or drag the file onto the Extensions view). No Node or terminal needed on their side. Depends on the clean-packaging item above.
    - **GitHub Actions workflow** (optional). Run `vsce package` on release publish and upload the `.vsix` automatically, so packaging never depends on the local Node version.
    - **Windows installer checkbox** (optional). Add a component to `installer/satyr-av.nsi`, e.g. "Install VSCode syntax highlighting", that bundles the `.vsix` and runs `code --install-extension` if `code` is on `PATH` (skip quietly otherwise).
    - Update `VSCodeExt/README.md` so the `.vsix` route is the recommended install for end users.
    - *(Maybe later)* Publish to the VS Marketplace and/or Open VSX so users can search "SatyrAV" and get auto-updates. Needs a publisher account and access token, and `publisher` in `package.json` must match it.
- Add an application-window capture source (Windows only) so we can stream another running program onto the slave display. Video only — no audio capture.
	- **Backend.** Windows Graphics Capture (WGC) via `GraphicsCaptureItem::CreateFromHWND` &rarr; `Direct3D11CaptureFramePool` &rarr; `FrameArrived`, sharing the existing D3D11 device with SDL/FFmpeg so the captured BGRA texture stays on the GPU. Requires Win11 (no yellow capture border).
	- **Cue integration.** New `capture "<window-title>"` keyword in `SceneParser`, new `CommandType::Capture`, dispatched through `CueEngine` &rarr; `SlaveWindow`. Tear-down via a `stopCapture` command (or fold into existing clear semantics — decide during design).
	- **Slave draw layer.** New layer in `SlaveWindow` between the video layer and the images layer. **Must honour the `[TargetedDisplay]` rect** so the captured window can be moved / scaled / rotated like images via `D → S`.
	- **Window identity.** Case-insensitive **substring match on the window title** with a fallback message if not found. Easy for operators; revisit if it proves too fuzzy in practice.
	- **Picker UI.** New popup that enumerates top-level windows (`EnumWindows`, filtered to visible + non-empty title, excludes SatyrAV's own HWNDs) so the operator can pick at scene-edit time. The chosen title substring is written back into the `.ngk`.
	- **Phased landing:** (1) backend `WindowCaptureSource` class, verified against a debug texture; (2) `SlaveWindow` draw layer with rect support; (3) `SceneParser` / `CueEngine` wiring; (4) picker popup.
	- **Why deferred from 1.7:** WGC needs WinRT ABI headers (`<windows.graphics.capture.h>` full method tables, `<windows.graphics.directx.direct3d11.interop.h>`, working `__uuidof` on templated `ITypedEventHandler`, etc.). The Debian mingw-w64 13 sysroot used for the current cross-compile only ships partial forward-decls — every workaround we tried hit a new gap (no `<wrl/event.h>`, no `<wrl/implements.h>`, `__declspec(uuid)` silently ignored, missing `IDirect3D11CaptureFramePool` method tables, missing `WindowsCreateStringReference` decl). Lands cleanly once the toolchain catches up — naturally that's the SDL3 / D3D11 reshuffle in 2.0.0 + MSYS2 (or equivalent full Windows SDK) by 2.2.0.
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
        - If eta is missing we just write: "69m00"
        - If Actors is missing, we write no actor names and the same with nijas.
        - If for some reason our parser cannot read it, or don't understand it, these above values is also the default values for this case.
