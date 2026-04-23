# Command reference

Full reference for every command, modifier, and section available in a SatyrAV `.ngk` scene file. Every entry follows the same layout:

| Field | Meaning |
| --- | --- |
| **Syntax** | Exact syntax with the argument names in UPPER_CASE. |
| **Arguments** | Type and meaning of each argument. |
| **Behaviour** | What it does when executed. |
| **Output** | What the operator and audience see. |
| **Example** | A copy-pasteable example. |

Commands are grouped by role: scene-file sections, executable commands, modifiers, and structural blocks (`loop`, `run`, `group`, `macro`).

> **Versioning:** features marked *(1.2)*, *(1.3)*, *(1.4)*, *(1.4.1)*, and *(1.5)* indicate the version they were introduced. Everything unmarked was already in 1.0.

---

## Sections

### `[Options]`

Per-scene settings that override project-level defaults.

- **Syntax:** `[Options]` on its own line, followed by `key = value` pairs.
- **Keys:**
    - `bc` — background colour (`black`, `blue`, `red`, `green`, `white`). Default: black.
    - `FontSize` — slave font size in points. Overrides the project `FontSize`.
    - `font` — path (relative to project root) to a `.ttf` font file.
    - `cap` — `true` or `false` to capitalize every text line.
    - `<TYPE>.SPEED`, `<TYPE>.DENS` (or `.DENSITY`), `<TYPE>.X.DIST = NAME(p1, p2)` *(1.4)*, `<TYPE>.VX.DIST`, `<TYPE>.VY.DIST` *(1.5)* — per-particle-type tunables. See [Particle tunables in `[Options]`](#particle-tunables-in-options-14) for details.
- **Example:**
    ```txt
    [Options]
    bc = black
    FontSize = 30
    RAIN.SPEED  = 1.5
    RAIN.X.DIST = NORMAL(0.5, 0.1)
    ```

### `[Preload]`

Files to pre-decode/pre-cache before the scene runs, so the first `show` or `play` is instant.

- **Syntax:** `[Preload]` on its own line, one filename per line.
- **Arguments:** bare filename or relative path. The resolver searches the project root first, then `pictures/`, `movies/`, and `sound/` in that order.
- **Example:**
    ```txt
    [Preload]
    satyr-logo.png
    intro.mp3
    ```

### `[Macro]` *(1.3)*

Defines reusable command blocks that can be expanded anywhere in `[Commands]` via the `macro` command.

- **Syntax:**
    ```txt
    [Macro]
    MacroName{
        command1
        command2
        ...
    }
    ```
- **Behaviour:** At parse time, every `macro MacroName` inside `[Commands]` is replaced by the macro's body. No parameters, no nesting.
- **Example:**
    ```txt
    [Macro]
    Chorus{
        clear & text "Vær nu med på revyens sang"
        text "Vi skal have det for vildt"
        clear
    }

    [Commands]
    macro Chorus
    text "Verse 2 line"
    macro Chorus
    ```

### `[Commands]`

The sequence of cues. Each line is one cue; the operator presses ENTER to advance.

---

## Executable commands

### `text`

- **Syntax:** `text "MESSAGE"` — or `text SEGMENT + SEGMENT + ...` *(1.4)*
- **Arguments:** `MESSAGE` — quoted string. `\n` inside the string inserts a line break (each line becomes its own entry in the stack).
- **Behaviour:** Appends a text line onto the slave's text stack. Multiple `text` commands stack vertically, centred by default unless a `pos()` / `move()` modifier overrides. With the *(1.4)* `+` operator, multiple styled segments render side-by-side on a single line — each segment can wrap its own `color(...)` / `trans(...)` (see [the `+` operator](#-concatenation-14)).
- **Output:** The message appears on the slave screen with the current font, colour, and outline.
- **Example:**
    ```txt
    text "Welcome!"
    text "Line 1\nLine 2"
    text color(RED, "Hello, ") + color(BLUE, "World!")
    ```

### `textCont` *(1.4)*

- **Syntax:** `textCont "MESSAGE"` — supports the `+` operator identically to `text`.
- **Arguments:** `MESSAGE` — quoted string, optionally with `+` concatenation.
- **Behaviour:** Appends to the **last pushed text entry** instead of starting a new line. Intended for building up a line over multiple cues — e.g. `text "Hello, "` fires, operator presses ENTER, `textCont "World!"` fires, and the slave now shows `Hello, World!` on one line. If there is no prior entry (e.g. a `clear` came first), `textCont` falls back to pushing a new entry so the content still appears.
- **Interaction with `\n`:** A `\n` inside the appended message breaks out of the target line — everything after the `\n` starts a new text entry.
- **Example:**
    ```txt
    text "Hello, "
    textCont "World!"
    # Slave now shows: Hello, World!
    ```

### `clear`

- **Syntax:** `clear`
- **Arguments:** none — but see the variants below.
- **Behaviour:** Clears the slave's text stack, current image, and active particle system. Does **not** clear groups (see `clearAll`).
- **Output:** The slave goes back to its background colour (or video, if one is playing).
- **Example:**
    ```txt
    clear
    ```

### `clearText` *(1.3)*

- **Syntax:** `clearText`
- **Behaviour:** Removes only the text stack. Any current image, particles, or groups continue running.
- **Example:**
    ```txt
    clearText
    ```

### `clearImages` *(1.3)*

- **Syntax:** `clearImages`
- **Behaviour:** Removes the current image and stops the particle system. Text and groups are untouched.
- **Example:**
    ```txt
    clearImages
    ```

### `clear NAME` *(1.3)*

- **Syntax:** `clear NAME`
- **Arguments:** `NAME` — the identifier used in a preceding `group NAME{ ... }` block.
- **Behaviour:** Removes all content belonging to the named group. Ungrouped content (ordinary text, the active image) is untouched.
- **Example:**
    ```txt
    group SpinningHearts{
        show move(S, [25,25], 800, spin(180, heart.png))
        show move(N, [75,75], 800, spin(180, heart.png))
    }
    ...
    clear SpinningHearts
    ```

### `clearAll` *(1.3)*

- **Syntax:** `clearAll`
- **Behaviour:** Removes text, the current image, particles, **and every group**. Equivalent to a full reset of the slave content layer.
- **Example:**
    ```txt
    clearAll
    ```

### `show`

- **Syntax:** `show FILE` or `show MODIFIER(..., FILE)`
- **Arguments:** `FILE` — image filename (resolved against the project root, `pictures/`, etc.). Modifiers wrap around the filename.
- **Supported formats:** `.png`, `.jpg`/`.jpeg`, `.bmp`, `.webp`, and `.gif`. Animated `.gif` files cycle through their frames at the delays encoded in the file — see the note below. *(1.4)*
- **Behaviour:** Sets the current image layer. A new `show` replaces the previous image. `clear` / `clearImages` remove it.
- **Output:** The image is drawn into the targeted slave rect, fitted to target dimensions unless `size()` / `scale()` overrides.
- **Animated GIFs:** *(1.4)* Every frame is pre-decoded to a texture at preload / first-show time; the slave then advances the current frame by wall time. All position, rotation, spin, move, size and transparency modifiers apply to the whole animation. GIFs used as particles (`particle(..., anim.gif)`) currently render frame 0 only.
- **Example:**
    ```txt
    show satyr-logo.png
    show pos([50, 20], size([200, 200], icon.png))
    show dancing-satyr.gif
    ```

### `play`

- **Syntax:** `play FILE`
- **Arguments:** `FILE` — audio or video file. Extension determines which.
- **Behaviour:**
    - Video (`.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`) — opens the hardware-decoded video pipeline and renders frames into the slave beneath text.
    - Audio (`.mp3`, `.wav`, etc.) — plays in the background; multiple tracks can play simultaneously.
- **Example:**
    ```txt
    play intro.mp3
    play background.mp4
    ```

### `stop`

- **Syntax:** `stop` or `stop FILE`
- **Arguments:** Optional filename. With no argument, stops all playback and particle systems. With a filename, stops only that audio track.
- **Note:** *(1.4)* Particle systems that are already in smooth-stop mode (see `stopParticleCont`) are **not** affected by `stop`/`clear`/`clearAll` — their endings always play out to completion.
- **Example:**
    ```txt
    stop
    stop music.mp3
    ```

### `stopParticleCont` *(1.4)*

- **Syntax:** `stopParticleCont` or `stopParticleCont GROUP`
- **Arguments:** Optional group name. With no argument, applies to ungrouped particle systems; with a group name, applies to that group only.
- **Behaviour:** Stops new particles from spawning, but lets every currently-alive particle finish its animation naturally — the system deactivates itself once the last live particle dies. Once issued, the smooth-stop is sticky: subsequent `stop`, `clear`, `clear NAME`, or `clearAll` commands leave the ending alone.
- **Multiple ungrouped systems (1.5):** Ungrouped `show particle(...)` calls now coexist — starting a second one no longer replaces the first. `stopParticleCont` with no group drains every ungrouped system together; `clear` and `stop` still clear all ungrouped systems at once (smooth-stopping ones survive per the sticky rule above).
- **Example:**
    ```txt
    show particle(CONFETTI, 50, heart.png)
    # … cue advances …
    stopParticleCont           # confetti stops spawning, in-flight pieces finish falling
    text "Goodbye"             # ending plays under the goodbye line

    group Sparkles{
        run{ show particle(NOISE, trail(8, star.png)) }
    }
    stopParticleCont Sparkles  # only the Sparkles group enters smooth-stop
    ```

### `comment` *(1.2)*

- **Syntax:** `comment "TEXT"`
- **Behaviour:** Displays `# TEXT` in the master navigator as a dimmed row. **Not executable** and **not selectable** — navigation skips over it. Does not consume a line number in the gray index column.
- **Output:** Only the operator sees the comment; nothing is shown on the slave.
- **Example:**
    ```txt
    comment "Verse 1 — keep intensity low"
    text "Det er revytid"
    ```

### `macro NAME` *(1.3)*

- **Syntax:** `macro NAME`
- **Arguments:** `NAME` — identifier of a block defined under `[Macro]`.
- **Behaviour:** Expanded at parse time to the macro body's commands, inlined in place. If the macro is undefined, the line is skipped with no effect.
- **Example:**
    ```txt
    [Macro]
    Intro{
        play intro.mp3
        text "Hej Revy!"
    }

    [Commands]
    macro Intro
    ```

---

## Compound and block forms

### `&` (compound)

- **Syntax:** `cmd1 & cmd2 & cmd3 ...`
- **Behaviour:** Executes all parts as a single cue. Occupies one navigator row.
- **Example:**
    ```txt
    clear & text "Next slide" & play sting.mp3
    ```

### `run{ ... }` *(1.2)*

- **Syntax:**
    ```txt
    run{
        cmd1
        cmd2
        ...
    }
    ```
- **Behaviour:** Identical semantics to `&` — all inner commands execute atomically as one cue. Easier to read when the compound is long.
- **Example:**
    ```txt
    run{
        show background.png
        play intro.mp3
        text "Hej Revy!"
        text "Er I klar?!"
    }
    ```

### `loop(COUNT, CLEAR_EVERY){ ... }`

- **Syntax:**
    ```txt
    loop(COUNT, CLEAR_EVERY){
        commands...
    }
    ```
- **Arguments:**
    - `COUNT` — integer, how many times to repeat the body.
    - `CLEAR_EVERY` — integer, insert a `clear` after every N iterations (0 to disable).
- **Behaviour:** Expanded inline at parse time. Each iteration is a separate cue row.
- **Example:**
    ```txt
    loop(3, 1){
        text "Iteration!"
    }
    ```

### `group NAME{ ... }` *(1.3)*

- **Syntax:**
    ```txt
    group NAME{
        commands...
    }
    ```
- **Arguments:** `NAME` — identifier used later with `clear NAME`.
- **Behaviour:** Every command inside the body is parsed just like at the top level — plain lines, `run{}`, `loop(){}`, and `macro NAME` all work — and each one becomes its own navigator row. The group block adds *no* cueing behaviour; its sole effect is to tag every piece of slave content the body produces with the group name. Wrap the body in `run{}` (as in the example below) if you want everything to fire on a single key press. `clear` / `clearText` / `clearImages` leave groups alone; only `clear NAME` or `clearAll` remove them.
- **Example:**
    ```txt
    group SpinningHearts{
        run{
            show move(S, [25,25], 800, spin(180, heart.png))
            show move(N, [75,75], 800, spin(180, heart.png))
        }
    }
    text "Regular text that CAN be cleared by clearText"
    clearText
    # the hearts are still spinning
    clear SpinningHearts
    ```

---

## Modifiers

Modifiers wrap around command arguments. They can be nested freely. For `text` they affect colour/rotation/position/animation; for `show` they additionally affect size and particles.

### `color(COLOUR, content)`

- **Arguments:** named colour (`red`, `green`, `blue`, `white`, `black`, `yellow`, `orange`, `purple`, `pink`, `cyan`, `lime`, `grey`) or `#RRGGBB`.
- **Example:** `text color(RED, "Warning!")`

### `trans(PERCENT, content)`

- **Arguments:** `PERCENT` — 0 (opaque) … 100 (invisible).
- **Example:** `show trans(50, image.png)`

### `rotate(DEG, content)`

- **Arguments:** `DEG` — static rotation in degrees.
- **Example:** `show rotate(45, diamond.png)`

### `pos([X, Y], content)`

- **Arguments:** `X`, `Y` — 0–100, percent of the targeted-display width/height. Marks the top-left corner of the content.
- **Example:** `text pos([50, 10], "Top-centred ish")`

### `size([W, H], content)`

- **Arguments:** `W`, `H` — pixel dimensions.
- **Behaviour:** Overrides the default "fit inside the targeted rect" sizing for images.
- **Example:** `show size([200, 150], thumbnail.png)`

### `scale(content, [W, H])` *(1.3)*

- **Arguments:** `content` first, then `[W, H]` in pixels.
- **Behaviour:** Alias of `size()` with the argument order flipped to match `random()`. Both modifiers set the same underlying dimensions; use whichever order you prefer.
- **Example:** `show scale(logo.png, [400, 400])`

### `random(FILE, [MIN_W, MIN_H], [MAX_W, MAX_H], KEEP_ASPECT)`

- **Arguments:** `FILE` — image; min/max — pixel bounds; `KEEP_ASPECT` — `true` or `false`.
- **Behaviour:** Each particle spawn picks a random size in the given range.
- **Example:** `show particle(RAIN, 50, random(heart.png, [50,50], [200,200], true))`

### `particle(TYPE, [INTENSITY,] content)`

- **Arguments:** `TYPE` — `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN` *(1.4)*, `OSCILLATION` *(1.4)*, `ORBIT` *(1.4)*, `NOISE` *(1.4)*, `LIFECURVE` *(1.4)*; `INTENSITY` — 1-100 (default 100).
- **Behaviour:** Starts a particle system using the wrapped image (or randomised image via `randomImages()`). Motion model per type:
    - `RAIN` — near-vertical fall, slight X jitter.
    - `SNOW` — slower fall with sinusoidal sway and rotation.
    - `CONFETTI` — medium fall speed with strong rotation.
    - `RISE` — spawns at the bottom and rises.
    - `SCATTER` — radial burst from screen centre, fades with lifetime.
    - `BROWNIAN` *(1.4)* — spawns anywhere; velocity drifts via random-walk kicks with mild damping. No gravity. Lifetime-linked alpha fade.
    - `OSCILLATION` *(1.4)* — sinusoidal X around a per-particle anchor column, linear Y descent. Each particle has its own amplitude, frequency, and phase so the swarm doesn't move in lock-step.
    - `ORBIT` *(1.4)* — circular motion around the screen centre at a random radius and angular velocity per particle. Direction is random (cw/ccw).
    - `NOISE` *(1.4)* — velocity steered by a 2D value-noise flow field sampled at the particle's position; nearby particles follow similar currents.
    - `LIFECURVE` *(1.4)* — linear motion with a triangular scale/alpha envelope (grow + fade in, shrink + fade out).
- **Example:** `show particle(SNOW, 30, snowflake.png)`
- **Example (1.4):** `show particle(ORBIT, heart.png)`

### `trail(N, content)` / `trail(N, MODE, content)` *(1.4, MODE added 1.4.1)*

- **Arguments:**
  - `N` — integer count of past frames kept per particle (clamped to 0–64; 0 disables).
  - `MODE` *(optional, 1.4.1)* — `GHOST` (default), `STRETCH`, or `GLOW`.
- **Behaviour:** Each particle keeps a ring buffer of its last `N` transforms (position, rotation, scale, alpha) plus an `age` per sample. New samples are pushed only after the particle has moved a small distance, so slow particles don't stack copies on themselves. Samples expire when `age` exceeds `max(0.5, N * 0.08)` seconds.
  - `GHOST` — fade-and-shrink past samples behind the live particle (exponential alpha decay + linear age-shrink scale). Default.
  - `STRETCH` — single elongated quad per particle, rotated along the vector from the oldest sample to the live position. Reads as real motion blur. Best with `NOISE`, `OSCILLATION`, `ORBIT`.
  - `GLOW` — same geometry as `GHOST`, drawn with `SDL_BLENDMODE_ADD` for a bloom look. Bright sprites stack into a glow; dark sprites mostly disappear.
- **Example:** `show particle(NOISE, trail(12, star.png))`
- **Example:** `show particle(NOISE, trail(12, STRETCH, star.png))`
- **Example:** `show particle(ORBIT, trail(20, size([40,40], heart.png)))`

### `trailGlow(N, content)` *(1.4.1)*

- **Arguments:** `N` — same as `trail`.
- **Behaviour:** Sugar for `trail(N, GLOW, content)`. Past samples are drawn with additive blending so overlapping trails accumulate brightness. Live particle still draws with normal alpha blend.
- **Example:** `show particle(NOISE, trailGlow(16, star.png))`

### Particle tunables in `[Options]` *(1.4)*

The scene `[Options]` block accepts per-type tunables using the `<TYPE>.<KEY>` convention. Project-level defaults live under `[Particle.<TYPE>]` tables in `schema.toml`. The cascade is scene → project → built-in defaults, and only fields the author explicitly set override the level below.

**Keys:**
- `<TYPE>.SPEED = f` — scalar multiplier on spawn velocity (and on angular velocity / oscillation frequency for the formula-driven types). Default `1.0`, clamped to `[0.01, 20]`.
- `<TYPE>.DENS = f` (or `<TYPE>.DENSITY`) — scalar multiplier on spawn rate. `2.0` doubles the density. Default `1.0`, clamped to `[0.01, 20]`.
- `<TYPE>.X.DIST = NAME(p1, p2)` — distribution used when picking the spawn X for types that randomise X (`RAIN`, `SNOW`, `CONFETTI`, `RISE`, `BROWNIAN`, `OSCILLATION`, `NOISE`, `LIFECURVE`). Formula-positioned types (`ORBIT`, `SCATTER`) ignore it.
- `<TYPE>.VX.DIST = NAME(p1, p2)` *(1.5)*, `<TYPE>.VY.DIST = NAME(p1, p2)` *(1.5)* — distributions that reshape how each motion model samples its spawn velocity within its built-in `[vmin, vmax]` range. `NORMAL(0.5, 0.1)` on `RAIN.VY` clusters fall speeds near the range midpoint; the default `UNIFORM` reproduces 1.4 behaviour exactly. Formula-driven types (`ORBIT`, `NOISE`) ignore these; `SCATTER` derives vx/vy from a polar sample and also ignores them. Params are in `[0, 1]` (mapped into the range), matching `X.DIST`.

**Distributions:** `UNIFORM` (default), `NORMAL(mean, stddev)`, `LOGNORMAL(mu, sigma)`, `BINOMIAL(n, p)`, `BERNOULLI(p)`, `GAMMA(shape, scale)`. Parameters are in the normalised `[0, 1]` screen-fraction space — e.g. `NORMAL(0.5, 0.1)` clusters spawns around the horizontal centre with ~10 % screen stddev.

**Scene example:**
```ini
[Options]
RAIN.SPEED     = 1.5
RAIN.DENS      = 2.0
RAIN.X.DIST    = NORMAL(0.5, 0.1)
RAIN.VY.DIST   = NORMAL(0.2, 0.1)    # (1.5) slow-skewed rainfall
SNOW.VX.DIST   = NORMAL(0.5, 0.3)    # (1.5) wider sway
BROWNIAN.DENS  = 3.0
NOISE.SPEED    = 2.0
```

**Project `schema.toml` example:**
```toml
[Particle.RAIN]
Speed    = 1.25
Density  = 2.0
XDist    = "normal"
XDistP1  = 0.5
XDistP2  = 0.15
VYDist   = "normal"    # (1.5)
VYDistP1 = 0.2
VYDistP2 = 0.1
```

### `bounce(BOOL, content)` *(1.4)*

- **Arguments:** `BOOL` — `true` or `false` (or `1` / `0`).
- **Behaviour:** When `true`, each particle reflects off the four screen edges. The reflected velocity component loses 20 % energy per hit. Applied uniformly across every particle type, including formula-driven types like `ORBIT`/`OSCILLATION` — for those, bounce acts as a soft clamp.
- **Gravity + settling (1.5):** Enabling `bounce(true, …)` also switches on a constant downward gravity (≈ 900 px/s²) for every integrating type (`RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`, `OSCILLATION`, `LIFECURVE`). Particles accelerate, bounce off the floor, lose energy each hit, and once both |vx| and |vy| drop below the rest threshold they lock in place — forming a visible pile. Settled particles skip physics but still tick their lifetime, so the pile fades out as droplets expire naturally (use `clear` or `stopParticleCont` to tear it down). `ORBIT` and `NOISE` are formula-driven and gravity does not affect them.
- **Example:** `show particle(BROWNIAN, bounce(true, heart.png))`
- **Example (1.5):** `show particle(RAIN, 50, bounce(true, droplet.png))` — droplets accelerate, bounce off the floor, and pile up.

### `randomImages([LIST])` and `randomImages(FOLDER)`

- **Arguments:** a comma-separated list, or a folder name under `pictures/`.
- **Behaviour (1.3):** Each particle spawn picks a *new* random image from the list/folder, so a rain of `randomImages(professorPhotos)` drops many different photos, not the same one repeatedly.
- **Example:** `show particle(RAIN, 50, randomImages(professorPhotos))`

### `move(FROM, TO, DURATION_MS, content)` *(1.2)*

- **Arguments:**
    - `FROM` / `TO` — either `[X, Y]` percent coords or a side letter (`N`, `S`, `E`, `W`) meaning "just off that edge of the targeted rect".
    - `DURATION_MS` — milliseconds for the slide. Content holds at `TO` after that.
- **Behaviour:** Linear slide from `FROM` to `TO` over the duration, then static.
- **Example:** `show move(W, [50,50], 800, logo.png)`

### `spin(DEG_PER_SEC, content)` *(1.2)*

- **Arguments:** `DEG_PER_SEC` — positive = clockwise, negative = counter-clockwise.
- **Behaviour:** Continuously rotates the content. Composes additively with `rotate()` (the static angle is the initial offset).
- **Example:** `show spin(180, logo.png)`

### Nesting

Modifiers compose freely:

```txt
show pos([10, 10], trans(80, rotate(45, image.png)))
show move(N, [50,50], 800, spin(180, logo.png))
show particle(RAIN, 50, random(heart.png, [50,50], [200,200], true))
```

### `+` concatenation *(1.4)*

- **Syntax:** `text SEGMENT + SEGMENT + ...` (also works inside `textCont`).
- **Arguments:** each `SEGMENT` is any expression you would otherwise pass to `text` — a bare quoted string, or a modifier chain wrapping one.
- **Behaviour:** Splits the argument at top-level `+` tokens (respecting quotes, parens, and brackets), parses every segment independently, and renders them side-by-side on a single line.
    - **Per-segment modifiers:** `color(...)` and `trans(...)` apply only to their own segment — adjacent segments can therefore differ in colour or transparency.
    - **Line-level modifiers:** `pos(...)`, `move(...)`, `rotate(...)`, `spin(...)`, and `size(...)` act on the whole line. When two segments both try to set the same line-level modifier, **the leftmost wins** — the value from segments further right is silently ignored. Put line-level modifiers on the first segment when possible.
- **Example:**
    ```txt
    text color(RED, "Hello, ") + color(BLUE, "World!")
    text pos([20, 80], "Warning: ") + color(RED, "engine overheating")
    # pos from the first segment wins; a pos on the second segment would be ignored.
    ```

---

## Line endings and comments

- `\n` inside a quoted `text` string produces a new text entry.
- `#` starts a line/end-of-line comment — everything after it on the line is discarded.
- Blank lines are ignored.
