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
| K | Toggle video pause — the picture and the video's own sound. (1.6.0+) Pause freezes the last decoded frame on the slave instead of going black; press `K` again to resume from that frame. |
| M | Toggle pause of the standalone sounds — F-key hotkeys and `play` audio files |

*(2.0.1)* The two are independent: `K` leaves F-key sounds playing, and `M` leaves a video's sound playing. In 2.0.0 each also paused the other's sound.

### Clearing the screen (2.1.0+)

| Key | Action |
|-----|--------|
| BACKSPACE | Clear the slave, the same as a `clearAll` cue: all text, images, particles and countdowns, grouped or not. Video and sound keep playing — use `K` / `M` for those. Works at any time, without a cue for it. |

### Audio hotkeys (1.6.3+)

| Key | Action |
|-----|--------|
| F1–F12 | Project audio hotkeys. Each key plays an audio file declared in the project's `[Hotkeys]` table in `schema.toml`. Pressing the same key again while its sound is still playing **stops** it (toggle behaviour) — useful for long ambient cues like a knocking loop where the performer drives both start and stop with one key. *(2.0.0)* Different F-keys layer: their sounds play over each other and over a video's own sound. A sound is identified by its file, so the same file bound to two keys counts as one sound. *(2.1.0)* Each key looks at **its own** sound: F1, F2, F1 stops F1's sound, and a key whose sound has finished plays it again on the first press, even while a video's sound is playing. Starting an F-key sound while `M` has paused sound un-pauses every sound. F-keys are ignored while a text field is focused. |

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

### Marking a line to fix later (2.0.1+)

| Key | Action |
|-----|--------|
| O | Mark the **last executed cue** in the project's fix-it log. The master shows `Marked Example.ngk line 42 in fix-it` for a few seconds. |

Meant for use during a live show: when something on the slave looks wrong, press `O` straight after the cue and carry on. After the show, open the log to find every line that needs attention.

- The log is `fix-it/ErrorInFiles-YYYY-MM-DD.LOG` inside the project folder, one line per mark:
    ```txt
    21:43:07  Example.ngk  line 42:  text "Helo, World!"
    ```
  The code is the line exactly as written in the scene file, comment included, as it was when the scene was loaded (or last reloaded with `U`).
- One file per day. Marks from several runs on the same day go into the same file.
- The `fix-it` folder is created when a project is opened; the day's file is only created by the first mark of the day, so a show with nothing to fix leaves no file.
- A `run{ … }` block fires as one cue, so its mark points at the `run` line.
- Pressing `O` before any cue has run shows `Nothing to mark yet` and writes nothing.
- With the Debug popup open, `O` opens the Overview popup instead (see below).

### Show time log (2.1.0+)

Nothing to press: while a show runs, SatyrAV writes down when everything happened, so the recording can be lined up with the show afterwards.

- The show starts with the **first cue** after the project is opened. Opening a project just to look at it writes nothing.
- Each run writes two files in `showlog/` inside the project folder, named after the revy and the date, with a run number: `MatRevy2026-2026-12-24-1.log` and `.srt`. Opening the project again the same day gives `-2`, `-3`, …
- The `.log` has one line per event:
    ```txt
    24/12-2026 | 19:30:12.126 | Started MatRevy2026
    24/12-2026 | 19:30:12.453 | Akt 1 | Scene BandIntro | started scene
    24/12-2026 | 19:32:59.599 | Akt 1 | Scene BandIntro | ended scene (2:47)
    24/12-2026 | 19:32:59.599 | Akt 1 | Scene TeXIntro | started scene
    24/12-2026 | 19:32:59.634 | Akt 1 | Scene TeXIntro | play TeXIntro.mp4
    24/12-2026 | 19:33:40.002 | Akt 1 | Scene TeXIntro | F3: play applause.mp3
    ```
- Logged: scene starts and ends (with how long the scene took), every cue that runs (each part of a `&` compound on its own line), and the keys `BACKSPACE`, `F1`–`F12`, `K`, `M`, `O` and `U`. Keys pressed before the first cue are not logged.
- The `.srt` holds the same events as subtitles, timed from the start of the show, to drop onto the recording in a video editor. Each subtitle lasts until the next event, at most 5 seconds.
- Leaving the project (ESC) ends the show and closes both files. The `.log` is written line by line, so a crash loses nothing; the `.srt` may miss its last subtitle.

### Clock and stopwatches (2.1.0+)

With **Clock on Master** set to `On` in Options (off by default), the master shows, top right: the time of day, the time spent in the current scene, and the time since the show started. Both stopwatches start with the first cue, and the scene one restarts at every scene change. They are on the master only, never on the slave.

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
| `Backstage` *(2.0.2)* | The last update sent to the backstage screen: green `sent <scene> Ns ago: HTTP 200, … ms` when it arrived, red `FAILED …` with the reason when it did not, grey `off` when the project has no `[Backstage] Url` or has `Enabled = false`. See [`[Backstage]`](commands.md#backstage-202). |

### Sub-popups from Debug (D)

While the Debug popup is open and a project is loaded:

| Chord | Action |
|-------|--------|
| D → S | Open the Display Rect editor — interactively move/scale/rotate the targeted-display rect with `WASD`/`Z`/`X`/`Q`/`E` (hold `SHIFT` for fine precision). `C` resets to default. `ESC` saves and closes. |
| D → O | (1.6.3+) Open the Overview Debug popup — walks every scene in the active project and lists parser warnings (unknown keywords) and missing media files referenced by `show` / `play` commands. `UP`/`DOWN` scrolls; `ESC` closes. Modal — input to the underlying screen is suppressed while open. |
