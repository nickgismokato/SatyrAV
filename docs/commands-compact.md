# Command reference — compact

Quick list of every command, block, modifier, and option in SatyrAV. For full explanations see [commands.md](commands.md).

## Sections

- `[Options]`
- `[Preload]`
- `[Macro]`
- `[Hotkeys]` *(1.6.3, project schema only)*
- `[Commands]`

## `[Options]` keys

- `bc = black | blue | red | green | white`
- `FontSize = <int>`
- `font = <path/to.ttf>` (or bare filename → `FONTS/` *(1.6.1)*)
- `fontItalic = <path>`, `fontBold = <path>`, `fontBoldItalic = <path>` *(1.6.1)*
- `cap = true | false`
- `<TYPE>.SPEED = <float>`
- `<TYPE>.DENS = <float>` (alias `<TYPE>.DENSITY`)
- `<TYPE>.X.DIST = NAME(p1, p2)`
- `<TYPE>.VX.DIST = NAME(p1, p2)`
- `<TYPE>.VY.DIST = NAME(p1, p2)`

### Project-level `[Options]` (in `schema.toml` only)

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
- `clear`
- `clear NAME`
- `clearText`
- `clearImages`
- `clearAll`
- `show FILE`
- `show MODIFIER(..., FILE)`
- `play FILE`
- `stop`
- `stop FILE`
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

- `#` — line or trailing comment
- `\n` inside `"..."` — new text entry
- Blank lines ignored
