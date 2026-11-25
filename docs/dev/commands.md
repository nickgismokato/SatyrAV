# Command reference

The full reference for every section, command, block and modifier of a `.ngk` scene file: exact syntax, every argument, edge cases, and where each part lives in the code. Written for people changing SatyrAV. To *use* the commands, [howto/scene.md](../howto/scene.md) has them in tables by what they do, with examples.

Every entry has the same fields where they apply: **Syntax** (argument names in UPPER_CASE), **Arguments**, **Behaviour**, **Output** (what the operator and audience see) and **Example**.

## Where it lives in the code

A cue goes through three stages: the parser turns a line into a `Command` (`core/Types.hpp`), `CueEngine` runs it, and `PerformanceScreen` puts it on the slave.

| Part | Code |
|---|---|
| Sections, `[Options]`, `[Preload]`, `[Macro]`, `[Cue]`, `[Backstage]`, `[Animation]` | `SceneParser::ParseFile` (`core/SceneParser.cpp`); `[Animation]` blocks in `ParseAnimationBlock`, `[Backstage]` keys in `Backstage::ApplyKey` (`core/Backstage.cpp`) |
| Option names and aliases | `LookupOptionKey` (`core/OptionKeys.cpp`) |
| One command line: keyword, arguments | `ParseSingleCommand` (`core/SceneParser.cpp`) |
| `&`, `run{}`, `loop(){}`, `group{}`, `macro` | `ProcessLogicalCommand`, `ExpandLoop`, `TagCommandWithGroup` |
| Modifiers | `ParseModifiers` → `RenderModifiers` (`core/Types.hpp`) |
| Text, `+` pieces, `bold()` / `it()` / `colour()` per piece | `ParseTextRuns`, `SplitPlus` → `TextRun` |
| `use` / `fontSize` | parsed in `ParseSingleCommand`, applied to the text below by the stamping pass at the end of `ParseFile` |
| Comments, quotes, `#` inside parentheses | `FindStructural`, `FindCommentStart`, `FindOptionCommentStart`, `DelimAt` |
| LaTeX symbols | `ExpandLatexSymbols` (`core/LatexSymbols.cpp`) |
| Colours | `ParseColour` (`core/Types.cpp`) |
| Running a cue, the options cascade | `CueEngine` (`core/CueEngine.cpp`) |
| What a command does on the slave | `PerformanceScreen::DispatchSlaveCommand`; animations in `StartAnimation` / `UpdateAnimations` |
| Drawing text, images, countdowns, layers, clears | `SlaveWindow` (`render/SlaveWindow.cpp`), glyphs in `TextRenderer` |
| Video and sound | `MediaPlayer` (`render/MediaPlayer.cpp`) — see [video-pipeline.md](video-pipeline.md) |
| Particles, trails, bounce, distributions | `ParticleSystem` (`render/ParticleSystem.cpp`) |
| Fixed text centre | `TextPlan` (`core/TextPlan.cpp`) |

---

## Sections

### `[Options]`

Per-scene settings that override project-level defaults.

