# Options

Every setting in SatyrAV, where it goes, and what it does.

**Contents:** [Three places](#three-places) · [Text](#text) · [Background and screen](#background-and-screen) · [Sound](#sound) · [Translation subtitles](#translation-subtitles) · [Particles](#particles) · [Other project settings](#other-project-settings) · [The Options screen](#the-options-screen)

## Three places

| Where | Applies to | Written by |
|---|---|---|
| `[Options]` in a scene's `.ngk` | That scene only | You |
| `[Options]` in the project's `schema.toml` | Every scene in the project | You — a new project lists every option at its default |
| The **Options** screen (`config.toml`) | This computer, every project | The Options screen |

A scene's value wins over the project's, and the project's over the computer's: **scene → project → default**. A scene that doesn't set an option gets the project's, never the previous scene's.

An option has the **same name** in a scene and in `schema.toml`, in any case (`TextColour`, `textcolour`). Older spellings such as `bc`, `cap` and `font` still work. A value may be quoted, which a font path with spaces needs. In `schema.toml`, write text and colours in quotes and numbers and `true` / `false` without.

```txt
[Options]
BackgroundColour = black
TextColour       = #FFD700     # gold — a # straight after = is a colour, not a comment
FontSize         = 70
```

## Text

| Option | Values | Default | What it does |
|---|---|---|---|
| `FontSize` | points | project: 25 (new projects: 60) | Size of the slave text. `fontSize N` in `[Commands]` changes it for the lines below. |
| `Font` | font file | bundled DejaVu Sans | The font. A bare name is looked for in the project's `fonts/` / `font/` folder, the project folder, then the shared `FONTS` folder. |
| `FontBold`, `FontItalic`, `FontBoldItalic` | font file | the DejaVu Sans faces | The faces `textbf`, `textit`, `bold(…)` and `it(…)` use. Missing: the regular face, made bold / slanted. |
| `TextColour` | [colour](scene.md#colours-and-positions) | white | Text colour, unless `colour(…)` says otherwise. Countdowns use it too. |
| `TextOutline` | points, `0` = none | 4 | Black outline round the letters. |
| `DisableTextOutline` | `true` / `false` | `false` | `true`: no outline, whatever `TextOutline` says. A scene can switch it either way. |
| `Capitalize` | `true` / `false` | `false` | ALL TEXT IN CAPITALS. A scene can switch it on, not off. |
| `FixedTextCentre` | `true` / `false` | `false` (new projects: `true`) | Lines appear where they will stay, instead of the block moving up as lines are added. A scene can switch it either way. [Details](../dev/commands.md#fixed-text-centre) |

## Background and screen

| Option | Values | Default | What it does |
|---|---|---|---|
| `BackgroundColour` | [colour](scene.md#colours-and-positions) | black | The slave's background. |
| `TransparentBackground` | `true` / `false` | `false` | The slave is see-through wherever nothing is drawn, so another program on that screen shows underneath. A scene can switch it either way. The capture shows black there. [Details](../dev/commands.md#see-through-slave) |

A project that uses `TransparentBackground` anywhere opens a see-through slave window when the project opens. If you add it during a show with `U`, leave the project and open it again.

## Sound

| Option | Values | Default | What it does |
|---|---|---|---|
| `AudioFade` | `true` / `false` | `false` | The fade times on `play video.mp4 IN, OUT` fade the video's sound too, not only the picture. A scene can switch it on, not off. |

## Translation subtitles

For the second line of `textD`. **`schema.toml` only** — the same for every scene.

| Option | Values | Default | What it does |
|---|---|---|---|
| `SubtitleItalic` | `true` / `false` | `true` | Italic. |
| `SubtitleColour` | [colour](scene.md#colours-and-positions) | `grey` (`#AAAAAA` here) | Colour. |
| `SubtitleTransparency` | 0–100 | 0 | Percent: 0 solid, 100 invisible. |
| `SubtitlePosY` | 0–100 | 90 | Where the newest subtitle sits, in percent from the top. |

## Particles

Each particle type (`RAIN`, `SNOW`, …) can be tuned for a scene in `[Options]`, or for the project in a `[Particle.TYPE]` table in `schema.toml`. A scene setting only one key keeps the project's others.

| In a scene | In `schema.toml` `[Particle.RAIN]` | Default | What it does |
|---|---|---|---|
| `RAIN.SPEED = 1.5` | `Speed = 1.5` | 1.0 | Faster or slower (0.01–20). |
| `RAIN.DENS = 2.0` | `Density = 2.0` | 1.0 | More or fewer particles (0.01–20). |
| `RAIN.X.DIST = NORMAL(0.5, 0.1)` | `XDist = "NORMAL"`, `XDistP1 = 0.5`, `XDistP2 = 0.1` | `UNIFORM` | Where across the screen they appear. |
| `RAIN.VX.DIST`, `RAIN.VY.DIST` | `VXDist…`, `VYDist…` | `UNIFORM` | How their sideways / downward speeds spread. |

Distributions: `UNIFORM`, `NORMAL(mean, spread)`, `LOGNORMAL(mu, sigma)`, `BINOMIAL(n, p)`, `BERNOULLI(p)`, `GAMMA(shape, scale)`, with values from 0 to 1 across the screen — `NORMAL(0.5, 0.1)` gathers them in the middle. [Details](../dev/commands.md#particle-tunables-in-options)

## Other project settings

These tables live only in `schema.toml`; [project.md](project.md#schematoml) shows a whole file.

| Table | What it's for |
|---|---|
| `[RevyData]` | `Revy` (name and year) and `Creator`. |
| `[Structure]`, `[Structure.AktN]` | The akter and the scenes in each, in order. See [project.md](project.md#akter-and-scenes). |
| `[Hotkeys]` | `F1 = "knock.mp3"` … `F12` — sounds on the F-keys. See [controls.md](controls.md#sound-on-the-f-keys). |
| `[Backstage]` | `Url` and `Enabled` — where the backstage screen is. New projects have it on. [Details](../dev/commands.md#backstage) |
| `[Display]` | The targeted display — written for you by `D` → `S`. All zero: the whole screen. |

## The Options screen

From the main menu. These belong to the computer, not to a project; they are saved in `config.toml` (`Documents\SatyrAV\config.toml` on Windows, `~/.config/satyrav/config.toml` on Linux) when you press **Save**.

| Field | What it does |
|---|---|
| Font Size | Text on the master (the operator's screen). Also the slave's size when neither project nor scene sets one. |
| Font Colour | Not used at the moment: it is neither saved nor applied (see [known bugs](../known-bugs.md)). |
| Projects Directory | Where your projects are. ENTER opens a folder dialog; you can also type the path. |
| Secondary Display | Which screen is the slave. |
| Slave Window Mode | *Fullscreen* for shows; *Windowed* to work on one screen. |
| Capture Mirror | A screen for the capture output (a copy of the slave for the recording), or *Off*. Off in windowed mode. |
| Audio Output | The sound device; *Default* follows the system. |
| Clock on Master | The time, the scene stopwatch and the show stopwatch on the master. |
| Show Log & Subtitles | Write the show log and the per-act subtitle files — turn on for shows that are recorded. See [structure.md](structure.md#the-show-log-and-subtitles). |

A few settings have no field and are changed in `config.toml` by hand — video preloading and the zero-copy switch among them. They are listed in [project-structure.md](../dev/project-structure.md#global-settings--configtoml).
