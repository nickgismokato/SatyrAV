# Projects

A project is one revy in one year: a folder with a `schema.toml`, the scene files, and the pictures, videos, sounds and fonts they use.

**Contents:** [Creating a project](#creating-a-project) · [The project folder](#the-project-folder) · [Files and names](#files-and-names) · [Akter and scenes](#akter-and-scenes) · [schema.toml](#schematoml) · [Opening a project](#opening-a-project) · [Shared folders](#shared-folders)

## Creating a project

Main menu → **New project**:

- **Revy** — `MatRevy`, `BioRevy`, `KemiRevy`, `MBKRevy`, `PsykoRevy`, `FysikRevy`, `GeoRevy`, `DIKURevy`, `SaTyR`, or `Other` with a name of your own.
- **Jubi** — for a jubilee year: adds `Jubi` to the name (`MatRevyJubi_2026`).
- **Year** and **your name** (shown in the project list).

The project is made in the projects folder (set in [Options](options.md#the-options-screen)) as `<Revy>_<Year>`, with two akter and four example scenes — text, pictures, particles, and groups / macros — that run straight away. Read them, change them, then replace them with the show.

## The project folder

```
MatRevy_2026/
├── schema.toml          the project: name, options, akter and scenes
├── scenes/              one .ngk per scene
│   ├── Intro.ngk
│   └── Finale.ngk
├── pictures/            .png .jpg .bmp .webp .gif
│   └── faces/           a folder for randomImages(faces)
├── movies/              .mp4 .mkv .webm .avi .mov
├── sound/               .mp3 .wav …
├── fonts/               .ttf .otf for this project (or font/)
├── fix-it/              made by SatyrAV: lines marked with O
└── showlog/             made by SatyrAV: show logs and subtitles
```

`fix-it/` is made when the project is opened, `showlog/` only by a show with *Show Log & Subtitles* on. Everything else is yours to make.

## Files and names

- **Scenes** are `scenes/<Name>.ngk`, named in `schema.toml` without `.ngk`. The name is what the master shows; `æøå` and spaces are fine.
- **Media** can go anywhere in the project folder. A scene names a file by itself (`show logo.png`) and SatyrAV looks in the project folder, then `pictures/`, `movies/` and `sound/`. A path with a folder in it — `show posters/act1.png` — is taken from the project folder.
- **Video or sound** is decided by the extension: `.mp4`, `.mkv`, `.webm`, `.avi` and `.mov` are videos, anything else `play` is given is sound.
- **Fonts** are looked for in the project's `fonts/` (or `font/`) folder, the project folder, then the shared `FONTS` folder — so a font only one show uses can travel with the project.

## Akter and scenes

The show is the akter in order, and each akt is its scenes in order, as `schema.toml` lists them:

```toml
[Structure]
akter = 2

[Structure.Akt1]
schema = ["Intro", "Sketch1", "Song1"]

[Structure.Akt2]
schema = ["Sketch2", "Finale"]
```

- A scene can be in more than one akt, or twice in one.
- A `.ngk` in `scenes/` that no akt lists is never shown (the [scanner](controls.md#project-scanner) points it out).
- To add a scene: create `scenes/Name.ngk`, add `"Name"` to an akt, and press `U` if the project is open.

## schema.toml

A new project's `schema.toml` lists every option at its default, so you change values instead of looking names up. The parts:

```toml
[RevyData]
Revy    = "MatRevy 2026"
Creator = "Anna"

[Options]                     # the project's defaults — see options.md
FontSize              = 60
TextOutline           = 4
DisableTextOutline    = false
TransparentBackground = false
Capitalize            = false
AudioFade             = false
FixedTextCentre       = true
BackgroundColour      = "black"
TextColour            = "white"
Font                  = ""
FontItalic            = ""
FontBold              = ""
FontBoldItalic        = ""
SubtitleItalic        = true
SubtitleColour        = "grey"
SubtitleTransparency  = 0.0
SubtitlePosY          = 90.0

[Structure]                   # the akter — see above
akter = 2

[Structure.Akt1]
schema = ["Intro", "Song1"]

[Structure.Akt2]
schema = ["Finale"]

[Display]                     # the targeted display, written by D → S
Width    = 0
Height   = 0
CenterX  = 0
CenterY  = 0
Rotation = 0.0

[Hotkeys]                     # sounds on F1–F12
F1  = "knock.mp3"
F12 = "applause.mp3"

[Backstage]                   # the backstage screen
Enabled = true
Url     = "https://backstage.texnik.dk/set-bck"
```

Every option is explained in [options.md](options.md). When `D` → `S` saves the targeted display, SatyrAV rewrites the file: values are kept, but comments you added are lost.

## Opening a project

Main menu → **Load project**. Projects are grouped by revy; **Search** filters by name, **Revy** shows one revy, **Sort** orders by year, last change or name. `TAB` moves between them and the list, `ENTER` opens.

Opening a project opens the slave on the screen chosen in Options. The show itself starts with the first cue — see [structure.md](structure.md).

## Shared folders

Next to the projects folder:

```
Documents\SatyrAV\   (Windows)        ~/satyrav/   (Linux)
├── projects/   your projects
├── FONTS/      fonts every project can use by name
└── log/        SatyrAV's own log, one file per day
```

Both are made on first start. When something goes wrong, the day's file in `log/` says what and why.
