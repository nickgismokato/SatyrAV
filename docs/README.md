# SatyrAV documentation

SatyrAV is an AV cue controller for SaTyR revyer (Danish student revue theater productions). It provides dual-display output with keyboard-first navigation through acts, scenes, and commands.

## Documentation index

- [Architecture](architecture.md) — system overview, module responsibilities, data flow
- [Scene file format](scene-format.md) — `.ngk` file syntax, commands, modifiers, loops
- [Command reference](commands.md) — every command, block, modifier, and option with full detail
- [Command reference — compact](commands-compact.md) — quick cheat-sheet of every name, no prose
- [Project structure](project-structure.md) — `schema.toml`, directory layout, options cascade
- [Video pipeline](video-pipeline.md) — how video playback works internally (FFmpeg, threading, sync)
- [Building](building.md) — compiling for Linux and cross-compiling for Windows
- [Controls](controls.md) — keyboard shortcuts and navigation
- [Changelog](CHANGELOG.md) — full version history
- [Roadmap](TODO.md) — planned features and known bugs

## Quick start

1. Build the project (see [building.md](building.md))
2. Launch `satyr-av`
3. Create a new project — choose a revy type, year, and your name
4. Edit scene files in `scenes/` using the `.ngk` format (see [scene-format.md](scene-format.md))
5. Load the project and navigate with arrow keys
6. Press `ENTER` to execute commands and advance through scenes

## License

MIT — see [LICENSE](../LICENSE) in the project root.
