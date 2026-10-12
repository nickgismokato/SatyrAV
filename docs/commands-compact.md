# Command reference — compact

Quick list of every command, block, modifier, and option in SatyrAV. For full explanations see [commands.md](commands.md).

## Sections

- `[Options]`
- `[Preload]`
- `[Macro]`
- `[Cue]` *(1.8.4)* — `Name note text…` per line; a command claims one with a trailing `\Name`. Shown in the master window's Cue Notes column, never on the slave
- `[Hotkeys]` *(1.6.3, project schema only)*
- `[Commands]`

## `[Options]` keys

- `bc = black | blue | red | green | white`
- `FontSize = <int>`
- `font = <path/to.ttf>` (or bare filename → `FONTS/` *(1.6.1)*)
- `fontItalic = <path>`, `fontBold = <path>`, `fontBoldItalic = <path>` *(1.6.1)*
- `cap = true | false`
- `audioFade = true | false` *(2.0.0)* — `play` fade times also fade the video's sound
- `<TYPE>.SPEED = <float>`
- `<TYPE>.DENS = <float>` (alias `<TYPE>.DENSITY`)
- `<TYPE>.X.DIST = NAME(p1, p2)`
- `<TYPE>.VX.DIST = NAME(p1, p2)`
- `<TYPE>.VY.DIST = NAME(p1, p2)`

### Project-level `[Options]` (in `schema.toml` only)

- `FontSize`, `TextOutline`, `Capitalize`, `BackgroundColour` — project defaults for the scene keys above
- `AudioFade = true | false` *(2.0.0)* — project default for `audioFade`
- `Font`, `FontItalic`, `FontBold`, `FontBoldItalic` — paths or bare filenames *(1.6.1)*
- `SubtitleItalic = true | false` — default italic for `textD` subtitles *(1.6.1)*
- `SubtitleColour = "#RRGGBB" | <name>` — default `#AAAAAA` *(1.6.1)*
- `SubtitleTransparency = <0–100>` — percent, default 0 *(1.6.1)*
- `SubtitlePosY = <0–100>` — percent of canvas, default 90 *(1.6.1)*

## `[Hotkeys]` keys *(1.6.3, schema.toml only)*

- `F1 = "<file>"` … `F12 = "<file>"` — bind any subset of F-keys to audio files in `sound/`

## `schema.toml` particle keys — `[Particle.<TYPE>]`

- `Speed`, `Density`
- `XDist`, `XDistP1`, `XDistP2`
- `VXDist`, `VXDistP1`, `VXDistP2`
- `VYDist`, `VYDistP1`, `VYDistP2`

## Distributions

- `UNIFORM`
- `NORMAL(mean, stddev)`
- `LOGNORMAL(mu, sigma)`
- `BINOMIAL(n, p)`
- `BERNOULLI(p)`
- `GAMMA(shape, scale)`

## Commands

- `text "MSG"`
- `text SEG + SEG + ...`
- `textCont "MSG"`
- `textCont SEG + SEG + ...`
- `textbf "MSG"` *(1.6.1)*
- `textit "MSG"` *(1.6.1)*
- `textbfCont "MSG"` *(1.6.1)*
- `textitCont "MSG"` *(1.6.1)*
- `textD "PRIMARY", "SECONDARY"` *(1.6.1)* — secondary optional; comma-split, whitespace-tolerant
- `countdown(SECONDS)` *(1.7)* — live `MM:SS` timer, digits only
- `countdown(SECONDS, "LANG")` *(1.7)* — `LANG` = `en` | `da`; adds a label prefix
- `typewrite(MS_PER_CHAR, TEXT)` *(1.8.2)* — text revealed one character at a time; `TEXT` takes the full `text` grammar; `0` = instant; cue advance snaps it to complete
- LaTeX symbols *(1.8.2)* — `\alpha`…`\omega`, the 11 LaTeX uppercase, `\var*` forms, `\times \pm \leq \geq \neq \approx \sum \prod \int \in \subseteq \cap \cup \forall \exists \wedge \vee \emptyset \rightarrow \infty …`, and `\mathbb{X}` for any capital. Unknown macros pass through unchanged (that is what preserves `\n`)
- `clear`
- `clear NAME`
- `clearText`
- `clearImages`
- `clearAll`
- `clearCountdown` *(1.7)*
- `show FILE`
- `show MODIFIER(..., FILE)`
- `play FILE`
- `play FILE FADE_IN_MS, FADE_OUT_MS` *(1.6.6, video only)*
- `stop`
- `stop FILE` *(has no effect in 2.0.0 — known bug)*
- `stopParticleCont`
- `stopParticleCont GROUP`
- `comment "TEXT"`
- `macro NAME`

## Blocks

- `cmd1 & cmd2 & ...`
- `run{ ... }`
- `loop(COUNT, CLEAR_EVERY){ ... }`
- `group NAME{ ... }`

## Modifiers

- `color(COLOUR, content)`
- `trans(PERCENT, content)`
- `bold(content)` *(1.6.1, text only)*
- `it(content)` *(1.6.1, text only)*
- `rotate(DEG, content)`
- `pos([X, Y], content)`
- `size([W, H], content)`
- `scale(content, [W, H])`
- `random(FILE, [MIN_W, MIN_H], [MAX_W, MAX_H], KEEP_ASPECT)`
- `randomImages([LIST])`
- `randomImages(FOLDER)`
- `particle(TYPE, content)`
- `particle(TYPE, INTENSITY, content)`
- `trail(N, content)`
- `trail(N, MODE, content)`
- `trailGlow(N, content)`
- `bounce(BOOL, content)`
- `move(FROM, TO, DURATION_MS, content)`
- `pathMove(P1, P2, ..., PN, DURATION_MS[, MODE], content)` *(1.8.3)* — 2+ waypoints in `move()`'s vocabulary; `DURATION_MS` is the whole path split evenly per leg; `MODE` = `once` (default) | `loop` | `pingpong`
- `layer(N, content)` *(1.8.3)* — image draw order, higher is in front, default `0`, ties by show order; ignored on text
- `spin(DEG_PER_SEC, content)`

## Particle types

- `RAIN`
- `SNOW`
- `CONFETTI`
- `RISE`
- `SCATTER`
- `BROWNIAN`
- `OSCILLATION`
- `ORBIT`
- `NOISE`
- `LIFECURVE`

## Trail modes

- `GHOST` (default)
- `STRETCH`
- `GLOW`

## Colours

- `red`, `green`, `blue`, `white`, `black`, `yellow`, `orange`, `purple`, `pink`, `cyan`, `lime`, `grey`
- `#RRGGBB`

## Position tokens

- `[X, Y]` — percent of targeted rect
- `N`, `S`, `E`, `W` — off-edge spawn (for `move`)

## Syntax

- `#` — line or trailing comment (ignored inside quoted strings *(1.6.6)*)
- `"..."`, `'...'`, `´...´` — interchangeable string delimiters; opener must match closer *(1.7)*
- `\n` inside a quoted string — new text entry
- Blank lines ignored
