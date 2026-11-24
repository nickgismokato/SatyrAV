# Scene file format (.ngk)

Scene files use a simple INI-like format made of sections (`[Options]`, `[Preload]`, `[Macro]`, `[Cue]`, `[Backstage]`, `[Commands]`). Lines starting with `#` are comments (end-of-line comments are also supported). A `#` is treated as literal data rather than a comment marker in three places: inside a quoted string, inside a modifier's parentheses — which is what makes `colour(#RRGGBB, …)` work — and as a hex colour straight after the `=` of an `[Options]` line, as in `TextColour = #FF8800`. See [commands.md](commands.md#line-endings-and-comments).

> How the parser reads a scene file, section by section, for people changing it. The complete list of every command, modifier and block is in [commands.md](commands.md); how to *use* them is in [howto/scene.md](../howto/scene.md).

## Sections

### [Options]

Per-scene settings that override project-level defaults. All are optional. Each option has the same name here as in the project's `schema.toml`, and case does not matter.

```ini
[Options]
BackgroundColour = black # Any colour name, #RRGGBB or #RGB. Unset = the project's (black by default)
TextColour = #FFD700     # Default text colour. Unset = the project's, then white
FontSize = 30            # Font size for slave display text
Font = path/to/font.ttf  # Regular face — relative path or bare FONTS/ filename FontItalic = DejaVuSans-Oblique.ttf         # Italic face for textit / it(...)
FontBold   = DejaVuSans-Bold.ttf            # Bold face for textbf / bold(...)
FontBoldItalic = DejaVuSans-BoldOblique.ttf # Combined face — used when a run is both bold + italic
Capitalize = true        # Capitalize all text output
AudioFade = true         # Let `play` fade times fade the sound too, not just the picture
FixedTextCentre = true   # Centre text on all its lines up to the next clear, so lines don't jump
TextOutline = 6          # Outline thickness for this scene, 0 = off
DisableTextOutline = true # No outline in this scene, whatever TextOutline says
TransparentBackground = true # See-through slave where nothing is drawn

# Per-particle-type tunables — scene overrides project defaults.
RAIN.SPEED   = 1.5                  # velocity multiplier
RAIN.DENS    = 2.0                  # spawn-rate multiplier
RAIN.X.DIST  = NORMAL(0.5, 0.1)     # cluster spawns around x=50%
# Velocity distributions — reshape sampling of vx/vy within each
# motion model's built-in range. UNIFORM is the default.
RAIN.VY.DIST = NORMAL(0.2, 0.1)     # slow-skewed fall speeds
SNOW.VX.DIST = NORMAL(0.5, 0.3)     # wider horizontal sway
```