- **Syntax:** `[Options]` on its own line, followed by `key = value` pairs.
- **Names**: an option has one name, the same here and in the project's `schema.toml`, and case does not matter (`TextColour`, `textcolour` and `TEXTCOLOUR` are the same option). A value may be wrapped in quotes, which is needed for a font path with spaces.
- **Keys:**
    - `BackgroundColour` — background colour: any [colour](#colours). A scene that does not set it uses the project's `BackgroundColour`, which is black unless the project says otherwise.
    - `TextColour` — default colour of the scene's slave text: any [colour](#colours). Text with its own `colour(...)` keeps that colour. A scene that does not set it uses the project's `TextColour`, then white. Countdowns use it too; `textD` subtitle lines use `SubtitleColour` instead.
    - `FontSize` — slave font size in points. Overrides the project `FontSize`. `fontSize N` in `[Commands]` changes it for the text below it; see [`use` / `fontSize`](#use-font--fontsize-n).
    - `Font` — path (relative to project root, or a bare filename resolved against the shared `FONTS/` directory) to a `.ttf` font file. A bare filename is looked for in the project's `fonts/` and `font/` folders and the project folder first, then `FONTS/`; a relative path is now taken from the project folder.
    - `FontItalic`, `FontBold`, `FontBoldItalic` — paths to the italic / bold / bold-italic typeface variants. Used by `textbf`, `textit`, and inline `bold(...)` / `it(...)`. Empty or missing → falls back to project / global config, then to regular.
    - *Fonts are per scene*: a scene without `Font` (or one of the style faces) uses the project's, then the global one — never the previous scene's.
    - `Capitalize` — `true` or `false` to capitalize every text line.
    - `AudioFade` — `true` or `false`. When true, the fade times on a `play FILE FADE_IN, FADE_OUT` command ramp the clip's **audio** as well as its picture. Default **false**: the picture fades and the sound cuts at full volume. Like `Capitalize`, a scene can switch this on but cannot switch off a project that enabled it.
    - `FixedTextCentre` — `true` or `false`. Normally the centred text stack is centred on the lines shown so far, so every new line pushes the block up by half a line. With `true`, the block is centred on **every line it will hold before it is cleared**, so each line appears where it stays. See [Fixed text centre](#fixed-text-centre). A scene can switch it either way, overriding the project. Off in projects that do not set it; new projects are generated with it on.
    - `TransparentBackground` — `true` or `false`. With `true` the slave becomes **see-through**: everything SatyrAV draws (text, images, video, particles) shows as normal, and everywhere it draws nothing — the background, the clear parts of a PNG, the faded part of `trans(...)` — shows whatever is underneath the slave window on that screen (another program, a camera feed, the desktop). Text stays in front of everything, as always. A scene can switch it either way, overriding the project; default `false`. See [See-through slave](#see-through-slave).
    - `TextOutline` — outline thickness in points for this scene's text, `0` = no outline. Overrides the project's `TextOutline`.
    - `DisableTextOutline` — `true` or `false`. `true` switches the outline off for this scene, whatever `TextOutline` says. A scene can switch it either way: `false` brings the outline back in a project that disables it. Default `false`.
    - `<TYPE>.SPEED`, `<TYPE>.DENS` (or `.DENSITY`), `<TYPE>.X.DIST = NAME(p1, p2)`, `<TYPE>.VX.DIST`, `<TYPE>.VY.DIST` — per-particle-type tunables. See [Particle tunables in `[Options]`](#particle-tunables-in-options) for details.
- **Hex values:** `TextColour = #FF8800` works unquoted. A `#` straight after the `=` followed by 3 or 6 hex digits is the colour; any later `#` starts a comment as usual.
- **Example:**
    ```txt
    [Options]
    BackgroundColour = black
    TextColour = #FFD700      # gold
    FontSize = 30
    RAIN.SPEED  = 1.5
    RAIN.X.DIST = NORMAL(0.5, 0.1)
    ```

### `[Preload]`

Files to pre-decode/pre-cache before the scene runs, so the first `show` or `play` is instant.

- **Syntax:** `[Preload]` on its own line, one name or pattern per line. Quotes are allowed and needed for a name with `#`.
- **Names:** a file name, or a path with folders (`MEGA/Lyric/Sequence 1/1A.png`, spaces allowed, `/` or `\`). Looked up as everywhere else (`ProjectPaths::ResolveMedia`): the project folder, then `pictures/`, `movies/`, `sound/`; names that differ only in case are found too; an absolute path is used as it is.
- **Patterns** (`ProjectPaths::ExpandWildcard`): `*` matches any run of characters and `?` one, within one folder or file name; `**` matches any number of folders, none included. Case is ignored. `MEGA/*.png` is every PNG directly in `pictures/MEGA/`; `MEGA/**/*.png` every PNG in it and below. Each matching file is preloaded; a pattern that matches nothing is a scanner warning.
- **What is loaded:** pictures and GIFs become textures (`MediaCache`), fonts are opened. Videos need no entry — every `play` of a video in the scene and the next is preloaded by `MediaPlayer`. The cache is keyed on the resolved path, which `show` resolves the same way, so a preloaded picture is a cache hit.
- **Example:**
    ```txt
    [Preload]
    satyr-logo.png
    MEGA/Lyric/Sequence 1/1A.png
    MEGA/**/*.png
    ```

### `[Cue]`

Named operator notes, shown in the master window's **Cue Notes** column beside the command that references them. Nothing reaches the slave — these are stage-management notes, not audience-facing text.

- **Syntax:** `[Cue]` on its own line, then one note per line: `Name` followed by whitespace and the note text. A command claims a note by ending with `\Name`.
- **Names:** letters, digits and underscores (`Cue1`, `stage_left_2`).
- **Placement:** `[Cue]` may appear before or after `[Commands]` — references are resolved in a pre-pass, so section order does not matter.
- **Binding:** the reference attaches to the whole **navigator row**. On a compound (`clear & play x.mp3 \Cue1`) the note belongs to the row, not to one sub-command. A `run{ … }` collapses its body into one row, so a reference written on a command inside the body surfaces on that row.
- **Blocks:** a reference on a block's closing brace (`} \Cue1`) is dropped — notes bind to command rows. The brace still closes the block, so the rest of the scene parses normally. Put the reference on a command inside the body instead.
- **Wrapping:** a long note wraps onto as many rows as it needs, and its command's row grows to match so the two stay aligned while scrolling.
- **Warnings:** a note that nothing references, a duplicate name, or a name with no text each raise a parser warning, listed by the project scanner (`C`). The unreferenced-note warning is how a mistyped `\Name` surfaces — see the note on collisions below.
- **Collision with LaTeX symbols:** a reference is only recognised when the name is actually defined in `[Cue]`. `\alpha` and `\Cue1` have the same shape, and an undefined `\Name` is left alone for the [symbol expander](#latex-symbols). A `\Name` inside quotes is always text.
- **Example:**
    ```txt
    [Cue]
    Cue1 Play audio when Actor leaves the scene
    Cue2 Stop audio when Actor begin screaming from outside the scene

    [Commands]
    show backgroundImage.png
    play Music.mp3 \Cue1
    stop \Cue2
    ```

### `[Macro]`

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

### `[Hotkeys]`

Project-level audio hotkeys bound to `F1`–`F12`. Pressing the key during a performance plays the configured audio file via the standalone-audio path; pressing the **same** key again while its sound is still playing **stops** it (toggle behaviour, useful for long ambient cues).

- **Syntax:**
    ```toml
    [Hotkeys]
    F1  = "knock.mp3"
    F2  = "doorbell.mp3"
    F12 = "applause.mp3"
    ```
- **Resolver:** filenames resolve against the project's `sound/` folder (and the project root, `pictures/`, `movies/` as a fallback) — the same lookup `play` uses.
- **Notes:** Hotkeys are project-only. F-keys are ignored while a text field is focused (search field, creator field, etc.) so typing in those inputs doesn't fire a cue.
- **Help popup:** the `H` popup lists `F1-F12` once you're inside a project.

### `[Backstage]`

What the backstage screen shows about this scene. When the show moves into a new scene, SatyrAV sends the current scene and the next two to the revy's backstage ticker ([@fysikrevy/backstage-ticker](https://www.npmjs.com/package/@fysikrevy/backstage-ticker)), so the people backstage see who is on next.

- **Syntax** (in a `.ngk`; the header in any case):
    ```txt
    [Backstage]
    Name         = Scene 1
    ExpectedTime = 2m40s
    Actors       = ["Actor 1", "Actor 2"]
    Ninjas       = ["Ninja 1"]
    ```
- **Keys** (any case, all optional):
    - `Name` — shown as the scene's title. Default: the scene's file name without `.ngk`. Quote it if it contains `#`.
    - `ExpectedTime` — `2m40s`, `2m`, `40s`, `2:40`, `1:02:03`, or plain seconds (`160`). Sent as `m:ss`.
    - `Actors`, `Ninjas` — a list: `["A", "B"]`, `['A', 'B']`, `[A, B]`, or `A, B`. A list may run over several lines, TOML style, as long as the first line ends with `[`.
- **Turning it on** — in `schema.toml`:
    ```toml
    [Backstage]
    Enabled = true
    Url     = "https://backstage.texnik.dk/set-bck"
    ```
  New projects are generated with exactly this. Nothing is sent when the `Url` is empty or missing, or when `Enabled = false` (which keeps the `Url` for later). A missing `Enabled` counts as on. With no scheme the Url is `https://`; with no path it is `/set-bck`.
- **When it is sent:** when you press ENTER at *Press ENTER for next scene*, and when a cue runs in a scene the show was not in yet — the first scene of the night, or one you jumped to with the arrow keys. Later cues in the same scene send nothing, so it is one POST per scene.
- **What is sent:** the current scene and the next two scenes **that have a `[Backstage]` block**, in show order across akter, one line each:
    ```txt
    Scene 1;2:40;actors;Actor 1;Actor 2;ninjas;Ninja 1
    Scene 3;4:30;actors;Actor 6;Actor 7
    Scene 4;0:45;actors;Actor 1;ninjas;Ninja 4
    ```
  A scene without a block (an AV interlude) is skipped, and starting one sends nothing, so the backstage screen keeps the last real lineup. Empty `Actors` / `Ninjas` lists are left out. Near the end of the show fewer than three lines are sent.
- **Request:** `POST` with the header `sl-cmd: waiting` and a plain-text UTF-8 body, as the ticker expects.
- **Never blocks the show:** the request is made in the background with a 4-second limit and is not retried — the next scene change sends a fresh lineup anyway. If a newer lineup is ready before an older one went out, only the newest is sent.
- **Text the ticker cannot take:** `;` and line breaks would split a field and are removed. `<`, `>` and `&` are sent as they are, but the ticker shows text as HTML, so they raise a warning.
- **Warnings** in the project scanner (`C`): unknown keys, a time it cannot read, a line without `=`, and the characters above.
- **Watching it:** the Debug popup (`D`) has a *Backstage* line — green with the HTTP status when the last POST arrived, red with the reason when it failed, grey when off. Every request is in the SatyrAV log: what was sent, line by line, then the result.
- **Platforms:** https works in the Windows build (through Windows' own HTTP stack, including its proxy and certificate settings). The Linux build sends plain `http://` only.

### `[Animation]`

Named animations that repeat — a tumbleweed that rolls across the stage every few seconds, at a different height each time. Define them here, start one in `[Commands]` with `show NAME`.

- **Syntax:** `[Animation]` (or `[Animations]`, any case), then one block per animation: a name, `{`, `key = value` lines, `}`. The brace may also go on the line after the name.
- **Keys** (any case):

    | Key | Meaning |
    |---|---|
    | `show = …` | What moves: anything `show` takes — a file, or a file wrapped in `rotate()`, `spin()`, `trans()`, `scale()`, `size()`, `random()`, `randomImages()`, `layer()`… Or: |
    | `text = …` | Text that moves: anything `text` takes, including `colour()`, `bold()`, `it()`, `rotate()`, `spin()` and `+`. One of `show` / `text` is required. |
    | `path = P1, P2, …` | **Required.** Two or more points, as in `pathMove`: `[X, Y]` in percent or a side letter `N` / `S` / `E` / `W` (just off that edge). |
    | `duration = MS` | **Required.** Milliseconds for one pass along the whole path. |
    | `repeat = N` / `forever` | How many passes. Default `forever`. |
    | `for = SECONDS` | No new pass starts after this long. Optional. |
    | `every = MS` | Milliseconds from one pass's start to the next. Default: the duration, so passes run back to back. Shorter than the duration means passes overlap. |
    | `randomY = MIN, MAX` | Each pass crosses at a random height between `MIN` and `MAX`, in **slave pixels** (0 = top; 1080 = bottom on a 1080p slave). The whole path is moved up or down so that its middle line (50 %) lands there — for a `W, E` path, the content's centre runs along that pixel row. The pixels are the slave canvas, so the numbers mean the same in windowed dev mode. |
    | `randomX = MIN, MAX` | The same sideways, in slave pixels from the left edge, for `N, S` paths. |
    | `layer = N` | Draw order for an image, as `layer()`. |
    | `size = [W, H]` | Size of an image, as `size()`. |

- **Modifiers:** each pass is drawn exactly like the same `show` / `text` line with a `pathMove` on it, so every modifier works the same way. `randomImages()` and `random()` pick again for every pass (a different picture or size each time). `pos()` and `move()` are overruled by `path`. `particle()` can't follow a path or end with a pass, so it is ignored with a warning and the image itself moves.
- **Ending:** the animation ends by itself when `repeat` passes have started or `for` has run out; the pass on screen finishes its run.
- **Stopping it yourself** — soft (no new passes, the one on screen finishes) or hard (gone at once):

    | Command | Effect |
    |---|---|
    | `stop NAME` | Soft, that animation. |
    | `clear`, `clearImages` | Soft, every animation. |
    | `clear NAME` | Hard, that animation. |
    | `clearAll`, `BACKSPACE`, next scene | Hard, every animation. |

    `clearText` and plain `stop` leave animations alone. `show NAME` while it runs starts it over (count and `for` from zero).
- **Names:** an animation name used with `show`, `stop` or `clear` means the animation, even if a file or group has the same name.
- **Mistakes** (missing `path`, an unknown key, `repeat = lots`) are listed by the project scanner (`C`), which also checks that each animation's image exists. A block without `show`/`text`, `path` or `duration` is skipped.
- **Example:**
    ```txt
    [Animation]
    Tumbleweed{
        show     = spin(240, size([150, 150], tumbleweed.png))   # rolls as it goes
        path     = W, E
        duration = 3000
        repeat   = 5
        every    = 4000
        randomY  = 650, 950    # low on a 1080p stage
    }

    [Commands]
    show Tumbleweed      # five spinning tumbleweeds, one every 4 s, low on the stage
    stop Tumbleweed      # ...or fewer: the one rolling finishes
    ```

### `[Commands]`

The sequence of cues. Each line is one cue; the operator presses ENTER to advance.

---

## Executable commands

### `text`

- **Syntax:** `text "MESSAGE"` — or `text SEGMENT + SEGMENT + ...`
- **Arguments:** `MESSAGE` — quoted string. `\n` inside the string inserts a line break (each line becomes its own entry in the stack).
- **Behaviour:** Appends a text line onto the slave's text stack. Multiple `text` commands stack vertically, centred by default unless a `pos()` / `move()` modifier overrides. With the `+` operator, multiple styled segments render side-by-side on a single line — each segment can wrap its own `colour(...)` / `trans(...)` (see [the `+` operator](#-concatenation)).
- **Output:** The message appears on the slave screen with the current font, colour, and outline.
- **Example:**
    ```txt
    text "Welcome!"
    text "Line 1\nLine 2"
    text colour(RED, "Hello, ") + colour(BLUE, "World!")
    ```

### Fixed text centre

How the centred text stack is placed when `FixedTextCentre = true` (in `schema.toml`'s `[Options]`, or a scene's).

- **Without it**: the stack is centred on the lines currently on screen. A second line moves the first up by half a line, a third moves both up again, and so on.
- **With it:** when a centred line appears on an empty screen, SatyrAV looks ahead in the scene and counts every centred line still to come before the next `clear`, `clearText` or `clearAll` (or the end of the scene), and centres the block on all of them. The first line appears at the top of the finished block, and no line moves as the rest appear.
- **What counts:** `text`, `textbf`, `textit`, `typewrite`, and the main line of `textD`, including the lines of a `\n` break and lines that word-wrap (the planned lines are measured exactly like the real ones). Text placed with `pos(...)`, `move(...)` or `pathMove(...)`, `textD`'s subtitle line, and `textCont` (which adds to the last line) are not part of the stack, as before.
- **What ends a block:** `clear` without a name, `clearText`, `clearAll`, and the end of the scene. `clear NAME` only removes one group, so the block carries on. Cues inside `run{ … }`, `loop(…){ … }`, a `macro`, or a compound `&` count in the order they fire — so in `clear & text "A"` the clear ends the old block and `"A"` starts the new one.
- **If more lines appear than were planned** — after jumping around in the cue list, say — the block grows the old way rather than overflowing.
- **Example:** with this scene, `"One"` appears where it sits when all three lines are up:
    ```txt
    [Commands]
    text "One"
    text "Two"
    text "Three"
    clear
    ```

### See-through slave

`TransparentBackground = true` (in a scene's `[Options]` or in `schema.toml`) makes the slave see-through wherever nothing is drawn.

- **The window:** a see-through slave needs a window made for it, and that can only be chosen when the window opens. SatyrAV opens it see-through when the project opens if the project, or any scene in it, uses `TransparentBackground = true`; other projects keep the normal window. If you add the option to a scene during a show with `U`, leave the project and open it again — until then that scene shows black where it would be see-through, and the log says so.
- **Per scene:** with the see-through window open, scenes that don't ask for it look exactly as before (their background colour, black outside the targeted rect). Scenes that do ask are see-through everywhere nothing is drawn — **including outside the targeted rect**, which is normally painted black.
- **What shows through** is whatever is on that screen under the slave window. Put the other program on the slave's screen first, then open the project.
- **Capture mirror:** a capture card has no transparency, so the see-through parts are black on the capture mirror.
- **Performance (Windows):** a see-through window uses an older, slightly slower way of presenting frames, which is why it is only opened for projects that use it. On laptops with two graphics cards it may not work at all (an SDL limitation); the slave then shows black there.
- **Example:**
    ```txt
    [Options]
    TransparentBackground = true

    [Animation]
    Tumbleweed{
        show     = spin(240, size([150, 150], tumbleweed.png))
        path     = W, E
        duration = 3000
        randomY  = 650, 950
    }

    [Commands]
    show Tumbleweed    # rolls across whatever is on the screen underneath
    ```

### `textCont`

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

### `textbf` / `textit`

- **Syntax:** `textbf "MESSAGE"` and `textit "MESSAGE"` — both support the `+` operator and inline modifiers identically to `text`.
- **Behaviour:** Identical to `text` but stamps a default style onto every produced run — `textbf` sets `bold`, `textit` sets `italic`. Per-segment inline `bold(...)` / `it(...)` wrappers OR onto the line default, so `textbf "A " + it("B")` renders A as bold and B as bold-italic.
- **Font requirement:** Real italic / bold typeface files must be configured (in `config.toml` `[font]`, in `[Options].FontItalic`/`FontBold`/`FontBoldItalic`, or via the bundled DejaVu Sans family that ships in `assets/fonts/`). If a slot isn't loaded, the renderer falls back via bold-italic → bold → italic → regular so the line still appears, just without the missing emphasis.
- **Example:**
    ```txt
    textbf "Important!"
    textit "Whispered aside"
    textbf "Heads up: " + it("Don't miss this.")
    ```

### `textbfCont` / `textitCont`

- **Syntax:** `textbfCont "MESSAGE"` and `textitCont "MESSAGE"`.
- **Behaviour:** Same as `textCont` (append to the most recently pushed text entry, falling back to a new line if there's nothing to extend) but the appended runs inherit the keyword's bold / italic style.
- **Example:**
    ```txt
    text "The verdict is "
    textbfCont "guilty."
    # Slave shows: The verdict is guilty.   (with "guilty." in bold)
    ```

### `textD "PRIMARY", "SECONDARY"`

- **Syntax:** Two quoted strings separated by a comma. Whitespace around the comma is irrelevant. Either side accepts the full `+` / `bold(...)` / `it(...)` / `colour(...)` machinery.
- **Behaviour:** Pushes the primary text as a normal centred-stack entry (identical to `text`) **and** pushes the secondary text as a separate bottom-anchored subtitle with project-configured italic / colour / transparency / position defaults applied. Intended for bilingual revues — the primary line is the language the audience hears, the secondary line is the translation.
- **Single-arg form:** if only one quoted string is given, `textD` collapses to plain `Text` semantics (no subtitle entry pushed).
- **Subtitle stacking:** multiple `textD` cues stack — the most-recently pushed subtitle sits at the configured `SubtitlePosY`; older subtitles move one line up each. `clearText` / `clear` / `clearAll` clear subtitles along with regular text since they share the same entry list.
- **Project defaults** (in `[Options]`):
    - `SubtitleItalic` — bool, default `true`.
    - `SubtitleColour` — any [colour](#colours), default `#AAAAAA` (light grey). `grey` here means that same `#AAAAAA`.
    - `SubtitleTransparency` — percent (0 = opaque, 100 = invisible), default `0`.
    - `SubtitlePosY` — percent of canvas height where the bottom edge of the newest subtitle sits, default `90.0`.
- **Per-run override:** inline modifiers inside the secondary string still win — e.g. `colour(YELLOW, "Watch out!")` keeps the project's italic default but overrides the colour.
- **Example:**
    ```txt
    textD "Vi har started nu!", "We have started now!"
    textD "Mor er hjemme.",      "Mum is home."
    textD "Tak for i aften!"
    # The single-arg call behaves like `text` — no subtitle.
    ```

### `clear`

- **Syntax:** `clear`
- **Arguments:** none — but see the variants below.
- **Behaviour:** Clears the slave's text stack, current image, and active particle system. Ungrouped content only — groups stay (see `clear NAME` and `clearAll`). Countdowns go too. Running [animations](#animation) start no new passes; the ones on screen finish.
- **Output:** The slave goes back to its background colour (or video, if one is playing).
- **Example:**
    ```txt
    clear
    ```

### `clearText`

- **Syntax:** `clearText`
- **Behaviour:** Removes all text — grouped too, and `textD` subtitle lines — except the passes of running [animations](#animation). Images, particles and countdowns stay.
- **Example:**
    ```txt
    clearText
    ```

### `clearImages`

- **Syntax:** `clearImages`
- **Behaviour:** Removes every image, grouped or not, and stops every particle system (those already smooth-stopping finish). Running animations start no new passes; the ones on screen finish. Text and countdowns stay.
- **Example:**
    ```txt
    clearImages
    ```

### `clear NAME`

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

### `clearAll`

- **Syntax:** `clearAll`
- **Behaviour:** Removes text, images, particles and countdowns, **including every group**, and cuts running animations at once. A full reset of the slave content. Video and sound keep playing. `BACKSPACE` does the same without a cue.
- **Example:**
    ```txt
    clearAll
    ```

### `show`

- **Syntax:** `show FILE` or `show MODIFIER(..., FILE)`
- **Arguments:** `FILE` — image file name or path with folders, spaces allowed (resolved against the project root, then `pictures/`, `movies/`, `sound/`; case-insensitive fallback). Modifiers wrap around the filename.
- **Supported formats:** `.png`, `.jpg`/`.jpeg`, `.bmp`, `.webp`, and `.gif`. Animated `.gif` files cycle through their frames at the delays encoded in the file — see the note below.
- **Behaviour:** Adds the image to the slave. Images stack: a new `show` goes on top of the ones already there (see [`layer()`](#layern-content) to choose the order). `clear` removes ungrouped images, `clearImages` all of them.
- **Output:** The image is drawn into the targeted slave rect, fitted to target dimensions unless `size()` / `scale()` overrides.
- **Animated GIFs:** Every frame is pre-decoded to a texture at preload / first-show time; the slave then advances the current frame by wall time. All position, rotation, spin, move, size and transparency modifiers apply to the whole animation. GIFs used as particles (`particle(..., anim.gif)`) currently render frame 0 only.
- **Example:**
    ```txt
    show satyr-logo.png
    show pos([50, 20], size([200, 200], icon.png))
    show dancing-satyr.gif
    ```

### `play`

- **Syntax:** `play FILE` — or `play FILE FADE_IN_MS, FADE_OUT_MS`
- **Arguments:**
    - `FILE` — audio or video file. Extension determines which.
    - `FADE_IN_MS`, `FADE_OUT_MS` — optional integer milliseconds for a fade-from-black on start and a fade-to-black on end. `0` on either side disables that side of the fade. Bare `play FILE` is equivalent to `play FILE 0, 0`. By default the fade is visual only and the sound cuts. Set the `audioFade` option (scene) or `AudioFade` (project) to fade a video's sound together with its picture.
- **Behaviour:**
    - Video (`.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`) — opens the hardware-decoded video pipeline and renders frames into the slave beneath text. The fade pair is applied as a per-frame brightness multiplier on the video texture; 0 % brightness is solid black, 100 % is the decoded frame untouched.
    - Audio (`.mp3`, `.wav`, etc.) — plays in the background. Several tracks can play at once, and over a video's own sound. Playing a file that is already playing restarts it. Fade pair is ignored.
- **Example:**
    ```txt
    play intro.mp3
    play background.mp4
    play background.mp4 1000, 2000   # 1 s fade-in, 2 s fade-out
    ```

### `stop`

- **Syntax:** `stop`, `stop FILE`, or `stop NAME` for an [animation](#animation) — no new passes, the one on screen finishes.
- **Arguments:** Optional filename. With no argument, stops all playback (video and every sound) and particle systems. With a filename, stops only that audio track.
- **File names** resolve the same way as `play`: against the project folder, then `pictures/`, `movies/` and `sound/`, so `stop music.mp3` stops the sound that `play music.mp3` started. Other sounds keep playing.
- **Note:** Particle systems that are already in smooth-stop mode (see `stopParticleCont`) are **not** affected by `stop`/`clear`/`clearAll` — their endings always play out to completion.
- **Example:**
    ```txt
    stop
    stop music.mp3
    ```

### `stopParticleCont`

- **Syntax:** `stopParticleCont` or `stopParticleCont GROUP`
- **Arguments:** Optional group name. With no argument, applies to ungrouped particle systems; with a group name, applies to that group only.
- **Behaviour:** Stops new particles from spawning, but lets every currently-alive particle finish its animation naturally — the system deactivates itself once the last live particle dies. Once issued, the smooth-stop is sticky: subsequent `stop`, `clear`, `clear NAME`, or `clearAll` commands leave the ending alone.
- **Multiple ungrouped systems :** Ungrouped `show particle(...)` calls now coexist — starting a second one no longer replaces the first. `stopParticleCont` with no group drains every ungrouped system together; `clear` and `stop` still clear all ungrouped systems at once (smooth-stopping ones survive per the sticky rule above).
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

### `countdown`

- **Syntax:** `countdown(SECONDS)` — or `countdown(SECONDS, "LANG")`
- **Arguments:**
    - `SECONDS` — integer from `0` to `3599` (= `59:59`). Values outside the range clamp into it.
    - `LANG` — optional language tag: `"en"` → prefix `"Countdown: "`, `"da"` → prefix `"Nedtælling: "`. Any other value (or omitting the argument entirely) renders only the digits — useful when the countdown is meant as a neutral overlay without an explicit label.
- **Behaviour:** Pushes a live timer onto the slave. The display ticks down once per second in real time, formatted as zero-padded `MM:SS` (e.g. `02:00`, `00:05`). When the timer reaches `00:00` it **holds at zero** until cleared — the operator decides when to advance. Multiple `countdown(...)` calls stack vertically at the top of the slave in the order they were pushed.
- **Clear semantics:** `clear` (no name), `clearAll`, and the new `clearCountdown` all remove every countdown. `clearText` and `clearImages` deliberately do NOT touch them, so a timer can survive routine text refreshes during the scene.
- **Output:** Centred horizontally at the top of the canvas (≈ 8 % from the top edge), at the project font size, in the scene's `TextColour` *(white unless set)*.
- **Example:**
    ```txt
    countdown(300, "da")     # "Nedtælling: 05:00" → ticks to "00:00"
    countdown(120, "en")     # "Countdown: 02:00"
    countdown(60)            # just "01:00"
    ```

### `clearCountdown`

- **Syntax:** `clearCountdown`
- **Behaviour:** Removes every active countdown. Text, images, particles, and groups are untouched. Use when you want to dismiss a timer without disturbing the rest of the scene.
- **Example:**
    ```txt
    countdown(60, "en")
    # … operator advances cues, intermission ends early …
    clearCountdown
    ```

### `typewrite`

- **Syntax:** `typewrite(MS_PER_CHAR, TEXT)`
- **Arguments:**
    - `MS_PER_CHAR` — milliseconds between revealed characters. `0` reveals everything at once (identical to `text`); values are clamped to `0`–`5000`.
    - `TEXT` — the **full text expression**, exactly as `text` accepts it: quoted strings in any of the three delimiters, `+` concatenation, and every modifier (`colour`, `bold`, `it`, `trans`, `pos`, `move`, `spin`, …). The split between the two arguments is on the first **structural** comma, so a comma inside `colour(RED, "a, b")` stays part of the text.
- **Behaviour:** Pushes an ordinary text line that reveals itself one character at a time from the moment the cue fires. In every other respect it *is* a text line — it stacks in the same centred layout, word-wraps the same way, honours groups, and is removed by the same `clear` / `clearText` / `clearAll`.
- **Behaviour:** Revealing is **character-based, not byte-based**, so LaTeX symbols and `æøå` are never cut mid-character.
- **Behaviour:** A multi-line `typewrite` (one containing `\n`) reveals in reading order — the second line does not begin until the first has finished.
- **Cue advance:** Pressing ENTER for the next cue **snaps any still-typing line to fully typed** before the next command runs. An operator who advances early never leaves a half-written word on the slave.
- **Output:** Identical to `text` once complete. While typing, the line grows and re-centres as characters appear.
- **Example:**
    ```txt
    typewrite(45, "The letter arrived at dawn...")
    typewrite(80, colour(RED, "DANGER") + " — do not open")
    typewrite(30, pos([50, 80], "subtitle style reveal"))
    typewrite(0, "instant — same as text")
    ```

### `use FONT` / `fontSize N`

Change the font or the font size of the text **below** this line in the scene file.

- **Syntax:** `use FONT`, `use default`, `fontSize N`, `fontSize default`. `FONT` may be quoted, which is needed for a name with spaces.
- **Behaviour:** Neither is a cue. Both show in the navigator as a dimmed comment row (`# Using font Fancy.ttf`, `# Font size 90`) and navigation skips them, so you never press ENTER for them. Each one applies to every text cue after it in the file — `text`, `textCont`, `textbf`/`textit`, `typewrite`, the first part of `textD` — until the next `use` (or `fontSize`), whichever way you get to the cue (ENTER, arrow keys, SHIFT+ENTER, `U`).
- **Scope:** One scene file. Every scene starts on its own font and size from `[Options]` (then `schema.toml`, then the global setting), and nothing carries into the next scene.
- **`default`:** `use default` goes back to the scene's font; `fontSize default` goes back to the scene's `FontSize`, then the project's, then the global one.
- **Mixing on one line:** `textCont` after a `use` or `fontSize` adds to the same line in the new font or size. The line gets as tall as its biggest part, and every part sits on the same baseline.
- **Where fonts are found:** a bare file name is looked for in the project's `fonts/` folder, then `font/`, then the project folder, then the shared `FONTS/` folder. A path with a folder in it is taken from the project folder; a full path is used as it is. A font that cannot be found is logged and the scene's font is used instead.
- **Bold and italic:** a `use` font is one file, so `textbf`, `bold(...)` and friends in it get a computed bold / slant rather than a separate face.
- **Preloading:** every font and size a scene uses is opened when the scene is entered. Listing the font in `[Preload]` is allowed but not needed.
- **Bad values:** `fontSize` takes 1 to 1000. Anything else (`fontSize big`) is shown as an ignored comment, changes nothing, and is listed by the project scanner (`C`).
- **Example:**
    ```txt
    [Commands]
    use Fancy.ttf
    text "Hello, "
    use default
    fontSize 110   # just for the punchline
    textCont "World!"
    fontSize default
    text "Back to normal"
    ```

### `comment`

- **Syntax:** `comment "TEXT"`
- **Behaviour:** Displays `# TEXT` in the master navigator as a dimmed row. **Not executable** and **not selectable** — navigation skips over it. Does not consume a line number in the gray index column.
- **Output:** Only the operator sees the comment; nothing is shown on the slave.
- **Example:**
    ```txt
    comment "Verse 1 — keep intensity low"
    text "Det er revytid"
    ```

### `macro NAME`

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
- **Behaviour:** Only a **structural** `&` separates commands — one that is outside every quoted string and outside every modifier argument list. An `&` in displayed text is literal, so `text "Tom & Jerry"` is one text line.
- **Example:**
    ```txt
    clear & text "Next slide" & play sting.mp3
    text "Tom & Jerry"                        # one command, ampersand shown
    clear & text colour(RED, "Salt & Pepper")  # one separator, one literal
    ```

### `run{ ... }`

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

### `group NAME{ ... }`

- **Syntax:**
    ```txt
    group NAME{
        commands...
    }
    ```
- **Arguments:** `NAME` — identifier used later with `clear NAME`.
- **Behaviour:** Every command inside the body is parsed just like at the top level — plain lines, `run{}`, `loop(){}`, and `macro NAME` all work — and each one becomes its own navigator row. The group block adds *no* cueing behaviour; its sole effect is to tag every piece of slave content the body produces with the group name. Wrap the body in `run{}` (as in the example below) if you want everything to fire on a single key press. Plain `clear` leaves groups alone; `clearText` and `clearImages` remove grouped text and images too; `clear NAME` removes one group, `clearAll` everything.
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

### Colours
 Every colour in SatyrAV — the `colour(...)` modifier and the `BackgroundColour`, `TextColour` and `SubtitleColour` options — accepts the same values:

| Name | Value | | Name | Value |
|---|---|---|---|---|
| `black` | `#000000` | | `orange` | `#FFA500` |
| `white` | `#FFFFFF` | | `purple` | `#800080` |
| `red` | `#FF0000` | | `pink` | `#FF69B4` |
| `green` | `#00FF00` | | `cyan` | `#00FFFF` |
| `blue` | `#0000FF` | | `lime` | `#32CD32` |
| `yellow` | `#FFFF00` | | `grey` / `gray` | `#808080` |

- **Hex:** `#RRGGBB`, or the short `#RGB`, where each digit is doubled (`#F80` = `#FF8800`).
- Names and hex digits are case-insensitive.
- **One exception:** `SubtitleColour = "grey"` is the lighter `#AAAAAA` it has always been, so existing subtitles do not change shade.

### `colour(COLOUR, content)`

- **Arguments:** any [colour](#colours).
- **Example:** `text colour(RED, "Warning!")`
- **Example:** `text colour(#FF5500, "Custom colour")`
- Without `colour(...)`, text uses the scene's [`TextColour`](#options), which is white unless set.

### `trans(PERCENT, content)`

- **Arguments:** `PERCENT` — 0 (opaque) … 100 (invisible).
- **Example:** `show trans(50, image.png)`

### `bold(content)`

- **Behaviour:** Marks a single text segment as bold. Composes with `colour(...)`, `trans(...)`, and `it(...)` like any other modifier — order doesn't matter. Only meaningful on `text` / `textCont` / `textD` runs (or their styled variants); ignored elsewhere.
- **Example:** `text "Plain " + bold("BANG!") + " then plain again"`

### `it(content)`

- **Behaviour:** Italic counterpart of `bold(...)`. The short name matches the LaTeX-flavoured `\it` spelling and keeps `+` lines compact.
- **Stacking:** `bold(it("X"))` (or `it(bold("X"))`) renders bold-italic.
- **Example:** `text "He whispered " + it("very softly") + " behind her."`

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

### `scale(content, [W, H])`

- **Arguments:** `content` first, then `[W, H]` in pixels.
- **Behaviour:** Alias of `size()` with the argument order flipped to match `random()`. Both modifiers set the same underlying dimensions; use whichever order you prefer.
- **Example:** `show scale(logo.png, [400, 400])`

### `random(FILE, [MIN_W, MIN_H], [MAX_W, MAX_H], KEEP_ASPECT)`

- **Arguments:** `FILE` — image; min/max — pixel bounds; `KEEP_ASPECT` — `true` or `false`.
- **Behaviour:** Each particle spawn picks a random size in the given range.
- **Example:** `show particle(RAIN, 50, random(heart.png, [50,50], [200,200], true))`

### `particle(TYPE, [INTENSITY,] content)`

- **Arguments:** `TYPE` — `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`, `OSCILLATION`, `ORBIT`, `NOISE`, `LIFECURVE`; `INTENSITY` — 1-100 (default 100).
- **Behaviour:** Starts a particle system using the wrapped image (or randomised image via `randomImages()`). Motion model per type:
    - `RAIN` — near-vertical fall, slight X jitter.
    - `SNOW` — slower fall with sinusoidal sway and rotation.
    - `CONFETTI` — medium fall speed with strong rotation.
    - `RISE` — spawns at the bottom and rises.
    - `SCATTER` — radial burst from screen centre, fades with lifetime.
    - `BROWNIAN` — spawns anywhere; velocity drifts via random-walk kicks with mild damping. No gravity. Lifetime-linked alpha fade.
    - `OSCILLATION` — sinusoidal X around a per-particle anchor column, linear Y descent. Each particle has its own amplitude, frequency, and phase so the swarm doesn't move in lock-step.
    - `ORBIT` — circular motion around the screen centre at a random radius and angular velocity per particle. Direction is random (cw/ccw).
    - `NOISE` — velocity steered by a 2D value-noise flow field sampled at the particle's position; nearby particles follow similar currents.
    - `LIFECURVE` — linear motion with a triangular scale/alpha envelope (grow + fade in, shrink + fade out).
- **Example:** `show particle(SNOW, 30, snowflake.png)`
- **Example :** `show particle(ORBIT, heart.png)`

### `trail(N, content)` / `trail(N, MODE, content)`

- **Arguments:**
  - `N` — integer count of past frames kept per particle (clamped to 0–64; 0 disables).
  - `MODE` *(optional)* — `GHOST` (default), `STRETCH`, or `GLOW`.
- **Behaviour:** Each particle keeps a ring buffer of its last `N` transforms (position, rotation, scale, alpha) plus an `age` per sample. New samples are pushed only after the particle has moved a small distance, so slow particles don't stack copies on themselves. Samples expire when `age` exceeds `max(0.5, N * 0.08)` seconds.
  - `GHOST` — fade-and-shrink past samples behind the live particle (exponential alpha decay + linear age-shrink scale). Default.
  - `STRETCH` — single elongated quad per particle, rotated along the vector from the oldest sample to the live position. Reads as real motion blur. Best with `NOISE`, `OSCILLATION`, `ORBIT`.
  - `GLOW` — same geometry as `GHOST`, drawn with `SDL_BLENDMODE_ADD` for a bloom look. Bright sprites stack into a glow; dark sprites mostly disappear.
- **Example:** `show particle(NOISE, trail(12, star.png))`
- **Example:** `show particle(NOISE, trail(12, STRETCH, star.png))`
- **Example:** `show particle(ORBIT, trail(20, size([40,40], heart.png)))`

### `trailGlow(N, content)`

- **Arguments:** `N` — same as `trail`.
- **Behaviour:** Sugar for `trail(N, GLOW, content)`. Past samples are drawn with additive blending so overlapping trails accumulate brightness. Live particle still draws with normal alpha blend.
- **Example:** `show particle(NOISE, trailGlow(16, star.png))`

### Particle tunables in `[Options]`

The scene `[Options]` block accepts per-type tunables using the `<TYPE>.<KEY>` convention. Project-level defaults live under `[Particle.<TYPE>]` tables in `schema.toml`. The cascade is scene → project → built-in defaults, and only fields the author explicitly set override the level below.

**Keys:**
- `<TYPE>.SPEED = f` — scalar multiplier on spawn velocity (and on angular velocity / oscillation frequency for the formula-driven types). Default `1.0`, clamped to `[0.01, 20]`.
- `<TYPE>.DENS = f` (or `<TYPE>.DENSITY`) — scalar multiplier on spawn rate. `2.0` doubles the density. Default `1.0`, clamped to `[0.01, 20]`.
- `<TYPE>.X.DIST = NAME(p1, p2)` — distribution used when picking the spawn X for types that randomise X (`RAIN`, `SNOW`, `CONFETTI`, `RISE`, `BROWNIAN`, `OSCILLATION`, `NOISE`, `LIFECURVE`). Formula-positioned types (`ORBIT`, `SCATTER`) ignore it.
- `<TYPE>.VX.DIST = NAME(p1, p2)`, `<TYPE>.VY.DIST = NAME(p1, p2)` — distributions that reshape how each motion model samples its spawn velocity within its built-in `[vmin, vmax]` range. `NORMAL(0.5, 0.1)` on `RAIN.VY` clusters fall speeds near the range midpoint; the default `UNIFORM` reproduces 1.4 behaviour exactly. Formula-driven types (`ORBIT`, `NOISE`) ignore these; `SCATTER` derives vx/vy from a polar sample and also ignores them. Params are in `[0, 1]` (mapped into the range), matching `X.DIST`.

**Distributions:** `UNIFORM` (default), `NORMAL(mean, stddev)`, `LOGNORMAL(mu, sigma)`, `BINOMIAL(n, p)`, `BERNOULLI(p)`, `GAMMA(shape, scale)`. Parameters are in the normalised `[0, 1]` screen-fraction space — e.g. `NORMAL(0.5, 0.1)` clusters spawns around the horizontal centre with ~10 % screen stddev.

**Scene example:**
```ini
[Options]
RAIN.SPEED     = 1.5
RAIN.DENS      = 2.0
RAIN.X.DIST    = NORMAL(0.5, 0.1)
RAIN.VY.DIST   = NORMAL(0.2, 0.1)    # slow-skewed rainfall
SNOW.VX.DIST   = NORMAL(0.5, 0.3)    # wider sway
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
VYDist   = "normal"    # VYDistP1 = 0.2
VYDistP2 = 0.1
```

### `bounce(BOOL, content)`

- **Arguments:** `BOOL` — `true` or `false` (or `1` / `0`).
- **Behaviour:** When `true`, each particle reflects off the four screen edges. The reflected velocity component loses 20 % energy per hit. Applied uniformly across every particle type, including formula-driven types like `ORBIT`/`OSCILLATION` — for those, bounce acts as a soft clamp.
- **Gravity + settling :** Enabling `bounce(true, …)` also switches on a constant downward gravity (≈ 900 px/s²) for every integrating type (`RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`, `OSCILLATION`, `LIFECURVE`). Particles accelerate, bounce off the floor, lose energy each hit, and once both |vx| and |vy| drop below the rest threshold they lock in place — forming a visible pile. Settled particles skip physics but still tick their lifetime, so the pile fades out as droplets expire naturally (use `clear` or `stopParticleCont` to tear it down). `ORBIT` and `NOISE` are formula-driven and gravity does not affect them.
- **Example:** `show particle(BROWNIAN, bounce(true, heart.png))`
- **Example :** `show particle(RAIN, 50, bounce(true, droplet.png))` — droplets accelerate, bounce off the floor, and pile up.

### `randomImages([LIST])` and `randomImages(FOLDER)`

- **Arguments:** a comma-separated list, or a folder under `pictures/` (`faces`, or a deeper one: `MEGA/Lyric`).
- **Behaviour :** Each particle spawn picks a *new* random image from the list/folder, so a rain of `randomImages(professorPhotos)` drops many different photos, not the same one repeatedly.
- **Example:** `show particle(RAIN, 50, randomImages(professorPhotos))`

### `move(FROM, TO, DURATION_MS, content)`

- **Arguments:**
    - `FROM` / `TO` — either `[X, Y]` percent coords or a side letter (`N`, `S`, `E`, `W`) meaning "just off that edge of the targeted rect".
    - `DURATION_MS` — milliseconds for the slide. Content holds at `TO` after that.
- **Behaviour:** Linear slide from `FROM` to `TO` over the duration, then static.
- **Example:** `show move(W, [50,50], 800, logo.png)`

### `pathMove(P1, P2, …, PN, DURATION_MS [, MODE], content)`

- **Arguments:**
    - `P1 … PN` — two or more waypoints, each in `move()`'s endpoint vocabulary: `[X, Y]` percent coords or a side letter (`N`, `S`, `E`, `W`).
    - `DURATION_MS` — milliseconds for the **whole path**, split evenly across the `N-1` legs. A short leg therefore travels more slowly than a long one; the timing is per-leg, not constant-speed.
    - `MODE` *(optional)* — `once` (default), `loop`, or `pingpong`. Case-insensitive.
- **Behaviour:** Moves the content through each waypoint in turn.
    - `once` — traverse the path and hold at the final waypoint (same end state as `move()`).
    - `loop` — on reaching the end, jump back to the first waypoint and go again.
    - `pingpong` — on reaching the end, travel back along the path in reverse, then forward again.
- **Behaviour:** Works on text as well as images, exactly like `move()`. In a `+` concatenation it is a line-level modifier, so the leftmost one wins.
- **Malformed specs degrade gracefully:** fewer than two waypoints, a missing or non-positive duration, or an unrecognised `MODE` leaves the content **unanimated but still displayed**, rather than swallowing the cue. An unknown mode word is treated as a typo rather than silently meaning `once`.
- **Example:**
    ```txt
    show pathMove([0,50], [50,90], [100,50], 3000, logo.png)      # arc across, once
    show pathMove([10,10], [90,10], [90,90], [10,90], 4000, loop, logo.png)   # square circuit
    text pathMove([20,50], [80,50], 2000, pingpong, "drifting")   # back and forth forever
    show pathMove(W, [50,50], E, 2500, logo.png)                  # in from the left, out to the right
    ```

### `layer(N, content)`

- **Arguments:** `N` — integer draw order. Higher numbers draw **in front**; negative values are legal and sit behind unlayered content.
- **Behaviour:** Controls the depth of images relative to each other. The default is `0`, and images on the same layer keep the order they were shown in — so without `layer()` the most recent `show` is on top.
- **Scope:** Images only. `layer()` on `text` or on a particle system parses but has no effect — text is always drawn above the image layer.
- **Example:**
    ```txt
    show layer(-1, backdrop.png)   # behind everything
    show sun.png                   # default layer 0
    show layer(5, foreground.png)  # always in front, even though it was shown first
    show layer(5, curtain.png)     # same layer — ties break by show order, so this is on top of foreground
    ```

### `spin(DEG_PER_SEC, content)`

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

### `+` concatenation

- **Syntax:** `text SEGMENT + SEGMENT + ...` (also works inside `textCont`).
- **Arguments:** each `SEGMENT` is any expression you would otherwise pass to `text` — a bare quoted string, or a modifier chain wrapping one.
- **Behaviour:** Splits the argument at top-level `+` tokens (respecting quotes, parens, and brackets), parses every segment independently, and renders them side-by-side on a single line.
    - **Per-segment modifiers:** `colour(...)` and `trans(...)` apply only to their own segment — adjacent segments can therefore differ in colour or transparency.
    - **Line-level modifiers:** `pos(...)`, `move(...)`, `rotate(...)`, `spin(...)`, and `size(...)` act on the whole line. When two segments both try to set the same line-level modifier, **the leftmost wins** — the value from segments further right is silently ignored. Put line-level modifiers on the first segment when possible.
- **Example:**
    ```txt
    text colour(RED, "Hello, ") + colour(BLUE, "World!")
    text pos([20, 80], "Warning: ") + colour(RED, "engine overheating")
    # pos from the first segment wins; a pos on the second segment would be ignored.
    ```

---

## Line endings and comments

- `\n` inside a quoted `text` string produces a new text entry.
- `#` starts a line/end-of-line comment — everything after it on the line is discarded. Two exceptions, both cases where the `#` is data rather than a comment marker:
    - `#` inside a quoted string is kept verbatim, so `text "Item #1"` renders the `#` literally.
    - `#` **inside a modifier argument list** (i.e. between `(` and its matching `)`) is kept verbatim, so `text colour(#FF5500, "Warm")` passes the hex colour through. Parentheses are only counted outside strings, so a `)` in displayed text — `text "smile :)"` — cannot confuse the rule. A comment that merely starts with something hex-shaped, such as `#FFFFFF looks nice`, is still a comment.
- Blank lines are ignored.

## LaTeX symbols

Displayed text accepts LaTeX-style macros for Greek letters and common mathematical symbols. They are expanded at parse time, so the master navigator, the word-wrap, and the slave all deal in real characters. Expansion applies to every text-bearing command — `text` and its styled/`Cont` variants, both sides of `textD`, `typewrite`, each segment of a `+` concatenation, and `comment` rows — but **never** to filenames, group names, or section keys.

```txt
text "Let \varepsilon > 0 and pick \delta \in \mathbb{R}"
text "Energy: E = mc\Delta \approx 3 \times 10^8"
textbf "\forall x \in \mathbb{N} : x \geq 0"
```

**Greek, lower case** — `\alpha \beta \gamma \delta \epsilon \zeta \eta \theta \iota \kappa \lambda \mu \nu \xi \omicron \pi \rho \sigma \tau \upsilon \phi \chi \psi \omega`

**Greek variants** — `\varepsilon \vartheta \varpi \varrho \varsigma \varphi`

> **The `\epsilon` / `\phi` trap:** as in real LaTeX, `\epsilon` is the lunate ϵ and `\phi` is the closed ϕ. The shapes most people picture — ε and φ — are `\varepsilon` and `\varphi`.

**Greek, upper case** — `\Gamma \Delta \Theta \Lambda \Xi \Pi \Sigma \Upsilon \Phi \Psi \Omega`

> Only these eleven exist, matching LaTeX. The rest are visually identical to Latin capitals, so type `A` for Alpha and `B` for Beta. `\Alpha` is left unexpanded rather than silently diverging from LaTeX.

**Relations and arithmetic** — `\times \cdot \pm \mp \approx \sim \simeq \neq \leq \geq \ll \gg \infty \ldots \deg` (`\degree` is an accepted alias)

**Big operators** — `\sum \prod \int`

**Set theory** — `\in \notin \subset \supset \subseteq \supseteq \cap \cup \emptyset`

**Logic** — `\forall \exists \nexists \wedge \vee \land \lor \neg`

**Arrows** — `\rightarrow \leftarrow \leftrightarrow \Rightarrow \Leftarrow \Leftrightarrow`

**Blackboard bold** — `\mathbb{X}` for any single capital `A`–`Z`: `\mathbb{N}` ℕ, `\mathbb{Z}` ℤ, `\mathbb{Q}` ℚ, `\mathbb{R}` ℝ, `\mathbb{C}` ℂ, `\mathbb{F}` 𝔽, `\mathbb{H}` ℍ, `\mathbb{P}` ℙ, and the rest.

### Rules and limits

This is a lookup table, not a LaTeX engine:

- **Unrecognised macros are left byte-for-byte.** `\foo` displays as `\foo`. That is deliberate — it is what keeps the `\n` line-break marker working, since `n` is not a symbol name. It also means a typo like `\alpah` shows up on screen rather than vanishing.
- **No space is swallowed after a macro.** Real LaTeX renders `\alpha x` as `αx`; here it renders as `α x`, which is nearly always what was meant. Write `\alpha\beta` when two symbols should touch — no separator is needed, because a macro name ends at the first non-letter.
- **No arguments, no maths mode, no sizing.** `\frac{1}{2}` and friends are not supported and pass through unchanged.
- **`\omicron` is a deliberate addition.** LaTeX omits it (omicron is visually identical to Latin `o`); SatyrAV accepts it because a scene author looking for a Greek letter should not have to know that.
- **Fonts:** every symbol above is present in all four bundled DejaVu faces (regular, bold, oblique, bold-oblique). A custom `Font` in `[Options]` without these glyphs will render boxes instead.

## Quoting rules

Every quoted argument in scene files accepts **three interchangeable delimiter pairs**:

| Pair | Use it when the text contains… |
| --- | --- |
| `"..."` | apostrophes, acute accents |
| `'...'` | double quotes, acute accents |
| `´...´` | double quotes, apostrophes |

The opening and closing delimiters must match. Whichever pair is open treats the *other two* as literal characters, so:

```txt
text "He said 'hi' to me."          # ' is literal inside " "
text 'She said "absolutely not".'   # " is literal inside ' '
text ´don't worry about "quotes"´   # both ' and " literal inside ´ ´
```

`´` is the acute accent character (Unicode U+00B4, UTF-8 `0xC2 0xB4`) — distinct from the regular apostrophe `'`. It exists as a third option specifically for cases where both `"` and `'` need to appear inside the displayed text. The rule applies uniformly to `text`, `textCont`, `textbf`, `textit`, `textbfCont`, `textitCont`, `textD`, `comment`, and the optional `LANG` argument of `countdown(...)`.
