# Project structure

A SatyrAV project is a directory containing a `schema.toml` file and scene files.

## Directory layout

```
MyRevy2026/
├── schema.toml          # Project definition
├── scenes/
│   ├── ExampleScene.ngk
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

The project definition file uses TOML format:

```toml
# Project options — apply to all scenes unless overridden
[Options]
FontSize = 25
TextOutline = 4        # Black outline thickness (pt), 0 = disabled
Capitalize = false     # Uppercase all text
# Font = "path/to.ttf" # Custom font
# BackgroundColour = black

[RevyData]
Revy = "MatRevy 2026"

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
# Refreshed on save; width = 0 means "fill the physical display".
[Display]
Width    = 1920
Height   = 1080
CenterX  = 960
CenterY  = 540
Rotation = 0.0

# (1.4) Project-level particle tunables — one table per type.
# Scene [Options] can override individual fields.
[Particle.RAIN]
Speed    = 1.5
Density  = 2.0
XDist    = "NORMAL"
XDistP1  = 0.5
XDistP2  = 0.1
```

Scene names in the `schema` arrays correspond to `.ngk` filenames in the `scenes/` directory (without extension).

## Options cascade

Settings are resolved in priority order: scene → project → global config.

| Setting | Scene (`[Options]` in .ngk) | Project (`schema.toml [Options]`) | Global (config) |
|---|---|---|---|
| Font size | `FontSize = 30` | `FontSize = 25` | Config font size |
| Background | `bc = blue` | `BackgroundColour = black` | Black |
| Capitalize | `cap = true` | `Capitalize = false` | false |
| Text outline | — | `TextOutline = 4` | 4 |
| Font | `font = path.ttf` | `Font = path.ttf` | System default |
| Particle speed *(1.4)* | `RAIN.SPEED = 1.5` | `[Particle.RAIN] Speed = 1.5` | `1.0` |
| Particle density *(1.4)* | `RAIN.DENS = 2.0` | `[Particle.RAIN] Density = 2.0` | `1.0` |
| Particle X-dist *(1.4)* | `RAIN.X.DIST = NORMAL(0.5, 0.1)` | `[Particle.RAIN] XDist = "NORMAL" …` | `UNIFORM` |

If a scene sets `FontSize = 30`, that overrides the project's `FontSize = 25` for that scene only. All other scenes use 25. Particle tunings cascade per-field — setting only `SPEED` in a scene still inherits `DENS` and `X.DIST` from the project level.

## Creating a project

From the main menu, select "New project". Fill in:

- **Revy type** — one of: `MatRevy`, `BioRevy`, `KemiRevy`, `MBKRevy`, `PsykoRevy`, `FysikRevy`, `GeoRevy`, `DIKURevy`, `SaTyR`, or `Other`. Selecting `Other` reveals an extra text field for a custom revy name.
- **Jubi** *(1.3)* — optional toggle that appends `Jubi` to the revy name for jubilee productions. For example, `MatRevy` + Jubi → folder `MatRevyJubi_2026`; custom name `Foo` + Jubi → `FooJubi_2026`.
- **Year** — the production year.
- **Your name** — stored for reference.

The project directory is created under the configured projects path (default: `~/satyrav/projects/` on Linux, `Documents\SatyrAV\projects\` on Windows). An example scene is included to demonstrate the command syntax.
