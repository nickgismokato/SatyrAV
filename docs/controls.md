# Controls

## General (all screens)

| Key | Action |
|-----|--------|
| H | Toggle help popup |
| D | Toggle debug popup |
| ESC | Go back / quit |
| ENTER | Select / confirm |
| TAB | Next field (in forms) |
| UP / DOWN | Navigate |

## In menus

| Key | Action |
|-----|--------|
| UP / DOWN | Select option |
| ENTER | Confirm selection |
| ESC | Return to main menu |

## In a project (Performance screen)

The navigator has three focusable columns — **Akt**, **Scene**, **Command** — plus, from *1.8.4*, a read-only **Cue Notes** column on the right. Cue Notes is not focusable and has no keys of its own: it scrolls with the command list and shows the `[Cue]` note attached to each row. It only appears when the selected scene defines notes, so scenes without them keep the full-width command column.

### Navigation

| Key | Action |
|-----|--------|
| RIGHT | Enter akt → scenes → commands |
| LEFT | Go back one column |
| UP / DOWN | Move selection |
| SHIFT+UP | Jump 5 items up |
| SHIFT+DOWN | Jump 5 items down |
| CTRL+UP | Jump to top |
| CTRL+DOWN | Jump to bottom |

### Execution

| Key | Action |
|-----|--------|
| ENTER | Execute current command |
| SHIFT+ENTER | Step back one command (cursor only — does not undo side effects from already-executed commands) |

### Scene transitions

At the last command in a scene, pressing ENTER executes the command. A status bar appears: `>> Press ENTER for next scene: SceneName >>`. Pressing ENTER again advances to the next scene where the first command is primed. Pressing ENTER a third time executes the first command of the new scene.

### Media control

| Key | Action |
|-----|--------|
| K | Toggle video pause. (1.6.0+) Pause freezes the last decoded frame on the slave instead of going black; press `K` again to resume from that frame. |
| M | Toggle audio/music pause |

> **Known issue in 2.0.0:** `K` also pauses any F-key sounds that are playing, and `M` also pauses a video's sound. The two were meant to be independent. See *Known bugs* in [TODO.md](TODO.md).

### Audio hotkeys (1.6.3+)

| Key | Action |
|-----|--------|
| F1–F12 | Project audio hotkeys. Each key plays an audio file declared in the project's `[Hotkeys]` table in `schema.toml`. Pressing the same key again while its sound is still playing **stops** it (toggle behaviour) — useful for long ambient cues like a knocking loop where the performer drives both start and stop with one key. *(2.0.0)* Different F-keys layer: their sounds play over each other and over a video's own sound. A sound is identified by its file, so the same file bound to two keys counts as one sound. F-keys are ignored while a text field is focused. |

Example `schema.toml` entry:

```toml
[Hotkeys]
F1  = "knock.mp3"
F2  = "doorbell.mp3"
F12 = "applause.mp3"
```

### Scene files

| Key | Action |
|-----|--------|
| U | Reload every `.ngk` scene file in the current project in place — no need to exit and reload the project |

Useful when iterating on scene content from an external editor: save the `.ngk`, switch back to SatyrAV, press `U`.

## Popups

The help and debug popups are draggable — click and drag the title bar. The program continues running underneath. Press H or D again to close, or press the same key to toggle.

(1.6.2+) Pressing `H` or `D` while a text field is focused (e.g. the Load Project search field, the New Project creator field) types the letter into the field instead of toggling the popup.

### What the Debug popup shows

The current akt and scene, the last command with its resolved file path and whether that file exists, the slave resolution and display, the font-size cascade, cache size, and for video:

| Line | Meaning |
|------|---------|
| `Frames: N / M` | How many decoded frames are buffered ahead, out of the buffer's capacity. |
| `Decode:` *(2.0.0)* | How the playing video is being decoded. `Hardware (zero-copy)` in green is the fast path on Windows. `Hardware (read-back)` in white is hardware decode through the slower route. `Software` in orange means the CPU is decoding, which is normal on Linux and a fall-back on Windows. `no video` when nothing is playing. |
| `Sounds: N` *(2.0.0)* | How many standalone sounds (F-keys and `play` audio) are playing right now. |

### Sub-popups from Debug (D)

While the Debug popup is open and a project is loaded:

| Chord | Action |
|-------|--------|
| D → S | Open the Display Rect editor — interactively move/scale/rotate the targeted-display rect with `WASD`/`Z`/`X`/`Q`/`E` (hold `SHIFT` for fine precision). `C` resets to default. `ESC` saves and closes. |
| D → O | (1.6.3+) Open the Overview Debug popup — walks every scene in the active project and lists parser warnings (unknown keywords) and missing media files referenced by `show` / `play` commands. `UP`/`DOWN` scrolls; `ESC` closes. Modal — input to the underlying screen is suppressed while open. |
