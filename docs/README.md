# SatyrAV documentation

Two kinds of documents live here: **how-to guides** for the people who write and run shows, and **developer references** for the people who change SatyrAV itself. Each fact is written once: a guide says what something does and how to use it, and links to the reference for the detail.

## For people writing and running shows — [`howto/`](howto/README.md)

| Guide | What's in it |
|---|---|
| [First time](howto/firstime.md) | From a new project to a running show, step by step |
| [Scene files](howto/scene.md) | Every command and section of a `.ngk`, in tables by what it does, with examples |
| [Controls](howto/controls.md) | Every key, the popups, the project scanner |
| [Projects](howto/project.md) | Folders, file names, `schema.toml`, akter and scenes |
| [Options](howto/options.md) | Every option in scenes, `schema.toml` and the Options screen |
| [How a show fits together](howto/structure.md) | How SatyrAV loads a show, what you're free to do, the show log and subtitles |

## For developers

| Reference | What's in it |
|---|---|
| [Architecture](dev/architecture.md) | Modules, data flow, threads |
| [Command reference](dev/commands.md) | Every command, block, modifier and section in full detail, and where each lives in the code |
| [Scene file format](dev/scene-format.md) | How the parser reads a `.ngk` |
| [Project structure](dev/project-structure.md) | `schema.toml`, `config.toml`, the options cascade, how projects are created and loaded |
| [Video pipeline](dev/video-pipeline.md) | Decoding, preload, sound, clock, seeking, threads |
| [Building](building.md) | Linux, the Windows cross-build, the installer, troubleshooting |
| [Tests](../tests/README.md) | The core suite and the off-screen render tests |
| [VSCode extension](../VSCodeExt/README.md) | Syntax highlighting for `.ngk` |

## Plans and history

- [plans/planned.md](plans/planned.md) — what is planned, by version
- [plans/implemented.md](plans/implemented.md) — what was planned and shipped
- [known-bugs.md](known-bugs.md) — known problems and things to check
- [CHANGELOG.md](CHANGELOG.md) — every release in detail

## Words used throughout

| Word | Means |
|---|---|
| **Master** | The operator's screen: the cue list, the notes, the popups. |
| **Slave** | The audience's screen (the projector). Everything a cue shows goes here. |
| **Capture** | An optional third output: a copy of the slave for the recording. |
| **Revy** | The production. A **project** is one revy in one year, e.g. `MatRevy_2026`. |
| **Akt** | An act: a list of scenes. A show has one or more akter. |
| **Scene** | One `.ngk` file: its options, and its cues in order. |
| **Cue** | One line under `[Commands]` — what happens when the operator presses ENTER. |
| **Targeted display** | The part of the slave screen SatyrAV draws into, set with `D` → `S`. Positions in percent are of this rect. |
