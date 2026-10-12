# Scene file format (.ngk)

Scene files use a simple INI-like format with four sections. Lines starting with `#` are comments (end-of-line comments are also supported). A `#` is treated as literal data rather than a comment marker in two places: inside a quoted string *(1.6.6)*, and inside a modifier's parentheses *(1.8.2)* — which is what makes `color(#RRGGBB, …)` work. See [commands.md](commands.md#line-endings-and-comments).

> **Canonical reference:** This document is a tutorial introduction to the format. For the complete and authoritative list of every command, modifier, and block — with syntax and a runnable example per entry — see [commands.md](commands.md).

## Sections

### [Options]

Per-scene settings that override project-level defaults. All are optional.

```ini
[Options]
bc = black          # Background colour: black, blue, red, green, white
FontSize = 30       # Font size for slave display text
font = path/to/font.ttf  # Regular face — relative path or bare FONTS/ filename (1.6.1)
fontItalic = DejaVuSans-Oblique.ttf       # (1.6.1) Italic face for textit / it(...)
fontBold   = DejaVuSans-Bold.ttf          # (1.6.1) Bold face for textbf / bold(...)
fontBoldItalic = DejaVuSans-BoldOblique.ttf # (1.6.1) Combined face — used when a run is both bold + italic
cap = true          # Capitalize all text output
audioFade = true    # (2.0.0) Let `play` fade times fade the sound too, not just the picture

# (1.4) Per-particle-type tunables — scene overrides project defaults.
RAIN.SPEED   = 1.5                  # velocity multiplier
RAIN.DENS    = 2.0                  # spawn-rate multiplier
RAIN.X.DIST  = NORMAL(0.5, 0.1)     # cluster spawns around x=50%
# (1.5) Velocity distributions — reshape sampling of vx/vy within each
# motion model's built-in range. UNIFORM (default) reproduces 1.4.
RAIN.VY.DIST = NORMAL(0.2, 0.1)     # slow-skewed fall speeds
SNOW.VX.DIST = NORMAL(0.5, 0.3)     # wider horizontal sway
```

See [commands.md](commands.md) for the full list of `<TYPE>.<KEY>` tunables and supported distributions (`UNIFORM`, `NORMAL`, `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`).

### [Preload]

List of file paths (relative to project root) to preload into the media cache. One path per line. Images are loaded as SDL textures. Audio and video files are registered for size tracking.

```ini
[Preload]
pictures/logo.png
sound/intro.mp3
movies/background.mkv
```

### [Hotkeys] *(1.6.3, project schema only)*

Bindings for the `F1`–`F12` audio hotkeys. Lives in `schema.toml`, not in scene files. Filenames resolve against `sound/` (with the same fallback chain as `play`).

```toml
[Hotkeys]
F1  = "knock.mp3"        # short knock
F2  = "knock-loop.mp3"   # long looping knock — press F2 to start, F2 again to stop
F12 = "applause.mp3"
```

Press a key during a performance to play; press the **same** key again while its sound is still going to stop it (toggle behaviour, useful for long ambient loops). *(2.0.0)* Different F-keys layer: their sounds play over each other and over a video's own sound. A sound is identified by its file, so the same file bound to two keys counts as one sound.

### [Cue] *(1.8.4)*

Named notes for the operator, shown in the master window's **Cue Notes** column beside the command that references them. Nothing is displayed on the slave — these are stage-management notes, not audience-facing text.

Each line is `Name` then the note text. A command claims a note with a trailing `\Name`.

```ini
[Cue]
Cue1 Play audio when Actor leaves the scene
Cue2 Stop audio when Actor begin screaming from outside the scene

[Commands]
show backgroundImage.png
play Music.mp3 \Cue1
stop \Cue2
```

Names may use letters, digits and underscores. `[Cue]` can sit before or after `[Commands]`. Long notes wrap onto as many rows as they need, and the command's row grows to match so the two stay aligned.

