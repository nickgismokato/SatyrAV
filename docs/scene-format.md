# Scene file format (.ngk)

Scene files use a simple INI-like format with four sections. Lines starting with `#` are comments (end-of-line comments are also supported).

> **Canonical reference:** This document is a tutorial introduction to the format. For the complete and authoritative list of every command, modifier, and block — with syntax and a runnable example per entry — see [commands.md](commands.md).

## Sections

### [Options]

Per-scene settings that override project-level defaults. All are optional.

```ini
[Options]
bc = black          # Background colour: black, blue, red, green, white
FontSize = 30       # Font size for slave display text
font = path/to/font.ttf  # Custom font file
cap = true          # Capitalize all text output

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
clear                    Clear ungrouped text, image, and particles (groups survive)
clear NAME               Drop every entry tagged with group NAME          (1.3)
clearText                Drop the text layer across every group           (1.3)
clearImages              Drop the image + particle layer across every group (1.3)
clearAll                 Full reset — text, images, particles, and groups (1.3)
show image.png           Display an image (.png, .jpg, .bmp, .webp, .gif)
show anim.gif            Display an animated GIF — frames cycle at file-defined delays (1.4)
play file.mp3            Play audio file
play movie.mkv           Play video file (with embedded audio)
stop                     Stop all media playback and ungrouped particles
stop file.mp3            Stop specific audio file
stopParticleCont         Stop spawning ungrouped particles, let live ones finish (1.4)
stopParticleCont NAME    Same, scoped to a single group                      (1.4)
comment "note"           Non-executable navigator row                      (1.2)
```

## Compound commands

Join multiple commands with `&` to execute them as a single cue:

```
clear & text "Next slide"
play music.mp3 & show background.png
```

## Modifiers

Modifiers wrap around command arguments and can be nested. They apply render properties to text or images.

### color(COLOUR, content)

Set text colour. Named colours: red, green, blue, white, black, yellow, orange, purple, pink, cyan, lime, grey. Also supports hex: `#RRGGBB`.

```
text color(RED, "Warning!")
text color(#FF5500, "Custom colour")
```

### trans(percentage, content)

Set transparency. 0 = fully opaque, 100 = invisible.

```
show trans(50, image.png)
```

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

Inside `text` and `textCont`, multiple styled segments can be joined on a single line with `+`. Each segment carries its own `color()` / `trans()`; line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the whole line, and when more than one segment sets the same one, the **leftmost** value wins.

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
