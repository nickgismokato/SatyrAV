# Planned

What is planned for SatyrAV, newest version first. When a version ships, its items move to [implemented.md](implemented.md); known problems are in [known-bugs.md](../known-bugs.md).

Items are in no particular order. (**Critical**) marks the ones to look at first in that version, when the order of the work allows it.

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
- Package the VSCode extension cleanly: `vsce package` asks about a missing `LICENSE` and a missing `repository` field in `VSCodeExt/package.json`. Add a `LICENSE` to `VSCodeExt/` (the project's MIT licence) and `"repository": { "type": "git", "url": "https://github.com/nickgismokato/SatyrAV.git" }`, so it packages without prompts.
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
