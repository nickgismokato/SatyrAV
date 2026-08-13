# Project structure

A SatyrAV project is a directory containing a `schema.toml` file and scene files.

## Directory layout

```
MyRevy2026/
├── schema.toml          # Project definition
├── scenes/
│   ├── TextShowcase.ngk       # ┐ the four tutorial scenes a new
│   ├── PictureShowcase.ngk    # │ project is generated with (1.8.0)
│   ├── ParticleShowcase.ngk   # │
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
└── sound/
    ├── theme.mp3
    └── applause.wav
```

Media files referenced in scene commands are resolved relative to the project root, with automatic fallback search in `pictures/`, `movies/`, and `sound/` subdirectories.

## schema.toml

The project definition file uses TOML format. Since *1.8.0* a generated `schema.toml` contains **every** option the parser reads, uncommented and at its default value, so options can be changed in place without looking them up:

```toml
[RevyData]
Revy    = "MatRevy 2026"
Creator = "Nick"          # (1.6.2) Persisted authorship — shown in Load Project

[Options]
FontSize             = 60        # Slave text size, in points
TextOutline          = 4         # Black outline thickness in points, 0 = off
Capitalize           = false     # Uppercase every line of slave text
BackgroundColour     = "black"   # black | blue | red | green | white
Font                 = ""        # Regular face: full path, or a bare FONTS/ filename
FontItalic           = ""        # (1.6.1) Italic face, used by textit and it(...)
FontBold             = ""        # (1.6.1) Bold face, used by textbf and bold(...)
FontBoldItalic       = ""        # (1.6.1) Used when a run is bold AND italic
SubtitleItalic       = true      # (1.6.1) Render the textD translation line italic
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

# (1.2) Targeted-display rectangle — configured via the Debug → S menu.
# Refreshed on save; all zero means "fill the physical display".
[Display]
Width    = 0
Height   = 0
CenterX  = 0
CenterY  = 0
Rotation = 0.0

# (1.6.3) F1–F12 audio hotkeys — bare filenames resolved against sound/.
# Empty = unbound. Pressing the same key twice while audio plays stops it.
# The generator emits all twelve; only the bound ones are shown here.
[Hotkeys]
F1  = "knock.mp3"
F12 = "applause.mp3"

# (1.4) Project-level particle tunables — one table per type.
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

| Setting | Scene (`[Options]` in .ngk) | Project (`schema.toml [Options]`) | Global (config) |
|---|---|---|---|
| Font size | `FontSize = 30` | `FontSize = 60` *(1.8.4; generated value — a schema with no `FontSize` key at all still falls back to 25)* | Config font size |
| Background | `bc = blue` | `BackgroundColour = black` | Black |
| Capitalize | `cap = true` | `Capitalize = false` | false |
| Text outline | — | `TextOutline = 4` | 4 |
| Font | `font = path.ttf` | `Font = path.ttf` | System default |
| Particle speed *(1.4)* | `RAIN.SPEED = 1.5` | `[Particle.RAIN] Speed = 1.5` | `1.0` |
| Particle density *(1.4)* | `RAIN.DENS = 2.0` | `[Particle.RAIN] Density = 2.0` | `1.0` |
| Particle X-dist *(1.4)* | `RAIN.X.DIST = NORMAL(0.5, 0.1)` | `[Particle.RAIN] XDist = "NORMAL" …` | `UNIFORM` |
| Italic font *(1.6.1)* | `fontItalic = path.ttf` | `FontItalic = path.ttf` | `[font] italic = ...` in `config.toml` (or bundled DejaVu) |
| Bold font *(1.6.1)* | `fontBold = path.ttf` | `FontBold = path.ttf` | `[font] bold = ...` in `config.toml` (or bundled DejaVu) |
| Bold-italic font *(1.6.1)* | `fontBoldItalic = path.ttf` | `FontBoldItalic = path.ttf` | `[font] bold_italic = ...` in `config.toml` (or bundled DejaVu) |

If a scene sets `FontSize = 30`, that overrides the project's `FontSize` for that scene only; all other scenes keep the project value. Particle tunings cascade per-field — setting only `SPEED` in a scene still inherits `DENS` and `X.DIST` from the project level. The same applies to font slots: a scene can override only the bold face and still pick up project-level italic / bold-italic / regular.

## Shared directories *(1.6.1+)*

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
- **Jubi** *(1.3)* — optional toggle that appends `Jubi` to the revy name for jubilee productions. For example, `MatRevy` + Jubi → folder `MatRevyJubi_2026`; custom name `Foo` + Jubi → `FooJubi_2026`.
- **Year** — the production year.
- **Your name** — stored for reference.

The project directory is created under the configured projects path (default: `~/satyrav/projects/` on Linux, `Documents\SatyrAV\projects\` on Windows).

### Generated example scenes *(1.8.0)*

A new project ships with two akter and four tutorial scenes, one per layer of the engine. All four reference only `pictures/satyr-logo.png` — the single asset the generator copies in — so a fresh project runs end to end before any media is added.

| Akt | Scene | Covers |
|---|---|---|
| 1 | `TextShowcase` | `text`, `textbf` / `textit`, `color()` and `+` concatenation, `textCont`, `pos()`, `textD` translation lines, `countdown` |
| 1 | `PictureShowcase` | `show`, `size()` / `pos()`, image stacking, `trans()`, `rotate()` / `spin()`, `move()`, `random()`, the `&` compound operator |
| 2 | `ParticleShowcase` | every `particle()` type, intensity, `trail()` / `trailGlow()`, `bounce()`, `stopParticleCont`, per-scene `<TYPE>.SPEED` / `.DENS` / `.X.DIST` tuning |
| 2 | `MacroShowcase` | `[Macro]` definitions and `macro NAME`, `run{}`, `group NAME{}` with `clear NAME`, `loop(){}`, `clearText` / `clearAll` |

Before 1.8.0 a single `ExampleScene.ngk` was generated instead. Existing projects are untouched — this only affects newly created ones.

## Loading a project *(1.6.2 redesign)*

The Load Project screen (from the main menu) now groups projects by Revy. Each header (`MatRevy`, `KemiRevy`, …) collapses Jubi and non-Jubi editions into the same group. Two columns are shown per row: the project's full Revy name and the persisted creator (a dim em-dash for projects authored before 1.6.2 that don't have a `Creator` field yet — adding it manually to `schema.toml` is enough).

Three filters on top of the list:

- **Search** — case-insensitive substring match on the Revy name column.
- **Revy** — dropdown of `All` plus every distinct group derived from the live scan. Selecting a single Revy collapses the list to that group only.
- **Sort** — `Year` (default, descending), `Recent` (folder modification time descending), or `Name` (A→Z). Sorts within each group; group order is always alphabetical.

`TAB` cycles focus between Search → Revy → Sort → List. `ENTER` on the list opens the highlighted project; `ESC` defocuses to the list, then back to the main menu.