A note that nothing references raises a parser warning (visible in the Overview Debug popup, `D → O`) — that is how a mistyped `\Name` surfaces, since an unrecognised reference is deliberately left alone so it can't collide with the `\alpha`-style [LaTeX symbols](commands.md#latex-symbols-182).

### [Macro] *(1.3)*

Named, reusable blocks of commands. Inlined at parse time when referenced from `[Commands]` with `macro <Name>`. No parameters, no nesting.

```ini
[Macro]
HeartShower{
    show particle(RAIN, randomImages([heart.png, star.png]))
    text "Congratulations!"
}

[Commands]
macro HeartShower
```

### [Commands]

Each line is one cue. The operator presses ENTER to advance through commands sequentially.

## Commands

```
text "message"           Display text on the slave screen
text "line 1\nline 2"    Multi-line text (literal \n in the string)
text A + B               Styled concatenation — side-by-side runs          (1.4)
textCont "more"          Append to the previous text line                  (1.4)
textbf "message"         Bold text (uses fontBold face)                    (1.6.1)
textit "message"         Italic text (uses fontItalic face)                (1.6.1)
textbfCont "message"     Append, inheriting bold style                     (1.6.1)
textitCont "message"     Append, inheriting italic style                   (1.6.1)
textD "primary","sub"    Primary line + bottom-anchored italic translation (1.6.1)
countdown(SECONDS)       Live MM:SS countdown timer at top of slave       (1.7)
countdown(SEC, "LANG")   Same, with an "en" / "da" language label         (1.7)
typewrite(MS, "text")    Text revealed one character at a time            (1.8.2)
clear                    Clear ungrouped text, image, and particles (groups survive)
clear NAME               Drop every entry tagged with group NAME          (1.3)
clearText                Drop the text layer across every group           (1.3)
clearImages              Drop the image + particle layer across every group (1.3)
clearAll                 Full reset — text, images, particles, and groups (1.3)
clearCountdown           Remove every active countdown                    (1.7)
show image.png           Display an image (.png, .jpg, .bmp, .webp, .gif)
show anim.gif            Display an animated GIF — frames cycle at file-defined delays (1.4)
play file.mp3            Play audio file
play movie.mkv           Play video file (with embedded audio)
stop                     Stop all media playback and ungrouped particles
stop file.mp3            Stop one audio file (has no effect in 2.0.0 — known bug)
stopParticleCont         Stop spawning ungrouped particles, let live ones finish (1.4)
stopParticleCont NAME    Same, scoped to a single group                      (1.4)
comment "note"           Non-executable navigator row                      (1.2)
```

