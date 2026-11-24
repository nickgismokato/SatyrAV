# Project structure

A SatyrAV project is a directory containing a `schema.toml` file and scene files.

## Directory layout

```
MyRevy2026/
├── schema.toml          # Project definition
├── scenes/
│   ├── TextShowcase.ngk       # ┐ the four tutorial scenes a new
│   ├── PictureShowcase.ngk    # │ project is generated with │   ├── ParticleShowcase.ngk   # │
│   ├── MacroShowcase.ngk      # ┘
│   ├── Intro.ngk
│   └── Finale.ngk
├── pictures/
│   ├── logo.png
│   └── professorPhotos/
│       ├── prof1.jpg
│       └── prof2.jpg
├── movies/
│   └── intro.mkv
├── sound/
│   ├── theme.mp3
│   └── applause.wav
├── fix-it/              # Lines marked with O during a show
│   └── ErrorInFiles-2026-11-24.LOG
└── showlog/             # Show log and per-act subtitles (Show Log & Subtitles on)
    ├── MatRevy2026-2026-12-24-1.log
    ├── MatRevy2026-2026-12-24-1-Akt1.srt
    └── MatRevy2026-2026-12-24-1-Akt1.ass
```

The `fix-it/` folder is created when the project is opened. It holds one `ErrorInFiles-YYYY-MM-DD.LOG` per day on which the `O` key was used; see [controls.md](../howto/controls.md#marking-a-line-to-fix-later).

The `showlog/` folder is created by the first cue of a show, only when *Show Log & Subtitles* is on in Options. Each run writes a `.log` and, per act, an `.srt` and an `.ass` (`core/ShowLog`, `core/Subtitles`); see [structure.md](../howto/structure.md#the-show-log-and-subtitles).

Media files referenced in scene commands are resolved relative to the project root, with automatic fallback search in `pictures/`, `movies/`, and `sound/` subdirectories.

## schema.toml

The project definition file uses TOML format. A generated `schema.toml` contains **every** option the parser reads, uncommented and at its default value, so options can be changed in place without looking them up:

```toml
[RevyData]
Revy    = "MatRevy 2026"
Creator = "Nick"          # Persisted authorship — shown in Load Project

[Options]
FontSize             = 60        # Slave text size, in points
TextOutline          = 4         # Black outline thickness in points, 0 = off
DisableTextOutline   = false     # true = no outline (a scene can override either way)
TransparentBackground = false    # true = see-through slave where nothing is drawn (a scene can override)
Capitalize           = false     # Uppercase every line of slave text
AudioFade            = false     # Let `play` fade times fade the sound too, not just the picture
FixedTextCentre      = true      # Centre text on all its lines up to the next clear, so lines don't jump
BackgroundColour     = "black"   # A colour name, or a "#RRGGBB" string
TextColour           = "white"   # Default slave text colour: a colour name, or "#RRGGBB"
Font                 = ""        # Regular face: full path, or a bare FONTS/ filename
FontItalic           = ""        # Italic face, used by textit and it(...)
FontBold             = ""        # Bold face, used by textbf and bold(...)
FontBoldItalic       = ""        # Used when a run is bold AND italic
SubtitleItalic       = true      # Render the textD translation line italic
SubtitleColour       = "grey"    # grey | white | black, or a "#RRGGBB" string
SubtitleTransparency = 0.0       # Percent: 0 = opaque, 100 = invisible
SubtitlePosY         = 90.0      # Percent down from the top of the canvas

[Structure]
akter = 2

[Structure.Akt1]
schema = [
    "Intro",
    "Scene1",
    "Scene2"
]

[Structure.Akt2]
schema = [
    "Scene3",
    "Finale"
]

# Targeted-display rectangle — configured via the Debug → S menu.
# Refreshed on save; all zero means "fill the physical display".
[Display]
Width    = 0
Height   = 0
CenterX  = 0
CenterY  = 0
Rotation = 0.0

# F1–F12 audio hotkeys — bare filenames resolved against sound/.
# Empty = unbound. Pressing the same key twice while audio plays stops it.
# The generator emits all twelve; only the bound ones are shown here.
[Hotkeys]
F1  = "knock.mp3"
F12 = "applause.mp3"

# Backstage screen: each scene change sends the current scene and
# the next two (those with a [Backstage] block) to the backstage ticker.
# Enabled = false switches it off and keeps the Url. See docs/dev/commands.md.
[Backstage]
Enabled = true
Url     = "https://backstage.texnik.dk/set-bck"

# Project-level particle tunables — one table per type.
# Scene [Options] can override individual fields. The generator ships this
# block commented out, listing every key at its default, because "no table
# at all" *is* the default — writing ten no-op tables into every new
# project is noise.
[Particle.RAIN]
Speed    = 1.5
Density  = 2.0
XDist    = "NORMAL"
XDistP1  = 0.5
XDistP2  = 0.1
```

> **Comments are not round-tripped.** `Project::Save` (triggered by the Debug → S rect editor) reloads the file through toml++ and re-emits it, and toml++ does not preserve stand-alone comments. Keys and values survive; the generated commentary does not.

Scene names in the `schema` arrays correspond to `.ngk` filenames in the `scenes/` directory (without extension).

## Options cascade

Settings are resolved in priority order: scene → project → global config.
 An option has the same name in a scene's `[Options]` and in `schema.toml`, and case does not matter. Every colour option takes the same colours — see [Colours](commands.md#colours).

| Setting | Scene (`[Options]` in .ngk) | Project (`schema.toml [Options]`) | Global (config) |
|---|---|---|---|
| Font size | `FontSize = 30` | `FontSize = 60` *(generated value — a schema with no `FontSize` key at all still falls back to 25)* | Config font size |
| Background | `BackgroundColour = blue` | `BackgroundColour = "black"` | Black |
| Text colour | `TextColour = #FFD700` | `TextColour = "white"` | White |
| Capitalize | `Capitalize = true` | `Capitalize = false` | false |
| Audio fade | `AudioFade = true` | `AudioFade = false` | false |
| Fixed text centre | `FixedTextCentre = false` (either way) | `FixedTextCentre = true` *(generated value; a schema without the key is off)* | false |
| Text outline | `TextOutline = 6` | `TextOutline = 4` | 4 |
| Disable text outline | `DisableTextOutline = true` (either way) | `DisableTextOutline = false` | false |
| See-through slave | `TransparentBackground = true` (either way) | `TransparentBackground = false` | false |
| Font | `Font = path.ttf` | `Font = path.ttf` | System default |
| Particle speed | `RAIN.SPEED = 1.5` | `[Particle.RAIN] Speed = 1.5` | `1.0` |
| Particle density | `RAIN.DENS = 2.0` | `[Particle.RAIN] Density = 2.0` | `1.0` |
| Particle X-dist | `RAIN.X.DIST = NORMAL(0.5, 0.1)` | `[Particle.RAIN] XDist = "NORMAL" …` | `UNIFORM` |
| Italic font | `FontItalic = path.ttf` | `FontItalic = path.ttf` | `[font] italic = ...` in `config.toml` (or bundled DejaVu) |
| Bold font | `FontBold = path.ttf` | `FontBold = path.ttf` | `[font] bold = ...` in `config.toml` (or bundled DejaVu) |
| Bold-italic font | `FontBoldItalic = path.ttf` | `FontBoldItalic = path.ttf` | `[font] bold_italic = ...` in `config.toml` (or bundled DejaVu) |

If a scene sets `FontSize = 30`, that overrides the project's `FontSize` for that scene only; all other scenes keep the project value. Particle tunings cascade per-field — setting only `SPEED` in a scene still inherits `DENS` and `X.DIST` from the project level. The same applies to font slots: a scene can override only the bold face and still pick up project-level italic / bold-italic / regular. Fonts are resolved afresh for every scene, so a scene's own `Font` never carries over into the scenes after it.

## Global settings — `config.toml`

Settings that belong to the machine rather than to a project. The file lives at `Documents\SatyrAV\config.toml` on Windows and `~/.config/satyrav/config.toml` on Linux. It is written by the Options screen; every key can also be edited by hand. Wrap paths in single quotes, so that backslashes are not read as escapes.

| Key | Default | Meaning |
|---|---|---|
| `[font] size` | `25` | Master (operator) text size. Also the slave size when neither project nor scene sets one. |
| `[font] path`, `italic`, `bold`, `bold_italic` | empty | Font faces. Empty means the bundled DejaVu Sans family. |
| `[display] width`, `height` | `1280`, `720` | Size of the master window. |
| `[display] fullscreen` | `false` | Read and saved, but not currently used by anything. |
| `[display] slave_index` | `1` | Which display the slave opens on, counting from 0. |
| `[display] slave_windowed` | `false` | Open the slave as a window instead of fullscreen (development). |
| `[display] capture_enabled`, `capture_display` | `false`, `0` | The capture mirror and the display it opens on. |
| `[display] show_clock` | `false` | Clock and stopwatches on the master. **Clock on Master** in Options. |
| `[paths] projects`, `fonts` | empty | Where projects and shared fonts live. Empty means the default locations below. |
| `[paths] last_project` | empty | Remembered by the application. |
| `[video] preload_budget_mb` | `4096` | Total budget for preloaded video. |
| `[video] preload_max_seconds` | `60` | Longest look-ahead buffer per clip on the paths that buffer in RAM. |
| `[video] zero_copy` | `true` | Keep hardware-decoded video on the GPU. Set to `false` to copy frames through system memory instead, if a machine's graphics driver misbehaves. See [video-pipeline.md](video-pipeline.md). |
| `[audio] device` | empty | Output device by name. Empty means the system default. |

## Shared directories

Two directories sit parallel to `projects/` and are used by every project:

```
~/Documents/SatyrAV/        (Windows)        ~/satyrav/   (Linux/macOS)
├── projects/   ← your projects live here
├── FONTS/      ← shared font files; `Font = "Inter.ttf"` in any project picks up `FONTS/Inter.ttf`
└── log/        ← per-day rotating log files (satyrav-YYYY-MM-DD.log)
```

Both folders are auto-created on first launch. The font path keys in `config.toml` (`[font] path/italic/bold/bold_italic`) and in any project / scene `[Options]` block accept either an absolute path or a bare filename — bare names resolve against `FONTS/` so projects don't need to embed font files. Logs are written from `core/Logger.cpp` and mirrored to stderr; see [architecture.md](architecture.md) for the logging API.

## Creating a project

From the main menu, select "New project". Fill in:

- **Revy type** — one of: `MatRevy`, `BioRevy`, `KemiRevy`, `MBKRevy`, `PsykoRevy`, `FysikRevy`, `GeoRevy`, `DIKURevy`, `SaTyR`, or `Other`. Selecting `Other` reveals an extra text field for a custom revy name.
- **Jubi** — optional toggle that appends `Jubi` to the revy name for jubilee productions. For example, `MatRevy` + Jubi → folder `MatRevyJubi_2026`; custom name `Foo` + Jubi → `FooJubi_2026`.
- **Year** — the production year.
- **Your name** — stored for reference.

The project directory is created under the configured projects path (default: `~/satyrav/projects/` on Linux, `Documents\SatyrAV\projects\` on Windows).

### Generated example scenes

A new project ships with two akter and four tutorial scenes, one per layer of the engine. All four reference only `pictures/satyr-logo.png` — the single asset the generator copies in — so a fresh project runs end to end before any media is added.

| Akt | Scene | Covers |
|---|---|---|
| 1 | `TextShowcase` | `text`, `textbf` / `textit`, `colour()` and `+` concatenation, `textCont`, `pos()`, `textD` translation lines, `countdown` |
| 1 | `PictureShowcase` | `show`, `size()` / `pos()`, image stacking, `trans()`, `rotate()` / `spin()`, `move()`, `random()`, the `&` compound operator |
| 2 | `ParticleShowcase` | every `particle()` type, intensity, `trail()` / `trailGlow()`, `bounce()`, `stopParticleCont`, per-scene `<TYPE>.SPEED` / `.DENS` / `.X.DIST` tuning |
| 2 | `MacroShowcase` | `[Macro]` definitions and `macro NAME`, `run{}`, `group NAME{}` with `clear NAME`, `loop(){}`, `clearText` / `clearAll` |



## Loading a project

The Load Project screen (from the main menu) groups projects by Revy. Each header (`MatRevy`, `KemiRevy`, …) collapses Jubi and non-Jubi editions into the same group. Two columns are shown per row: the project's full Revy name and the persisted creator (a dim em-dash for projects without a `Creator` field — adding it to `schema.toml` is enough).

Three filters on top of the list:

- **Search** — case-insensitive substring match on the Revy name column.
- **Revy** — dropdown of `All` plus every distinct group derived from the live scan. Selecting a single Revy collapses the list to that group only.
- **Sort** — `Year` (default, descending), `Recent` (folder modification time descending), or `Name` (A→Z). Sorts within each group; group order is always alphabetical.

`TAB` cycles focus between Search → Revy → Sort → List. `ENTER` on the list opens the highlighted project; `ESC` defocuses to the list, then back to the main menu.
