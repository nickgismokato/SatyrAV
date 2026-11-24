# SatyrAV

SatyrAV runs the screens of a SaTyR revy. The operator steps through the show one cue at a time on the **master** screen; the audience sees text, pictures, video and effects on the **slave** screen (the projector), and a **capture** output can feed the recording.

**Current version:** see [VERSION](VERSION) · [changelog](docs/CHANGELOG.md)

## What it does

**A show is a list of cues.** A project is split into akter and scenes. Each scene is a small text file (`.ngk`) with one cue per line: show this text, show that picture, play this video, start the confetti. During the show the operator presses ENTER for the next cue; the master shows where you are, what comes next, and notes for the crew beside the cues.

**Everything the slave can show.** Text in any font, colour, size, bold and italic, with translations underneath (`textD`) and typewriter reveals. Pictures and animated GIFs that can be placed, sized, turned, faded, moved along paths and layered. Video with fades, which can be paused and wound back or forward during the show. Sound effects on the F-keys. Particle effects — rain, snow, confetti and more — from any picture. Repeating animations, countdowns, and a see-through slave that lets another program show underneath.

**Made for a live show.** Nothing waits on disk or the network during a cue: media is loaded ahead, video is decoded on the graphics card where it can be. `BACKSPACE` clears the slave at any moment. `O` notes a line that went wrong so it can be fixed after the show, and `U` reloads the scene files without leaving the show.

**Around the show.** The backstage screen is told who is on stage next at every scene change. A show log records everything with its time, and per-act subtitle files (`.srt` / `.ass`) of the text on screen can go straight onto the recording. The project scanner (`C`) finds mistakes in the scene files before the audience does.

## Quick start

1. Install SatyrAV (Windows installer), or build it — see [building.md](docs/building.md).
2. Start it, choose **New project**, pick the revy and year.
3. Open the project: it comes with four example scenes that show what the scene files can do.
4. Read [the first-time guide](docs/howto/firstime.md) — it walks you from an empty project to a running show.

## Where to find things

| I want to… | Read |
|---|---|
| Get started, run my first show | [howto/firstime.md](docs/howto/firstime.md) |
| Write scenes — every command, by what it does | [howto/scene.md](docs/howto/scene.md) |
| Know the keys during a show | [howto/controls.md](docs/howto/controls.md) |
| Set up a project: folders, files, `schema.toml` | [howto/project.md](docs/howto/project.md) |
| Change options (fonts, colours, outline, …) | [howto/options.md](docs/howto/options.md) |
| Understand how a show is built and loaded, logs and subtitles | [howto/structure.md](docs/howto/structure.md) |
| Build SatyrAV from source | [building.md](docs/building.md) |
| Change the code: architecture, full references | [docs/dev/](docs/README.md#for-developers) |
| See what's planned, what's done, what's broken | [planned](docs/plans/planned.md) · [implemented](docs/plans/implemented.md) · [known bugs](docs/known-bugs.md) |

All documentation starts at [docs/README.md](docs/README.md). Scene files get syntax highlighting in VSCode with the extension in [VSCodeExt/](VSCodeExt/README.md).

## Building

Debian 13 has everything packaged:

```bash
sudo apt install cmake g++ libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
cmake -S . -B build && cmake --build build -j$(nproc)
```

Other Linux versions, the Windows cross-build, the installer and the tests are in [docs/building.md](docs/building.md).

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
