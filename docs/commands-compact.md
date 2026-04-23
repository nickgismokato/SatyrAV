# Command reference — compact

Quick list of every command, block, modifier, and option in SatyrAV. For full explanations see [commands.md](commands.md).

## Sections

- `[Options]`
- `[Preload]`
- `[Macro]`
- `[Commands]`

## `[Options]` keys

- `bc = black | blue | red | green | white`
- `FontSize = <int>`
- `font = <path/to.ttf>`
- `cap = true | false`
- `<TYPE>.SPEED = <float>`
- `<TYPE>.DENS = <float>` (alias `<TYPE>.DENSITY`)
- `<TYPE>.X.DIST = NAME(p1, p2)`
- `<TYPE>.VX.DIST = NAME(p1, p2)`
- `<TYPE>.VY.DIST = NAME(p1, p2)`

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