> **LaTeX symbols *(1.8.2)*:** displayed text accepts LaTeX-style macros for Greek letters and common maths symbols — `text "pick \delta \in \mathbb{R}"` renders `pick δ ∈ ℝ`. Expansion happens at parse time and covers every text-bearing command, but never filenames or group names. Unrecognised macros pass through unchanged. See [commands.md](commands.md#latex-symbols-182) for the full list.

> **Quoting *(1.7)*:** every quoted argument accepts three interchangeable delimiter pairs — `"..."`, `'...'`, and `´...´` (acute accent, distinct from the apostrophe). The opening delimiter must match the closing one; the other two then count as literal characters, so `text 'He said "hi"'` displays the double quotes verbatim. See [commands.md](commands.md#quoting-rules-17) for the full rules.

## Compound commands

Join multiple commands with `&` to execute them as a single cue:

```
clear & text "Next slide"
play music.mp3 & show background.png
```

*(1.8.2)* Only a `&` outside quotes and outside modifier parentheses separates commands, so an ampersand in displayed text is literal:

```
text "Tom & Jerry"                        # one command
clear & text color(RED, "Salt & Pepper")  # one separator, one literal
```

## Modifiers

Modifiers wrap around command arguments and can be nested. They apply render properties to text or images.

### color(COLOUR, content)

Set text colour. Named colours: red, green, blue, white, black, yellow, orange, purple, pink, cyan, lime, grey. Also supports hex: `#RRGGBB`.

```
text color(RED, "Warning!")
text color(#FF5500, "Custom colour")
```

> The hex form only started working in **1.8.2** — before that the comment stripper cut the cue at the `#`. Scenes written against older versions that worked around it with a named colour keep working unchanged.

### trans(percentage, content)

Set transparency. 0 = fully opaque, 100 = invisible.

```
show trans(50, image.png)
```

### bold(content) / it(content) *(1.6.1)*

Per-segment style for `text` runs. Compose with `color`, `trans`, and each other in any order. Only meaningful inside text commands; ignored elsewhere.

```
text "Plain " + bold("BANG!") + " then plain"
text "He whispered " + it("very softly")
text bold(it("Bold AND italic"))
```

Line-level keywords (`textbf` / `textit`) set a default style on every run; per-segment `bold(...)` / `it(...)` OR onto that default. So `textbf "A " + it("B")` renders A bold and B bold-italic.

### rotate(degrees, content)

Rotate content by a static angle.

```
show rotate(45, diamond.png)
```

### spin(degPerSec, content) *(1.2)*

Continuous rotation. Composes with `rotate()` — `rotate` is the initial offset, `spin` is the dt-driven delta.

```
show spin(180, heart.png)
show rotate(45, spin(90, logo.png))
```

### move(FROM, TO, DURATION_MS, content) *(1.2)*

Linear tween of position over time. `FROM` and `TO` are either `[x,y]` percent coords or a single-letter side sentinel (`N`/`S`/`E`/`W`) that spawns the content off-screen against that edge.

```
show move(N, [50, 50], 800, heart.png)        # flies in from above over 800 ms
show move([0, 0], [100, 100], 2000, logo.png) # diagonal sweep
```

### pathMove(P1, P2, ..., PN, DURATION_MS[, MODE], content) *(1.8.3)*

Movement through two or more waypoints. Each waypoint uses `move()`'s vocabulary — `[x,y]` percent coords or a side letter. `DURATION_MS` is the time for the whole path, split evenly across the legs. `MODE` is `once` (default), `loop`, or `pingpong`.

```
show pathMove([0,50], [50,90], [100,50], 3000, logo.png)
show pathMove([10,10], [90,10], [90,90], [10,90], 4000, loop, logo.png)
text pathMove([20,50], [80,50], 2000, pingpong, "drifting")
```

### layer(N, content) *(1.8.3)*

Draw order for images — higher is nearer the front, default `0`, ties broken by show order. Negative values sit behind unlayered images. Ignored on text.

```
show layer(-1, backdrop.png)
show layer(5, foreground.png)
```

### pos([x, y], content)

Position content at a specific point on the slave display. Coordinates are in a 0-100 grid (percentage of screen width and height).

```
text pos([10, 20], "Top-left area")
show pos([50, 50], centered.png)
```

### size([width, height], content)

Override the display size of an image in pixels.

```
show size([200, 150], thumbnail.png)
```

### scale(content, [width, height]) *(1.3)*

Alias of `size()` with content-first argument order, matching `random()` and the other content-first modifiers.

```
show scale(logo.png, [400, 300])
```

### random(file, [minW, minH], [maxW, maxH], keepAspect)

Display an image at a random size between the given bounds. `keepAspect` is `true` or `false`.

```
show random(heart.png, [50, 50], [200, 200], true)
```

### particle(TYPE, [intensity,] content)

Start a particle effect using the given image. Intensity is 1-100 (default 100).

Types:
- `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER` — classic gravity / burst models.
- `BROWNIAN` *(1.4)* — random-walk drift, no gravity.
- `OSCILLATION` *(1.4)* — sinusoidal X, linear Y descent.
- `ORBIT` *(1.4)* — circular motion around the screen centre.
- `NOISE` *(1.4)* — 2D value-noise flow field.
- `LIFECURVE` *(1.4)* — linear motion with scale + alpha envelope.

```
show particle(RAIN, heart.png)
show particle(SNOW, 30, snowflake.png)
show particle(CONFETTI, 50, size([32, 32], star.png))
show particle(ORBIT, heart.png)
show particle(NOISE, trail(12, star.png))
```

### trail(N, content) / trail(N, MODE, content) *(1.4, MODE added 1.4.1)*

Keep the last N transforms of each particle in a ring buffer and render them behind the live particle. Samples are distance-gated (no stacking on slow particles) and aged in wall time. `MODE` selects the look:

- `GHOST` *(default)* — exponential alpha decay + age-shrink scale.
- `STRETCH` — single elongated quad per particle, rotated along the motion direction. Looks like real motion blur.
- `GLOW` — Ghost with additive blending (`SDL_BLENDMODE_ADD`). Best for bright sprites.

```
show particle(ORBIT, trail(20, heart.png))
show particle(NOISE, trail(12, STRETCH, star.png))
show particle(NOISE, trail(10, size([40,40], star.png)))
```

### trailGlow(N, content) *(1.4.1)*

Sugar for `trail(N, GLOW, content)` — additive-blend trail for bloom-style effects.

```
show particle(NOISE, trailGlow(16, star.png))
```

### bounce(true|false, content) *(1.4)*

Reflect particles off the four screen edges with an 0.8 energy-loss coefficient on the reflected velocity component.

```
show particle(BROWNIAN, bounce(true, heart.png))
show particle(NOISE, bounce(true, trail(8, star.png)))
```

### randomImages([list] or folder)

Choose a random image from a list or folder. When combined with `particle()`, each spawn picks a fresh random image (so a confetti rain of mixed professor faces, for example, keeps mixing rather than locking in on one).

```
show particle(RAIN, 50, randomImages([heart.png, star.png, face.png]))
show particle(RAIN, 50, randomImages(professorPhotos))
```

The folder form loads all files from `pictures/professorPhotos/`.

### Nesting

Modifiers can be nested in any order:

```
show pos([10, 10], trans(80, rotate(45, image.png)))
show particle(RAIN, 33, random(heart.png, [50, 50], [200, 200], true))
show move(S, [25, 25], 800, spin(180, heart.png))
```

### `+` concatenation *(1.4)*

Inside `text`, `textCont`, `textbf`, `textit`, `textbfCont`, `textitCont`, and either side of `textD`, multiple styled segments can be joined on a single line with `+`. Each segment carries its own `color()` / `trans()` / `bold()` / `it()`; line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the whole line, and when more than one segment sets the same one, the **leftmost** value wins.

```
text color(RED, "Hello, ") + color(BLUE, "World!")
text pos([20, 80], "Warning: ") + color(RED, "engine overheating")
```

### `textCont` *(1.4)*

Appends its argument to the last text entry instead of starting a new line. Useful for revealing a line piece by piece across multiple cues.

```
text "Hello, "
textCont "World!"
# slave shows: Hello, World!
```

## Blocks

### `run{ … }` *(1.2)*

Multi-line equivalent of the `&` compound operator. One cue, one navigator row, every sub-command fires at once.

```
run{
    clear
    show background.png
    text "Welcome"
    play theme.mp3
}
```

### `loop(count[, clearEvery]){ … }`

Repeat a block of commands. `clearEvery` inserts a `clear` after every N iterations. Expanded into inline commands at parse time — each iteration becomes its own navigator row.

```
loop(3, 1){
    text "Slide content"
    text "More content"
}
```

### `group NAME{ … }` *(1.3)*

Tag every command in the body with a group name. The group block adds no cueing behaviour — each inner command still becomes its own navigator row. Wrap the body in `run{}` if you want everything to fire on a single key press. Every piece of slave content the body produces carries the group name and can later be dropped with `clear NAME`.

```
group SpinningHearts{
    run{
        show move(S, [25, 25], 800, spin(180, heart.png))
        show move(N, [75, 75], 800, spin(180, heart.png))
    }
}
text "Regular text — clearable with clearText"
clearText
# the hearts are still spinning
clear SpinningHearts
```

### `macro <Name>` *(1.3)*

Inlines the body defined in `[Macro] Name{ … }` at this point. Unknown names are silently skipped.

```
macro HeartShower
```
