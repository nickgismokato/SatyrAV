# Controls

Every key in SatyrAV. SatyrAV is run from the keyboard; the mouse is only for dragging popups.

**Contents:** [Everywhere](#everywhere) · [Menus](#menus) · [In a show](#in-a-show) · [Popups](#popups) · [Project scanner](#project-scanner)

## Everywhere

| Key | Action |
|---|---|
| H | Help: the keys for where you are |
| D | Debug popup — see [below](#the-debug-popup) |
| ESC | Back; in a show, leave the project (this ends the show) |
| ENTER | Select / confirm |
| TAB | Next field in a form |
| UP / DOWN | Move |

While you type in a text field, letters go into the field: `H`, `D`, `C` and the F-keys don't open anything.

## Menus

| Key | Action |
|---|---|
| UP / DOWN | Choose |
| ENTER | Open / confirm. On *Projects Directory* in Options: a folder dialog |
| ESC | Back to the main menu |

## In a show

### Moving around

The master has three columns — **akter**, **scenes**, **cues** — and, if the scene has `[Cue]` notes, the notes beside the cues.

| Key | Action |
|---|---|
| RIGHT / LEFT | Into the next column / back out |
| UP / DOWN | Move |
| SHIFT + UP / DOWN | Move 5 |
| CTRL + UP / DOWN | To the top / bottom |

### Running cues

| Key | Action |
|---|---|
| ENTER | Run the highlighted cue; the next one is highlighted |
| SHIFT + ENTER | Highlight the previous cue (undoes nothing) |

After the last cue of a scene the master says *Press ENTER for next scene*: ENTER moves to the next scene, ENTER again runs its first cue. See [structure.md](structure.md#running-a-show) for what a scene change does.

### Video and sound

| Key | Action |
|---|---|
| K | Pause / resume the video and its sound. The picture stays up while paused. |
| M | Pause / resume all other sounds (`play` sounds and F-keys). |
| `,` | Video 5 seconds back |
| `.` | Video 5 seconds forward. Less than 5 seconds left: the video ends. |
| SHIFT + `,` | Video from the start |

`K` and `M` are independent: `K` leaves the F-key sounds playing, `M` leaves the video's sound playing. The seek keys are the comma and period keys on Danish and US keyboards alike; a paused video shows the new spot at once and stays paused.

### Sound on the F-keys

| Key | Action |
|---|---|
| F1 – F12 | Play the sound set for that key; press again while it plays to stop it. |

The sounds are set in `schema.toml`:

```toml
[Hotkeys]
F1  = "knock.mp3"
F12 = "applause.mp3"
```

Sounds from different keys play over each other and over a video. Each key looks after its own sound only.

### Clearing the screen

| Key | Action |
|---|---|
| BACKSPACE | Clear the slave at once, like a `clearAll` cue: all text, pictures, particles, countdowns and animations. Video and sound play on — `K` / `M` for those. |

### Scene files

| Key | Action |
|---|---|
| U | Reload every scene file from disk without leaving the show — save in your editor, press `U`. |
| C | The [project scanner](#project-scanner): every mistake in the project. |

### Marking a line to fix later

| Key | Action |
|---|---|
| O | Note the **last cue that ran** in the project's fix-it log. The master shows `Marked Example.ngk line 42 in fix-it` for a moment. |

For a live show: when something looks wrong, press `O` straight after and carry on. After the show, `fix-it/ErrorInFiles-YYYY-MM-DD.LOG` in the project folder has one line per mark:

```txt
21:43:07  Example.ngk  line 42:  text "Helo, World!"
```

- One file per day, made by the day's first mark; a show with nothing to fix leaves no file.
- The line is shown as it was when the scene was loaded (or last reloaded with `U`).
- A `run{ … }` is one cue, so its mark points at the `run` line.

### Clock and stopwatches

With **Clock on Master** on in Options, the master shows the time of day, the time in this scene and the time since the show started. The stopwatches start with the first cue; the scene one restarts at each scene change. Never on the slave.

## Popups

The help and debug popups can be dragged by their title bar; the show goes on underneath. Press their key again to close.

### The debug popup

`D` shows what SatyrAV is doing right now: the akt and scene, the last cue with its file and whether the file exists, the slave and capture, fonts, the cache, and:

| Line | Meaning |
|---|---|
| `Capture:` | The capture output's size: `1:1` in green when it copies the slave pixel for pixel, the scaling in orange when not, `off` when there is none. |
| `Frames: N / M` | Video frames decoded ahead, of how many can be. |
| `Decode:` | How the video is decoded: `Hardware (zero-copy)` (green, the fast path on Windows), `Hardware (read-back)`, `Software` (orange; normal on Linux), or `no video`. |
| `Sounds: N` | Sounds playing right now (`play` sounds and F-keys). |
| `Backstage` | The last update to the backstage screen: green when it arrived, red with the reason when it didn't, grey when it's off. |

With the debug popup open:

| Keys | Action |
|---|---|
| D → S | **Targeted display editor**: move, size and turn the area SatyrAV draws into — `W` `A` `S` `D` move, `Z` / `X` size, `Q` / `E` turn, hold SHIFT for small steps, `C` back to the whole screen. ESC saves it to `schema.toml` and closes. |
| D → O | The project scanner (same as `C`). |

## Project scanner

`C` in a project (or `D` → `O`) checks the whole project for mistakes, so you find them before the audience does. It reads the files from disk, so edits made since the project was opened are checked without `U`. It only runs when the window opens and when you press `R`, so it costs nothing during the show.

| Key | Action |
|---|---|
| UP / DOWN | Scroll (SHIFT, PAGE UP / PAGE DOWN: 10 rows) |
| R | Scan again |
| C / ESC | Close |

Each finding has its file and line — errors in red, warnings in yellow — and is also written to SatyrAV's log. It finds:

- **In `schema.toml`:** a file that doesn't read; scenes listed without a `.ngk`; missing F-key sounds and fonts; unknown option names and values of the wrong kind (`FontSize = "25"` instead of `25`).
- **In each scene:** misspelled sections (`[commands]`) and lines outside any section; unknown options, bad colours, numbers and `true` / `false`, and options that only work in `schema.toml`; pictures, videos, sounds and fonts that don't exist — in cues, `[Preload]`, `[Animation]`, `randomImages` and `use` — and `[Preload]` patterns that match nothing; unknown commands; and mistakes in `[Animation]`, `[Backstage]` and `[Cue]`.
- **Scenes no akt uses:** a `.ngk` in `scenes/` that `schema.toml` doesn't list.