See [commands.md](commands.md) for the full list of `<TYPE>.<KEY>` tunables and supported distributions (`UNIFORM`, `NORMAL`, `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`).
 A scene's `Font` settings apply to that scene only; the next scene goes back to the project's font unless it names its own. For `FixedTextCentre`, see [Fixed text centre](commands.md#fixed-text-centre).

### [Backstage]

What the backstage screen shows about this scene, when the project's `schema.toml` has `[Backstage]` with a `Url` (new projects do). At every scene change the current scene and the next two scenes *that have this block* are sent to the backstage ticker. Scenes without it are left off the backstage screen.

```ini
[Backstage]
Name         = Scene 1            # default: the scene's file name
ExpectedTime = 2m40s              # or 2:40, or 160 (seconds)
Actors       = ["Actor 1", "Actor 2"]
Ninjas       = ["Ninja 1"]
```

Full details — when it sends, what it sends, and how to see whether it arrived — in [commands.md](commands.md#backstage).

### [Preload]

List of file paths (relative to project root) to preload into the media cache. One path per line. Images are loaded as SDL textures. Audio and video files are registered for size tracking.

```ini
[Preload]
pictures/logo.png
sound/intro.mp3
movies/background.mkv
```
 Fonts used with `use` may be listed here too, but they do not need to be: every font and size a scene's text uses is opened when the scene is entered.

### [Animation]

Named animations that repeat, started with `show NAME` in `[Commands]`. See [commands.md](commands.md#animation) for every key and how they stop.

```ini
[Animation]
Tumbleweed{
    show     = spin(240, tumbleweed.png)   # any show expression
    path     = W, E          # left to right
    duration = 3000          # ms per pass
    repeat   = forever       # or a number
    every    = 4000          # ms between pass starts
    randomY  = 650, 950      # random height per pass, in slave pixels
}
```

### [Hotkeys] *(project schema only)*

Bindings for the `F1`–`F12` audio hotkeys. Lives in `schema.toml`, not in scene files. Filenames resolve against `sound/` (with the same fallback chain as `play`).

```toml
[Hotkeys]
F1  = "knock.mp3"        # short knock
F2  = "knock-loop.mp3"   # long looping knock — press F2 to start, F2 again to stop
F12 = "applause.mp3"
```

Press a key during a performance to play; press the **same** key again while its sound is still going to stop it (toggle behaviour, useful for long ambient loops). Different F-keys layer: their sounds play over each other and over a video's own sound. A sound is identified by its file, so the same file bound to two keys counts as one sound.

### [Cue]

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

A note that nothing references raises a parser warning (listed by the project scanner, `C`) — that is how a mistyped `\Name` surfaces, since an unrecognised reference is deliberately left alone so it can't collide with the `\alpha`-style [LaTeX symbols](commands.md#latex-symbols).

### [Macro]

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
text A + B               Styled concatenation — side-by-side runs          textCont "more"          Append to the previous text line                  textbf "message"         Bold text (uses FontBold face)                    textit "message"         Italic text (uses FontItalic face)                textbfCont "message"     Append, inheriting bold style                     textitCont "message"     Append, inheriting italic style                   textD "primary","sub"    Primary line + bottom-anchored italic translation countdown(SECONDS)       Live MM:SS countdown timer at top of slave       countdown(SEC, "LANG")   Same, with an "en" / "da" language label         typewrite(MS, "text")    Text revealed one character at a time            clear                    Clear ungrouped text, image, and particles (groups survive)
clear NAME               Drop every entry tagged with group NAME          clearText                Drop the text layer across every group           clearImages              Drop the image + particle layer across every group clearAll                 Full reset — text, images, particles, and groups clearCountdown           Remove every active countdown                    show image.png           Display an image (.png, .jpg, .bmp, .webp, .gif)
show anim.gif            Display an animated GIF — frames cycle at file-defined delays play file.mp3            Play audio file
play movie.mkv           Play video file (with embedded audio)
stop                     Stop all media playback and ungrouped particles
stop file.mp3            Stop one audio file; other sounds keep playing
stopParticleCont         Stop spawning ungrouped particles, let live ones finish stopParticleCont NAME    Same, scoped to a single group                      comment "note"           Non-executable navigator row                      ```

> **LaTeX symbols:** displayed text accepts LaTeX-style macros for Greek letters and common maths symbols — `text "pick \delta \in \mathbb{R}"` renders `pick δ ∈ ℝ`. Expansion happens at parse time and covers every text-bearing command, but never filenames or group names. Unrecognised macros pass through unchanged. See [commands.md](commands.md#latex-symbols) for the full list.

> **Quoting:** every quoted argument accepts three interchangeable delimiter pairs — `"..."`, `'...'`, and `´...´` (acute accent, distinct from the apostrophe). The opening delimiter must match the closing one; the other two then count as literal characters, so `text 'He said "hi"'` displays the double quotes verbatim. See [commands.md](commands.md#quoting-rules) for the full rules.

## Compound commands

Join multiple commands with `&` to execute them as a single cue:

```
clear & text "Next slide"
play music.mp3 & show background.png
```
 Only a `&` outside quotes and outside modifier parentheses separates commands, so an ampersand in displayed text is literal:

```
text "Tom & Jerry"                        # one command
clear & text colour(RED, "Salt & Pepper")  # one separator, one literal
```

## Modifiers

Modifiers wrap around command arguments and can be nested. They apply render properties to text or images.

### colour(COLOUR, content)

Set text colour. Named colours: red, green, blue, white, black, yellow, orange, purple, pink, cyan, lime, grey (or gray). Also supports hex: `#RRGGBB`, and the short `#RGB`. The same colours work in every colour option — see [Colours](commands.md#colours). Text without `colour(...)` uses the scene's `TextColour`, white unless set.

```
text colour(RED, "Warning!")
text colour(#FF5500, "Custom colour")
```


### trans(percentage, content)

Set transparency. 0 = fully opaque, 100 = invisible.

```
show trans(50, image.png)
```

### bold(content) / it(content)

Per-segment style for `text` runs. Compose with `colour`, `trans`, and each other in any order. Only meaningful inside text commands; ignored elsewhere.

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

### spin(degPerSec, content)

Continuous rotation. Composes with `rotate()` — `rotate` is the initial offset, `spin` is the dt-driven delta.

```
show spin(180, heart.png)
show rotate(45, spin(90, logo.png))
```

### move(FROM, TO, DURATION_MS, content)

Linear tween of position over time. `FROM` and `TO` are either `[x,y]` percent coords or a single-letter side sentinel (`N`/`S`/`E`/`W`) that spawns the content off-screen against that edge.

```
show move(N, [50, 50], 800, heart.png)        # flies in from above over 800 ms
show move([0, 0], [100, 100], 2000, logo.png) # diagonal sweep
```

### pathMove(P1, P2, ..., PN, DURATION_MS[, MODE], content)

Movement through two or more waypoints. Each waypoint uses `move()`'s vocabulary — `[x,y]` percent coords or a side letter. `DURATION_MS` is the time for the whole path, split evenly across the legs. `MODE` is `once` (default), `loop`, or `pingpong`.

```
show pathMove([0,50], [50,90], [100,50], 3000, logo.png)
show pathMove([10,10], [90,10], [90,90], [10,90], 4000, loop, logo.png)
text pathMove([20,50], [80,50], 2000, pingpong, "drifting")
```

### layer(N, content)

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

### scale(content, [width, height])

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
- `BROWNIAN` — random-walk drift, no gravity.
- `OSCILLATION` — sinusoidal X, linear Y descent.
- `ORBIT` — circular motion around the screen centre.
- `NOISE` — 2D value-noise flow field.
- `LIFECURVE` — linear motion with scale + alpha envelope.

```
show particle(RAIN, heart.png)
show particle(SNOW, 30, snowflake.png)
show particle(CONFETTI, 50, size([32, 32], star.png))
show particle(ORBIT, heart.png)
show particle(NOISE, trail(12, star.png))
```

### trail(N, content) / trail(N, MODE, content)

Keep the last N transforms of each particle in a ring buffer and render them behind the live particle. Samples are distance-gated (no stacking on slow particles) and aged in wall time. `MODE` selects the look:

- `GHOST` *(default)* — exponential alpha decay + age-shrink scale.
- `STRETCH` — single elongated quad per particle, rotated along the motion direction. Looks like real motion blur.
- `GLOW` — Ghost with additive blending (`SDL_BLENDMODE_ADD`). Best for bright sprites.

```
show particle(ORBIT, trail(20, heart.png))
show particle(NOISE, trail(12, STRETCH, star.png))
show particle(NOISE, trail(10, size([40,40], star.png)))
```

### trailGlow(N, content)

Sugar for `trail(N, GLOW, content)` — additive-blend trail for bloom-style effects.

```
show particle(NOISE, trailGlow(16, star.png))
```

### bounce(true|false, content)

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

### `+` concatenation

Inside `text`, `textCont`, `textbf`, `textit`, `textbfCont`, `textitCont`, and either side of `textD`, multiple styled segments can be joined on a single line with `+`. Each segment carries its own `colour()` / `trans()` / `bold()` / `it()`; line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the whole line, and when more than one segment sets the same one, the **leftmost** value wins.

```
text colour(RED, "Hello, ") + colour(BLUE, "World!")
text pos([20, 80], "Warning: ") + colour(RED, "engine overheating")
```

### `textCont`

Appends its argument to the last text entry instead of starting a new line. Useful for revealing a line piece by piece across multiple cues.

```
text "Hello, "
textCont "World!"
# slave shows: Hello, World!
```

## Blocks

### `run{ … }`

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

### `group NAME{ … }`

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

### `macro <Name>`

Inlines the body defined in `[Macro] Name{ … }` at this point. Unknown names are silently skipped.

```
macro HeartShower
```
