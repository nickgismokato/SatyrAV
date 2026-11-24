# Scene files (`.ngk`)

A scene file is the script for one scene: its options, and the cues the operator steps through with ENTER. This page has every command in tables by what it does, how the screen is cleared, and short examples for the common jobs. Every command name links to its full entry in the [command reference](../dev/commands.md), which has every argument and edge case.

**Contents:** [At a glance](#at-a-glance) · [Sections](#sections) · [Writing cues](#writing-cues) · [Text](#text) · [Fonts](#fonts) · [Pictures](#pictures) · [Video and sound](#video-and-sound) · [Movement and animation](#movement-and-animation) · [Particles](#particles) · [Timers](#timers) · [Clearing the screen](#clearing-the-screen) · [Running several things at once](#running-several-things-at-once) · [Modifiers](#modifiers) · [Colours and positions](#colours-and-positions) · [Common patterns](#common-patterns) · [When something is wrong](#when-something-is-wrong)

## At a glance

```txt
[Options]                       # how this scene looks (all optional)
BackgroundColour = black
TextColour       = white

[Preload]                       # load these before the scene starts
bg.png

[Backstage]                     # what the backstage screen says about this scene
Name         = Opening number
ExpectedTime = 3m30s
Actors       = ["Anna", "Bo"]

[Cue]                           # notes for the operator, never on the slave
Lights  Wait for the lights to go down

[Commands]                      # the cues: one line = one ENTER
show bg.png \Lights
text "Welcome to the revy!"
clearText
text "Act one"
```

Only `[Commands]` is needed. The sections can come in any order.

## Sections

| Section | What it's for |
|---|---|
| [`[Options]`](../dev/commands.md#options) | How the scene looks — background, text colour, font, outline, … Overrides the project's options for this scene only. See [options.md](options.md). |
| [`[Preload]`](../dev/commands.md#preload) | Files to load before the scene runs, so the first `show` / `play` is instant. One file per line. |
| [`[Commands]`](../dev/commands.md#commands) | The cues, one per line, in show order. |
| [`[Macro]`](../dev/commands.md#macro) | Blocks of cues with a name, used with `macro NAME`. See [Running several things at once](#running-several-things-at-once). |
| [`[Animation]`](../dev/commands.md#animation) | Named animations that repeat, started with `show NAME`. See [Movement and animation](#movement-and-animation). |
| [`[Cue]`](../dev/commands.md#cue) | Notes for the operator: `Name text…` per line. A cue ending in `\Name` shows that note beside it on the master. |
| [`[Backstage]`](../dev/commands.md#backstage) | `Name`, `ExpectedTime`, `Actors`, `Ninjas` — sent to the backstage screen when the show reaches this scene. |

## Writing cues

- **One line, one cue.** Each line under `[Commands]` is what happens on one ENTER.
- **Comments:** `#` and everything after it is ignored — except inside quotes, inside a modifier's parentheses (`colour(#FF8800, …)`), and as a colour after `=` in `[Options]`.
- **Quotes:** `"…"`, `'…'` and `´…´` all work; use the one your text doesn't contain: `text 'She said "no".'`
- **New line in text:** `\n` — `text "Line one\nLine two"`.
- **Maths and Greek:** `\alpha`, `\pi`, `\leq`, `\infty`, `\mathbb{R}` and more become the real symbols — see [LaTeX symbols](../dev/commands.md#latex-symbols).
- **Files** are found in the project folder, then `pictures/`, `movies/` and `sound/`, so `show logo.png` finds `pictures/logo.png`.
- **Several things on one ENTER:** join them with `&` — `clear & text "Act two" & play gong.mp3`.

## Text

Text lines stack in the middle of the slave, newest at the bottom, until cleared.

| Command | What it does |
|---|---|
| [`text "…"`](../dev/commands.md#text) | Adds a line of text. |
| [`text A + B + …`](../dev/commands.md#-concatenation) | One line made of pieces, each with its own colour, style or transparency: `text "Hej " + colour(RED, "med dig")`. |
| [`textCont "…"`](../dev/commands.md#textcont) | Adds to the end of the last line instead of starting a new one. |
| [`textbf "…"`, `textit "…"`](../dev/commands.md#textbf--textit) | A line in bold / italic. Inside a line, use `bold(…)` and `it(…)`. |
| [`textbfCont`, `textitCont`](../dev/commands.md#textbfcont--textitcont) | `textCont` in bold / italic. |
| [`textD "…", "…"`](../dev/commands.md#textd-primary-secondary) | A line plus its translation as a subtitle near the bottom (italic grey by default; see the `Subtitle…` options). |
| [`typewrite(MS, "…")`](../dev/commands.md#typewrite) | A line that appears one letter at a time, `MS` milliseconds apart. ENTER finishes it at once. |
| [`comment "…"`](../dev/commands.md#comment) | A note row in the cue list for the operator. Not a cue: ENTER skips it. |

Place text somewhere else with `pos([X, Y], "…")`; style it with the [modifiers](#modifiers). With the `FixedTextCentre` option on, lines appear where they will stay instead of the block moving up as lines are added — see [Fixed text centre](../dev/commands.md#fixed-text-centre).

## Fonts

| How | What it does |
|---|---|
| `Font`, `FontSize`, `FontBold`, … in `[Options]` | The font for the whole scene (or, in `schema.toml`, the project). See [options.md](options.md#text). |
| [`use Fancy.ttf` / `use default`](../dev/commands.md#use-font--fontsize-n) | The font for the text **below this line** in the file. Not a cue. |
| [`fontSize 90` / `fontSize default`](../dev/commands.md#use-font--fontsize-n) | The size for the text below this line. Not a cue. |

A font file is looked for in the project's `fonts/` or `font/` folder, the project folder, then the shared `FONTS` folder. `textCont` after a `use` mixes fonts on one line.

## Pictures

| Command | What it does |
|---|---|
| [`show logo.png`](../dev/commands.md#show) | Adds a picture (`.png`, `.jpg`, `.bmp`, `.webp`, `.gif` — animated GIFs play). Pictures stack; the newest is on top. |
| `show size([400, 300], logo.png)` | Shows it at that size in pixels (`scale(logo.png, [400, 300])` is the same). |
| `show pos([10, 10], logo.png)` | Places it; `[X, Y]` in percent of the screen. |
| `show layer(-1, bg.png)` | Draw order: higher is in front, default `0`. A background on `-1` stays behind everything shown later. |
| `show trans(50, logo.png)` | Half transparent. |

Pictures can also be turned, spun and moved — see [Modifiers](#modifiers) and [Movement and animation](#movement-and-animation).

## Video and sound

| Command | What it does |
|---|---|
| [`play intro.mp4`](../dev/commands.md#play) | Plays a video (`.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`) under the text. One video at a time. |
| `play intro.mp4 1000, 2000` | With a 1 s fade in from black and a 2 s fade out. The sound fades too if `AudioFade` is on. |
| `play theme.mp3` | Plays a sound. Several sounds can play at once, and over a video. |
| [`stop theme.mp3`](../dev/commands.md#stop) | Stops that sound; the others play on. |
| `stop` | Stops the video, every sound, and the particles. |
| F1–F12 | Sounds on keys, set in `schema.toml` under `[Hotkeys]` — see [controls.md](controls.md#sound-on-the-f-keys). |

During the show `K` pauses the video, `M` the sounds, and `,` / `.` / SHIFT + `,` wind the video back, forward and to the start — see [controls.md](controls.md#video-and-sound).

## Movement and animation

| Command | What it does |
|---|---|
| [`rotate(45, …)`](../dev/commands.md#rotatedeg-content) | Turned 45°. |
| [`spin(180, …)`](../dev/commands.md#spindeg_per_sec-content) | Keeps turning, 180° a second (negative: the other way). |
| [`move(W, [50, 50], 800, …)`](../dev/commands.md#movefrom-to-duration_ms-content) | Slides in from the left edge to the middle in 800 ms, then stays. `N` `S` `E` `W` mean just off that edge. |
| [`pathMove(W, [50, 30], E, 3000, …)`](../dev/commands.md#pathmovep1-p2--pn-duration_ms--mode-content) | Moves through the points over 3 s. Add `loop` or `pingpong` before the content to keep going. |
| [`[Animation]` + `show NAME`](../dev/commands.md#animation) | Something that keeps crossing the screen — e.g. a tumbleweed every 4 s at a random height. |

These work on text and pictures alike: `text move(N, [50, 50], 600, "Ta-da!")`.

An animation is defined in the `[Animation]` section and started from a cue:

```txt
[Animation]
Tumbleweed{
    show     = spin(240, size([150, 150], tumbleweed.png))
    path     = W, E          # left edge to right edge
    duration = 3000          # one crossing takes 3 s
    every    = 4000          # a new one every 4 s
    repeat   = 5             # five in all (default: forever)
    randomY  = 650, 950      # each at a random height, in slave pixels
}

[Commands]
show Tumbleweed
stop Tumbleweed              # no new ones; the one rolling finishes
```

## Particles

Particles are many copies of a picture moving by themselves: rain, snow, confetti.

| Command | What it does |
|---|---|
| [`show particle(SNOW, flake.png)`](../dev/commands.md#particletype-intensity-content) | Starts a particle effect. Types: `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`, `OSCILLATION`, `ORBIT`, `NOISE`, `LIFECURVE`. |
| `show particle(RAIN, 30, drop.png)` | At 30 % intensity (1–100). |
| `show particle(RAIN, randomImages(faces))` | A different picture from `pictures/faces/` for each particle; or a list: `randomImages([a.png, b.png])`. |
| `show particle(RAIN, random(drop.png, [20,20], [80,80], true))` | Each particle a random size in the range, keeping its shape. |
| [`trail(12, …)`](../dev/commands.md#trailn-content--trailn-mode-content), [`trailGlow(12, …)`](../dev/commands.md#trailglown-content) | A trail behind each particle (`GHOST`, `STRETCH` or `GLOW`). |
| [`bounce(true, …)`](../dev/commands.md#bouncebool-content) | Particles fall, bounce off the edges and pile up. |
| [`stopParticleCont`](../dev/commands.md#stopparticlecont) | No new particles; the ones in the air finish. `stopParticleCont NAME` for a group. |

Speed, density and where particles appear can be tuned per type in `[Options]` (`RAIN.SPEED = 1.5`) — see [options.md](options.md#particles).

## Timers

| Command | What it does |
|---|---|
| [`countdown(300)`](../dev/commands.md#countdown) | A countdown from 5:00 at the top of the slave. It stops at `00:00` until cleared. |
| `countdown(300, "da")` | With the label *Nedtælling:* (`"en"`: *Countdown:*). |
| [`clearCountdown`](../dev/commands.md#clearcountdown) | Removes the countdowns, nothing else. |

## Clearing the screen

Pick the clear that removes only what you want gone. Content made inside a `group NAME{…}` is *grouped* and survives a plain `clear`.

| | Text | Pictures | Particles | Countdowns | Animations | Video, sound |
|---|---|---|---|---|---|---|
| [`clear`](../dev/commands.md#clear) | ungrouped | ungrouped | ungrouped | ✓ | finish, no new | — |
| [`clearText`](../dev/commands.md#cleartext) | ✓ all | — | — | — | — | — |
| [`clearImages`](../dev/commands.md#clearimages) | — | ✓ all | ✓ all | — | finish, no new | — |
| [`clear NAME`](../dev/commands.md#clear-name) | that group | that group | that group | — | `NAME`: gone at once | — |
| [`clearAll`](../dev/commands.md#clearall), `BACKSPACE` | ✓ all | ✓ all | ✓ all | ✓ | gone at once | — |
| [`clearCountdown`](../dev/commands.md#clearcountdown) | — | — | — | ✓ | — | — |
| [`stop`](../dev/commands.md#stop) | — | — | ungrouped | — | — | ✓ all |
| Next scene | ungrouped | ungrouped | ungrouped | ✓ | gone at once | ✓ all |

- *ungrouped*: only content not made in a `group`. *finish, no new*: the passes on screen finish their run.
- Particles already told to `stopParticleCont` always finish, whatever clears them.
- `textD` subtitle lines are text. Animation passes are only cleared by the commands in the *Animations* column.

## Running several things at once

| Form | What it does |
|---|---|
| [`a & b & c`](../dev/commands.md#-compound) | All on one ENTER. |
| [`run{ … }`](../dev/commands.md#run--) | The same, one command per line — easier to read when it's long. |
| [`loop(3, 1){ … }`](../dev/commands.md#loopcount-clear_every--) | The lines inside, 3 times, each its own cue; a `clear` after every 1 (0: never). |
| [`group NAME{ … }`](../dev/commands.md#group-name--) | Tags what the cues inside put on screen with `NAME`, so a plain `clear` leaves it and `clear NAME` removes just that. |
| [`macro NAME`](../dev/commands.md#macro-name) | Puts the `[Macro]` block `NAME` here, as if written out. |

## Modifiers

Modifiers wrap what they change and can be nested in any order: `show pos([10, 10], trans(50, rotate(45, logo.png)))`.

| Modifier | On | What it does |
|---|---|---|
| [`colour(RED, …)`](../dev/commands.md#colourcolour-content) | text | Colour of the text or piece. |
| [`bold(…)`, `it(…)`](../dev/commands.md#boldcontent) | text | Bold / italic piece; both for bold italic. |
| [`trans(50, …)`](../dev/commands.md#transpercent-content) | all | Transparency in percent: 0 solid, 100 invisible. |
| [`pos([X, Y], …)`](../dev/commands.md#posx-y-content) | all | Position, in percent of the screen. |
| [`size([W, H], …)`](../dev/commands.md#sizew-h-content), [`scale(…, [W, H])`](../dev/commands.md#scalecontent-w-h) | pictures | Size in pixels. |
| [`layer(N, …)`](../dev/commands.md#layern-content) | pictures | Draw order; higher in front. Text is always in front of pictures. |
| `rotate`, `spin`, `move`, `pathMove` | all | See [Movement and animation](#movement-and-animation). |
| `particle`, `random`, `randomImages`, `trail`, `trailGlow`, `bounce` | pictures | See [Particles](#particles). |

In a `text A + B` line, `colour`, `bold`, `it` and `trans` apply to their own piece; `pos`, `move`, `rotate`, `spin` and `size` apply to the whole line, and the first piece that sets one wins.

## Colours and positions

- **Colours:** `black`, `white`, `red`, `green`, `blue`, `yellow`, `orange`, `purple`, `pink`, `cyan`, `lime`, `grey` — or `#RRGGBB` / `#RGB`. Any case. The same everywhere: `colour(…)` and every colour option. [Full table](../dev/commands.md#colours).
- **Positions:** `[X, Y]` in percent of the screen (the [targeted display](../README.md#words-used-throughout)), from the top-left. `N`, `S`, `E`, `W` — just off that edge — for `move` and `pathMove`.

## Common patterns

**A background that stays while the text changes.** Show the picture once and clear only the text. `clear & show bg.png` on every line would flash the picture and reload it each time.

```txt
show layer(-1, bg.png)
text "Verse one, line one"
text "Verse one, line two"
clearText
text "Verse two, line one"
```

**A song where the lines don't jump.** Turn on `FixedTextCentre` and each verse is centred on all its lines from the start.

```txt
[Options]
FixedTextCentre = true

[Commands]
text "Første linje"
text "Anden linje"
text "Tredje linje"
clearText
```

**Lyrics with a translation.**

```txt
textD "Vi har startet nu!", "We have started now!"
```

**A video with the title over it.**

```txt
play intro.mp4 1000, 1000 & text "THE REVY 2026"
```

**A logo that survives the scene's clears,** removed when you say so.

```txt
group Logo{
    show pos([85, 5], size([150, 150], logo.png))
}
text "Some text"
clear                   # the text goes, the logo stays
clear Logo              # now the logo goes
```

**The same chorus several times.**

```txt
[Macro]
Chorus{
    text "La la la"
    text "La la laa"
    clearText
}

[Commands]
text "Verse one"
clearText
macro Chorus
text "Verse two"
clearText
macro Chorus
```

**Confetti for the finale that ends gently.**

```txt
show particle(CONFETTI, 60, randomImages(confetti))
text "Tak for i aften!"
stopParticleCont          # no new confetti; what's in the air lands
```

**A note for the operator at a cue.**

```txt
[Cue]
Door  Wait for the door slam

[Commands]
play doorbell.mp3 \Door
```

**An intermission timer.**

```txt
clearAll & countdown(900, "da")
```

## When something is wrong

- Press **`C`** during the show or before it: the [project scanner](controls.md#project-scanner) lists every mistake in every scene with its file and line — unknown commands, misspelled options, missing pictures, videos, sounds and fonts.
- A cue that shows its own text instead of doing something has a misspelled command: an unknown first word is shown as text, so the show never stops on a typo. The scanner lists it.
- Press **`O`** right after a cue that went wrong during a show, and fix it afterwards from the [fix-it log](controls.md#marking-a-line-to-fix-later).
- Edited a scene during the show? **`U`** reloads all scene files without leaving.
