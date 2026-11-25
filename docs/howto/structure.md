# How a show fits together

How SatyrAV runs a show, what it loads when, how much room you have to build the show your way, and what it writes down while the show runs.

**Contents:** [Running a show](#running-a-show) · [What SatyrAV loads, and when](#what-satyrav-loads-and-when) · [Building the show your way](#building-the-show-your-way) · [Before the show](#before-the-show) · [The show log and subtitles](#the-show-log-and-subtitles) · [Other files SatyrAV writes](#other-files-satyrav-writes)

## Running a show

The master shows three columns — **akter**, **scenes**, **cues** — and, when a scene has `[Cue]` notes, the notes beside the cues. RIGHT goes into a column, LEFT back out, UP / DOWN move. In the cue column:

1. ENTER runs the highlighted cue, and the next one is highlighted.
2. After a scene's last cue, the master says *Press ENTER for next scene*. ENTER moves to the next scene (into the next akt after the last scene), with its first cue highlighted.
3. ENTER runs that first cue, and so on.

You can jump anywhere with the arrow keys and run any cue from there; SHIFT+ENTER moves the highlight back one cue without undoing anything. `comment` lines and `use` / `fontSize` lines are shown in the list but skipped. Every key is in [controls.md](controls.md).

**What a scene change does.** Moving on to the next scene stops the video and all sounds, cuts running animations, and removes ungrouped text, pictures, particles and countdowns. Grouped content (from `group NAME{…}`) stays until a `clear NAME` or `clearAll` — that is how something can stay up across scenes. Then the new scene's options are applied: its background, font, colours, outline.

**The show starts with the first cue.** Opening a project to look at it starts nothing. The first cue starts the show clock, the stopwatches and, if it's on, the show log. Leaving the project (ESC) ends the show.

## What SatyrAV loads, and when

| When | What |
|---|---|
| Opening the project | `schema.toml` and every scene file. A scene with mistakes still loads; the mistakes are left out and listed by the [scanner](controls.md#project-scanner). |
| Entering a scene | The scene's fonts. The pictures and GIFs in `[Preload]` of this scene and the scenes before and after it. The videos played in this scene and the next (and the one before), decoded ahead in the background. |
| Running a cue | Anything not loaded yet — a picture not in `[Preload]` is read from disk at that moment. |
| Pressing `U` | Every scene file again, from disk, without leaving the show. |

So: **list big pictures and GIFs in `[Preload]`** — they're then on screen the moment the cue runs. For a folder of pictures, one line does it: `MEGA/*.png` (that folder) or `MEGA/**/*.png` (and every folder below). Videos need nothing: SatyrAV finds every `play` of a video in the current and next scene and loads it ahead, so it starts at once. Sounds start immediately anyway.

Nothing in a cue waits on the network or on a slow disk: the backstage screen is told in the background, and the log is written line by line.

## Building the show your way

SatyrAV doesn't decide how a show is cut into scenes. Some ways that work well:

- **One scene per sketch or song.** The cue list stays short, the master shows the sketch's name, and the backstage screen can show who is on — give each such scene a `[Backstage]` block.
- **AV interludes as scenes of their own** — a video between sketches, a countdown for the break. Leave out `[Backstage]` and the backstage screen keeps showing the real lineup.
- **The same scene more than once** — a recurring jingle can be listed in several akter.
- **Content that spans scenes** goes in a `group`: a logo in the corner for a whole akt stays through scene changes until `clear NAME`.
- **Options where they belong.** Put the show's look — font, colours, outline — in `schema.toml` once; give a scene its own `[Options]` only where it differs.
- **Reuse inside a scene** with `[Macro]` blocks. A macro belongs to the scene file it's written in.
- **Shared or own fonts.** A font in the shared `FONTS` folder works in every project on that computer; a font in the project's `fonts/` folder travels with the project to another computer.
- **Translations** with `textD`, styled once for the whole show with the `Subtitle…` options.

Scene files are plain text: keep them in git, copy them between projects, write them in any editor — the [VSCode extension](../../VSCodeExt/README.md) colours them.

## Before the show

1. Press **`C`** — the [project scanner](controls.md#project-scanner) lists every mistake with its file and line. Fix until it's clean.
2. Rehearse on one screen with *Slave Window Mode: Windowed* in Options; switch back to *Fullscreen* for the show.
3. On the show computer: set the slave and capture screens in Options, then `D` → `S` to fit the [targeted display](../README.md#words-used-throughout) to the projection.
4. If the show is recorded, turn on **Show Log & Subtitles** in Options.

## The show log and subtitles

With **Show Log & Subtitles** on (Options; off by default), a show writes into the project's `showlog/` folder, named after the revy, the date and the run that day:

| File | What's in it |
|---|---|
| `MatRevy2026-2026-12-24-1.log` | Everything, with the time of day: the show, akter and scenes starting and ending (with their length), every cue, pictures and videos appearing and going (with how long they were up), and the operator's keys. |
| `…-1-Akt1.srt` | Per akt: the text the audience saw, as subtitles — plus one long cue per video and picture, from when it appeared to when it went. |
| `…-1-Akt1.ass` | Per akt: the same text with the font, size, bold and italic it was shown in. No video or picture cues. |

```txt
24/12-2026 | 19:30:12.453 | Akt 1 | Scene Intro | started act
24/12-2026 | 19:30:12.453 | Akt 1 | Scene Intro | started scene
24/12-2026 | 19:30:12.460 | Akt 1 | Scene Intro | play intro.mp4
24/12-2026 | 19:30:12.476 | Akt 1 | Scene Intro | on screen: [VIDEO] intro.mp4
24/12-2026 | 19:31:43.102 | Akt 1 | Scene Intro | off screen: [VIDEO] intro.mp4 (1:30)
```

**How the subtitles are made.** Every frame SatyrAV looks at what is on the slave. A subtitle starts when text appears and ends when it changes or is cleared, so lines that stack up become one subtitle that grows. Repeating animations and countdowns are left out. A video or picture gets one cue from the moment it appears to the moment it goes — handy for lining up the clip in the edit. These overlap the text subtitles.

**One pair per akt,** because each akt is edited as its own piece: the times in `…-Akt2.srt` count from the first cue of akt 2. Something still on screen when an akt ends is cut there and starts again at 0:00 in the next akt's file. An akt entered a second time in the same run gets its own pair (`-Akt1-2`).

**Using them.** Drop the `.srt` or `.ass` for an akt onto that akt's recording in the video editor and line up its start with the akt's first cue — the `.log` has the time of day of every cue, if the recording's clock is needed. The `.ass` keeps the fonts, which some editors and players use and others ignore.

**Safe in a crash.** The `.log` is written line by line, and the subtitle files are rewritten each time a subtitle ends, so only what was on screen at that moment is lost.

## Other files SatyrAV writes

| File | When |
|---|---|
| `fix-it/ErrorInFiles-YYYY-MM-DD.LOG` | Each time `O` is pressed: the file, line and text of the last cue, to fix after the show. See [controls.md](controls.md#marking-a-line-to-fix-later). |
| `log/satyrav-YYYY-MM-DD.log` in the [shared folder](project.md#shared-folders) | Always: SatyrAV's own log — what it loaded, what failed and why, the scanner's findings, backstage messages. |
