# First time with SatyrAV

From nothing to a running show in about half an hour. You'll make a project, write a scene, try it on one screen, and run it the way you will on the night.

## 1. Set up the screens

Start SatyrAV and open **Options**:

- **Slave Window Mode: Windowed** — for now, so the audience screen (the *slave*) opens as a window next to the operator's screen (the *master*). On the show computer you'll switch it to *Fullscreen*.
- **Projects Directory** — where your projects go. ENTER opens a folder dialog.

Press **Save**.

## 2. Make a project

Main menu → **New project**: pick the revy (or *Other* and a name), the year, and your name. SatyrAV makes the project folder, with two akter and four example scenes, and opens it. Two windows now: the master with three columns (akter, scenes, cues) and the slave. Next time, open it from **Load project**.

## 3. Look at the examples

On the master: DOWN to pick the first akt and RIGHT to go into it, pick its first scene the same way and RIGHT again — now you're in its cues. Press **ENTER** again and again. Each press runs one cue on the slave. At the end of a scene ENTER takes you to the next one.

The examples show most of what SatyrAV can do. Open their files — `scenes/TextShowcase.ngk` and the rest, in the project folder — in a text editor to see how each cue is written. Press `H` any time for the keys.

## 4. Write your own scene

Make `scenes/Hello.ngk` in the project folder:

```txt
[Options]
BackgroundColour = black

[Commands]
text "Hello!"
text "This is my first scene"
clearText
text colour(yellow, "Now in yellow")
clear
```

Each line under `[Commands]` is one cue. Then add the scene to the show: open `schema.toml` and put `"Hello"` first in akt 1:

```toml
[Structure.Akt1]
schema = ["Hello", "TextShowcase", "PictureShowcase"]
```

Back in SatyrAV press **`U`**: the scene files are reloaded, and *Hello* is in the list. Walk into it and press ENTER through it.

## 5. Add a picture and a sound

Put `logo.png` in the project's `pictures/` folder and `ding.mp3` in `sound/`. Then change the scene:

```txt
[Preload]
logo.png

[Commands]
show logo.png
play ding.mp3 & text "Hello!"
clearText
text "The logo stays — clearText only removes text"
clear
```

`&` puts two things on one ENTER. `[Preload]` loads the picture before the scene starts, so it's there the instant the cue runs. Press `U` and try it.

## 6. Check for mistakes

Press **`C`**. The project scanner lists every mistake in every scene — a misspelled command, a picture that isn't there — with its file and line. Change `show logo.png` to `show logoo.png`, press `R` in the scanner, and see it found. Put it back.

## 7. Run it like the real thing

During a show the operator only needs a few keys:

| Key | |
|---|---|
| ENTER | Next cue |
| SHIFT + ENTER | Back one cue (undoes nothing) |
| BACKSPACE | Clear the slave now |
| K / M | Pause the video / the sounds |
| `,` / `.` | Video 5 s back / forward |
| F1 – F12 | Sound effects, set in `schema.toml` |
| O | Something went wrong: note this cue to fix later |
| ESC | Leave the project |

All keys: [controls.md](controls.md).

## 8. On the show computer

1. Copy the project folder to it. Fonts in the project's `fonts/` folder travel with it.
2. Options: **Slave Window Mode: Fullscreen**, **Secondary Display** = the projector, **Capture Mirror** = the recording output if there is one, **Audio Output** = the sound desk. Turn on **Show Log & Subtitles** if the show is recorded. Save.
3. Open the project, press `D` then `S`, and fit the drawing area to the projection. ESC saves it.
4. Press `C` one last time.

## Where next

- [Scene files](scene.md) — every command, by what it does, and ready-made examples for the common jobs.
- [Options](options.md) — fonts, colours, outline and the rest.
- [Projects](project.md) — folders, file names, `schema.toml`.
- [How a show fits together](structure.md) — what happens at a scene change, what's loaded when, and the show log and subtitles.
