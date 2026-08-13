# Changelog

All notable changes to **SatyrAV** are documented here. Versions are listed newest first.

## Version 1.8.4 — shipped

Final sub-version of the 1.8 cycle, and the most complex: a fourth column in the master window plus a new `.ngk` section. Operators can now attach stage-management notes to individual cues and read them beside the command list during a show.

### Added
- **`[Cue]` section in `.ngk` scenes.** One note per line — the first token names it, the rest is the note text. A command claims a note by ending with `\Name`:
    ```txt
    [Cue]
    Cue1 Play audio when Actor leaves the scene

    [Commands]
    play Music.mp3 \Cue1
    ```
    New `CueNote` struct and `Scene::cueNotes` / `Command::cueName` in [Types.hpp](src/core/Types.hpp). Names accept letters, digits and underscores.
- **Cue Notes column in the master window.** A fourth, read-only column right of the command list, drawn in [MasterWindow.cpp](src/render/MasterWindow.cpp). It scrolls in lockstep with the commands, and the primed-row highlight spans both columns so the eye tracks a single row. Nothing in it is focusable or selectable, and nothing reaches the slave — these are notes for the operator.
    - **A note shares its command's row block.** Whichever of the two wraps taller sets the block height, so a note always starts level with its command. This also feeds the 1.6.4 wrap-aware scroll clamp — without counting note rows, the clamp would mis-measure the last screenful and the final command could fall off the bottom again.
    - Long notes wrap onto as many rows as they need, using the same greedy word-wrapper as the command column (now parameterised by pixel budget instead of capturing a constant, so one implementation serves both).
- **The column only claims width when the scene defines notes.** Projects that don't use the feature keep exactly the command column they had. The trade-off is a layout that differs between scenes; the alternative, permanently reserving a third of the command area, would penalise every existing project for a feature it never asked for.

### Changed
- **New projects are generated with `FontSize = 60` instead of 25.** The old value suits a desktop window and is far too small on a projected slave display, so every new project had to be edited before first use. The parser's built-in fallback is deliberately left at 25: changing it would silently resize any existing project that omits the key, and the 1.8.0 generator's whole premise is that it never alters how an existing scene renders. This is now the single documented divergence from "generated value == built-in default", asserted by its own test alongside the invariant it carves out of.

### Notes on the design
- **A reference binds to the navigator row, not to a command.** It is stripped before the `&` split, so `clear & play x.mp3 \Cue1` attaches the note to the Compound — the row the notes column draws beside. A `run{ … }` collapses its body into one row, so new [`RowCueName`](src/core/Types.cpp) falls back to the first sub-command carrying a reference; without it a note written inside a `run{}` body could never be displayed.
- **A reference is only recognised when the name is actually defined.** `\alpha` and `\Cue1` are the same shape, so an undefined `\Name` is left alone for the 1.8.2 symbol expander and a `\Name` inside quotes is always text. The cost is that a *mistyped* reference is silently not a reference — which is why an unreferenced note raises a warning instead, surfacing the mistake on the note rather than on the command. Duplicate names and names with no text warn too. All three appear in the Overview Debug popup (`D → O`).
- **A stray reference on a closing brace no longer eats the scene.** `} \Cue1` is what an author naturally writes when they want a note on a whole block. It was not the block terminator, so the block ran on and silently swallowed every following line. `IsBlockClose` now accepts it at all four body loops (`run{}`, `loop(){}`, `group{}`, `[Macro]`); the reference itself is dropped, since notes bind to command rows.

### Tests
- Suite is now **185 cases / 1783 assertions**, all passing. New [tests/test_cue_notes.cpp](tests/test_cue_notes.cpp) covers `[Cue]` parsing (text with punctuation, trailing comments, name charset, section order either side of `[Commands]`), reference binding across every command shape and block form, the compound and `run{}` row semantics via `RowCueName`, all three warnings, the closing-brace recovery, and the three LaTeX-collision cases (undefined name left alone, quoted name stays text, symbols still expand on a command that also carries a reference).
- Not covered by the SDL-independent suite: the column's layout, lockstep scrolling and on-screen wrapping, which live in `MasterWindow`. Render-layer tests remain deferred to 2.1.0.

---

## Version 1.8.3 — shipped

Fourth sub-version of the 1.8 cycle: image object control. Two modifiers that reshape the slave's image-entry model — explicit draw-order layering, and multi-waypoint movement with repeat. Shipped together, as planned, so that subsystem is only touched once.

### Added
- **`layer(N, content)` — explicit draw order for images.** Until now depth was purely push order: the most recent `show` sat on top and there was no way to put a backdrop behind an image that was already on screen. Higher `N` draws nearer the front; negative values sit behind unlayered content. New `RenderModifiers::layer` in [Types.hpp](src/core/Types.hpp), parsed in [SceneParser.cpp](src/core/SceneParser.cpp).
    - **Existing scenes render pixel-identically.** The default is `0` and ties break by push order, so new [`SlaveWindow::InsertImageByLayer`](src/render/SlaveWindow.cpp) — an `upper_bound` insert that keeps `imageEntries` sorted by layer — degenerates to the old `push_back` when every layer is 0. Sorting at push rather than per frame also leaves the draw loop a plain front-to-back iteration, costing nothing at draw time.
    - Images only. `layer()` on text parses but does nothing, because text is drawn in a later pass that is always above the image layer.
- **`pathMove(P1, P2, …, PN, DURATION_MS [, MODE], content)` — movement through multiple waypoints.** Two or more waypoints, each in `move()`'s existing endpoint vocabulary (`[x,y]` percent coords or an `N`/`S`/`E`/`W` side sentinel), so nothing new has to be learned to use it. `DURATION_MS` is the time for the **whole path**, split evenly across the `N-1` legs — predictable to author, at the cost of non-constant speed on an uneven path. New `RenderModifiers::PathPoint` / `PathRepeat` / `pathPoints` / `pathDurationSec` / `pathRepeat` in [Types.hpp](src/core/Types.hpp).
    - **Repeat modes:** `once` (default — traverse and hold at the final waypoint, the same end state as `move()`), `loop` (restart from the first waypoint), and `pingpong` (travel back along the path in reverse, then forward again). Case-insensitive.
    - Works on text as well as images, and folds into the line-level `+` concatenation slot alongside `move()` on a leftmost-wins basis.
    - **Malformed specs degrade gracefully**, matching `move()`: fewer than two waypoints, a missing or non-positive duration, or an unrecognised mode word leaves the content displayed but unanimated rather than swallowing the cue or printing the raw modifier text on the slave. An unknown mode is treated as a typo rather than silently meaning `once` — the author sees the animation not happen instead of getting the wrong one.

### Changed
- **The two-point move lerp is no longer written out twice.** The text draw path and `RenderTextureFitted` each had their own copy of the `move()` evaluation, and a path would have needed a third and fourth. Both now call one [`EvalAnimatedPosition`](src/render/SlaveWindow.cpp) helper that handles `move()` and `pathMove()` and returns false when an entry is not animated. Parsing a path also sets `hasMove`, so every existing "is this entry animated?" check — the centred-stack exclusion in particular — keeps working untouched, and the evaluator consults `pathPoints` first with a fall-back to the two-point fields. Plain `move()` behaviour is unchanged, pinned by a test.
- The image draw loop's comment claimed ungrouped shows replaced each other at push time, which stopped being true in 1.6.5. Corrected, and extended to describe the layer ordering.
- VSCode extension highlights `pathMove` and `layer`, plus `scale` and `random` which had been missing from the modifier list.

### Tests
- Suite is now **159 cases / 1675 assertions**, all passing. New coverage for `layer()` (default, negative, nesting both ways, non-numeric fallback), `pathMove()` (waypoints, side sentinels, ms→seconds, all three repeat modes, case-insensitivity, many waypoints, nesting, text as well as images), five malformed-spec degradation cases, and a regression case pinning that plain `move()` still parses exactly as before.
- Not covered by the SDL-independent suite, and verified by running the app instead: the actual draw order produced by `InsertImageByLayer`, and the on-screen path interpolation. Both live in the render layer, whose tests are deferred to 2.1.0.

---

## Version 1.8.2 — shipped

Third sub-version of the 1.8 cycle: text rendering features, plus two parser fixes that share a root cause. LaTeX-style symbol macros land in displayed text, `typewrite` reveals a line one character at a time, and the two separators that never respected quoting (`#` and `&`) now share one scanner.

### Added
- **LaTeX symbol macros in displayed text.** `text "pick \delta \in \mathbb{R}"` now renders `pick δ ∈ ℝ`. New [core/LatexSymbols.{hpp,cpp}](src/core/LatexSymbols.cpp) exposes `ExpandLatexSymbols`, called from `ParseTextRuns` — one site, so every text-bearing command inherits it: `text`, the styled and `Cont` variants, both sides of `textD`, `typewrite`, each segment of a `+` concatenation, and `comment` rows (an operator reading "after the Ω speech" beats "after the \Omega speech"). Expansion happens at **parse** time, so the master navigator, the word-wrap measuring and the glyph rendering all deal in real characters and nothing downstream needs to know the feature exists. Filenames, group names and section keys are never touched — a file really can be called `\alpha.png`.
    - **Coverage:** all 24 Greek lower case, the six `\var` forms, and the 11 upper case that LaTeX itself defines. `\Alpha` / `\Beta` are deliberately *not* defined, matching LaTeX — they are visually identical to Latin capitals, so type `A`. `\omicron` is the one deliberate addition (LaTeX omits it for the same reason, but a scene author hunting for a Greek letter should not have to know that). Symbols: `\times \cdot \pm \mp \approx \sim \simeq \neq \leq \geq \ll \gg \infty \ldots \deg`/`\degree`, `\sum \prod \int`, `\in \notin \subset \supset \subseteq \supseteq \cap \cup \emptyset`, `\forall \exists \nexists \wedge \vee \land \lor \neg`, and six arrows. `\mathbb{X}` works for any capital `A`–`Z`, mapping the seven Letterlike codepoints (ℂℍℕℙℚℝℤ) and the Mathematical Alphanumeric block for the rest — so `\mathbb{F}` is a genuine 𝔽 (U+1D53D, a 4-byte UTF-8 sequence), not a fallback.
    - **Design:** a macro name is the greedy run of ASCII letters after the backslash, which makes prefixes unambiguous with no longest-match rule — `\in`, `\infty` and `\int` coexist. Unrecognised macros are copied **byte-for-byte**, which is precisely what keeps the renderer's `\n` line-break marker working (`n` is not a symbol name) and means a typo like `\alpah` appears on screen instead of vanishing. No space is swallowed after a macro, unlike real LaTeX: `\alpha x` renders `α x`, which is nearly always the intent.
    - **Fonts verified, not assumed:** every codepoint in the table was checked against the cmap of all four bundled DejaVu faces (regular / bold / oblique / bold-oblique), including all 26 `\mathbb` capitals. Nothing renders as a missing-glyph box. Rendering was already fully UTF-8 (`TTF_RenderUTF8_Blended` / `TTF_SizeUTF8`), so no render-layer change was needed.
- **`typewrite(MS_PER_CHAR, TEXT)`.** A text line that reveals itself one character at a time. `TEXT` is the full text expression — quoted strings in any of the three delimiters, `+` concatenation, and every modifier — because the argument split is on the first *structural* comma, leaving `color(RED, "a, b")` intact. New `CommandType::Typewrite` and `Command::typeMsPerChar` in [Types.hpp](src/core/Types.hpp), paren-style parsing alongside `countdown` in [SceneParser.cpp](src/core/SceneParser.cpp), and `PushTypewriteRuns` / `CompleteTypewriters` on [SlaveWindow](src/render/SlaveWindow.cpp).
    - Implemented as three fields on the existing `SlaveTextEntry` rather than a parallel entry type, so a typewriter participates in the layout, word-wrap, grouping, `pos`/`move`/`spin` and clear machinery for free. The reveal is resolved in `DrawText`'s wrap pre-pass, before measuring, so a growing line wraps and re-centres exactly as the finished line will; an entry with nothing revealed yet keeps its slot in the centred stack, so the screen does not jump when the first character appears.
    - Revealing is **character-based, not byte-based** — a shared UTF-8 walk means a Greek letter or a `\mathbb{F}` is never cut mid-codepoint into a replacement box. Because `PushTextRuns` splits a `\n`-bearing line into one entry per output line, each entry records how many characters precede it (`typeCharOffset`), which is what makes a multi-line typewrite reveal in reading order instead of every line typing at once.
    - **Advancing the cue snaps any still-typing line to fully typed** (`CompleteTypewriters`, called from `PerformanceScreen::ExecuteCommand` before dispatch). An operator who advances early never leaves a half-written word on the slave. It is called ahead of the dispatch so it applies whatever the next command turns out to be, including a `clear`.
    - `MS_PER_CHAR` is clamped to 0–5000; `0` degrades cleanly to a plain text push. A non-numeric rate falls back to 0 rather than failing the cue.
- **VSCode extension** highlights `typewrite` as a command keyword.

### Bugfixes
- **`#RRGGBB` hex colours work in `color(...)`.** Documented since colour support landed, but never functional: `FindCommentStart` treated any `#` outside a quoted string as the start of a line comment, so `text color(#FF5500, "Custom colour")` was cut down to `text color(` and the rest of the cue became the comment. The hex path itself was fine — `ParseNamedColour` parses `#RRGGBB` correctly — it simply never received the argument.
- **`&` inside quoted text no longer splits the cue.** `ParseCommandLine` split on `&` with `std::getline(stream, segment, '&')`, which knew nothing about quoting, so `text "Tom & Jerry"` was chopped into `text "Tom ` and `Jerry"` and dispatched as a two-command Compound.
- **Both fixes share one scanner.** These were the same bug twice — a separator split without regard for context — and were fixed years apart, each missing the other site. New [`FindStructural`](src/core/SceneParser.cpp) finds the next occurrence of a target character that is outside every quoted string *and* outside every modifier argument list; `FindCommentStart` is now a one-line wrapper over it, and the `&` split routes through it too. A third separator, or a fourth quote style, is now a single edit.
    - Parentheses are counted only outside strings, so `text "smile :)"` cannot unbalance the depth, and a stray closer clamps at zero rather than going negative and disabling the scan for the rest of the line.
    - Two alternatives were rejected for `#`: "no comment when `#` is followed by six hex digits" would silently eat a legitimate comment like `#FFFFFF looks nice here`, and "require whitespace before `#`" breaks on `color( #FF5500, "x")` where the author simply spaced out the argument list.
    - **Syntax note:** a `#` or `&` inside modifier parentheses is now data. A malformed line with an unbalanced `(` before a trailing comment loses that comment — no such line exists in the repo or the generated scenes.

### Tests
- **Fixed: the MinGW test binary segfaulted at exit, so `ctest` reported a failure while doctest printed `Status: SUCCESS!`.** CTest judges a test by its process exit code alone, so a crash *after* the assertions finish looks like a failing test with nothing failing in the output. The cause was doctest's `INFO()`/`CAPTURE()` context stack — a `thread_local std::vector` (`DOCTEST_THREAD_LOCAL`, doctest.h:4578) whose destructor runs at process exit and crashes on MinGW when libstdc++ is a DLL. It is instantiated only when `CAPTURE` is used, which is why the suite passed on Linux and on any subset with no `CAPTURE` in it. Reduced to a five-line program to rule out project code: identical source exits 0 linked with `-static-libstdc++` and segfaults linked against `libstdc++-6.dll`, with one `CAPTURE()` enough to trigger it and no `CAPTURE` meaning no crash. [tests/CMakeLists.txt](tests/CMakeLists.txt) now links the test binary with `-static-libgcc -static-libstdc++` under `if(MINGW)`, which also makes `satyr-tests.exe` self-contained — the runtime DLLs no longer need to sit beside it. `satyr-av` is untouched. A troubleshooting entry in [building.md](docs/building.md) explains the symptom, since the output is so misleading.
- Suite grew from the 134 cases / 1086 assertions 1.8.1 shipped with to **155 cases / 1526 assertions**, all passing (1527 on Windows — the CRLF case asserts one extra thing on that side). New [tests/test_latex_symbols.cpp](tests/test_latex_symbols.cpp) covers the expansion function directly (every symbol, the `\epsilon`/`\phi` trap, greedy prefix matching, `\mathbb` malformed forms, `\n` preservation, lone and trailing backslashes) and then its wiring through each text-bearing command and every block form.
- The `KNOWN LIMITATION:` characterization test for `&` was replaced by the assertions it was standing in for — exactly what it was written to force.
- The 1.6.6 `text "Item #1"` regression, the five block-form comment cases, and the 1.8.0 generated-scene checks all still pass untouched.

---

## Version 1.8.1 — shipped

Second sub-version of the 1.8 cycle: a unit-test suite for the SDL-independent core. No application behaviour changed — the only edits outside `tests/` are three explanatory comments and the CMake opt-in.

### Added
- **Core unit-test suite.** 134 test cases / 1086 assertions over `SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project` and `Types` — the modules with no SDL or FFmpeg dependency, so the suite survives the 2.0.0 SDL3 migration untouched and acts as the regression net while SDL2 is ripped out. Framework is [doctest](https://github.com/doctest/doctest) v2.4.11, fetched via CMake `FetchContent` exactly like toml++ so there is no new system dependency. New `tests/` directory with one file per module plus `helpers.hpp` (a self-cleaning `TempDir` and a `ParseScene` helper that runs a `.ngk` source string through the real `ParseFile`, so no code path is faked). See [tests/README.md](tests/README.md).
- **`SATYR_BUILD_TESTS` CMake option, default OFF.** A normal build compiles no test code, downloads no test framework, and gains no dependency — verified by configuring both ways and confirming the `satyr-tests` target is absent and doctest is not fetched when the flag is off. Opt in with `cmake .. -DSATYR_BUILD_TESTS=ON`, which adds the `satyr-tests` target next to `satyr-av` and registers it with CTest. New `SATYR_CORE_SOURCES` glob in the root [CMakeLists.txt](CMakeLists.txt) is shared by both targets so the list cannot drift.
- **The suite can be built without SDL2 or FFmpeg.** `tests/CMakeLists.txt` detects being configured as its own top-level project (`cmake -S tests -B build-tests`) and declares toml++ itself, skipping the root's `find_package(SDL2 …)` entirely. Useful for CI and for running the core tests on a machine with no media libraries installed.
- **Coverage of the 1.8.0 generators.** `test_project.cpp` asserts that a generated `schema.toml` resolves to *exactly* the built-in `RevySchema` defaults field by field (the 1.8.0 contract: spelling every option out must not change any behaviour), that all four tutorial scenes parse with zero warnings, that they reference no asset other than `satyr-logo.png`, and that each one actually exercises the layer it advertises.
- **Documentation.** New *Unit tests* section in [building.md](docs/building.md) stating the default-off behaviour up front, plus two new troubleshooting entries; new [tests/README.md](tests/README.md) covering layout, how to run, and what is deliberately not tested.

### Notes
- **Explicitly untested features now carry a note at their declaration**, rather than being silently skipped: `CueEngine::StopAllMedia` / `ToggleVideo` / `ToggleMusic` (empty stubs — a test would pin the placeholder, and they land in the render layer whose tests are deferred to 2.1.0), `SceneParser::ParseLoop` (dead stub returning `{}`; real expansion happens in `ExpandLoop`, which is covered), and `Config::Save()` (the no-argument overload forwards to the user's real config path).
- **One characterization test is named `KNOWN LIMITATION:`.** `ParseCommandLine` splits on `&` without the quote tracking `FindCommentStart` has for `#`, so `text "Tom & Jerry"` becomes a Compound instead of one text line. The test pins current behaviour so a future fix fails it loudly instead of going unnoticed.
- **CRLF scene files behave differently per platform**, and the suite asserts each side rather than an aspiration. `Trim` strips only spaces and tabs, so a stray `\r` sticks to the last token on every line. On Windows this never surfaces because `ParseFile` opens the stream in text mode and the CRT translates CRLF to LF; on POSIX there is no translation, so a `.ngk` authored on Windows and parsed on Linux fails to match even its section headers.

---

## Version 1.8.0 — shipped

First sub-version of the 1.8 cycle: project scaffolding and documentation. Nothing in the render, parser, or playback path was touched — only the files a *new* project is generated with, plus docs. Existing projects are unaffected.

### Added
- **Generated `schema.toml` now lists every project option at its default value.** Previously the generator emitted five keys and left the rest to be discovered in the docs. [Project::GenerateDefaultSchema](src/core/Project.cpp) now writes all twelve `[Options]` keys (`FontSize`, `TextOutline`, `Capitalize`, `BackgroundColour`, `Font`, `FontItalic`, `FontBold`, `FontBoldItalic`, `SubtitleItalic`, `SubtitleColour`, `SubtitleTransparency`, `SubtitlePosY`), the full `[Display]` rect at its `0` "fill the display" sentinel, and all twelve `[Hotkeys]` slots as empty strings — uncommented, so an operator edits in place rather than looking a key up. Every emitted value round-trips to exactly the `RevySchema` default it replaces, so a generated file and an absent file resolve identically. Two exceptions, both deliberate: `[RevyData]` carries the user's own name/year/creator, and the ten `[Particle.<TYPE>]` tables ship as one fully-documented commented `[Particle.RAIN]` template (their default is "no override at all", so ten no-op tables in every new project would be pure noise). The bloated per-key commentary was cut back to one trailing comment per line.
- **Four tutorial scenes replace the single `ExampleScene.ngk`.** A new project is generated with two akter and one scene per engine layer: `TextShowcase` (styling, `+` concatenation, `textCont`, `pos`, `textD`, `countdown`), `PictureShowcase` (`size` / `pos` / `trans` / `rotate` / `spin` / `move` / `random`, image stacking, the `&` operator), `ParticleShowcase` (every motion model, `trail` / `trailGlow` / `bounce`, `stopParticleCont`, per-scene particle tuning), and `MacroShowcase` (`[Macro]`, `run{}`, `group{}` + `clear NAME`, `loop(){}`). All four reference only `pictures/satyr-logo.png` — the one asset `Project::Create` copies in — so a brand-new project is fully runnable before any media is added. New [`Project::GenerateExampleScenes`](src/core/Project.cpp) replaces `GenerateExampleScene`; the scene templates are raw string literals rather than per-line `out <<` with escaped quotes, which is what made four scenes practical to write and review.
- **`docs/building.md` troubleshooting expanded** from five flat bullets to eighteen, grouped by when the problem bites — *Configure / CMake*, *Compile / link*, *Launch*, and *During a show* — plus a pointer to the daily log file. New entries cover the SDL2/SDL2_image minimum-version symptoms (`SDL_UpdateNVTexture`, `IMG_LoadAnimation`), toolchain-file caching against an existing build directory, `VERSION` needing a re-configure, offline `FetchContent` for toml++, parallel-build OOM, the real `config.toml` location on each platform, a `[Display]` rect set too small, a `.ngk` missing from `[Structure.AktN]`, and using the Overview Debug popup (`D → O`) to find typo'd cues and unresolved asset paths.

### Fixed
- **Commented `BackgroundColour` example in the generated schema was invalid TOML.** The generator emitted `# BackgroundColour = black` — unquoted, so uncommenting it produced a parse error and `SchemaParser::ParseFile` silently fell back to every default. Now emitted as a live `BackgroundColour = "black"`.

---

## Version 1.7.0 — shipped

Two scene-language additions: alternate quote delimiters so quoted text can itself contain quotation marks, and a live `countdown` command with optional language labels.

### Added
- **Alternate quote delimiters in scene text.** Until 1.7 every quoted argument used `"..."` exclusively, so a line like `text "He said "hi""` was impossible to write. 1.7 accepts three interchangeable string pairs — `"..."`, `'...'`, and `´...´` (acute accent, UTF-8 `0xC2 0xB4`) — and the opener must match the closer. The other two delimiters inside an open string become literal characters, so `text 'He said "hi"'` and `text ´don't worry´` both render correctly. The change is purely additive: every existing scene that uses `"..."` still parses identically. New `DelimAt` helper in [SceneParser.cpp](src/core/SceneParser.cpp) returns the byte length + a kind code for the delimiter at a given offset, used in four refactored sites (`FindCommentStart`, `SplitArgs`, `SplitPlus`, `StripQuotes`) plus the bareword check that decides whether an unknown leading token is a typo or really meant to be text.
- **Live countdown timer.** New `countdown(SECONDS)` command displays a `MM:SS` clock at the top of the slave that ticks down in real time. Both sides are zero-padded (`02:00`, `00:05`); the timer holds at `00:00` when it reaches zero and stays until cleared. Optional second argument selects a language prefix: `countdown(120, "en")` renders `Countdown: 02:00`, `countdown(120, "da")` renders `Nedtælling: 02:00`; omitting the argument shows just the digits so the timer can act as a neutral overlay. Range is `0`–`3599` (59:59) — values outside clamp into range. Multiple active countdowns stack downward. New companion `clearCountdown` command removes them without touching text/images/particles; plain `clear` and `clearAll` also wipe them, but `clearText` deliberately does not, so a timer survives routine text refreshes. Countdowns are scene-scoped — advancing to another scene drops them along with the rest of the slave content. New `CommandType::Countdown` / `ClearCountdown` and `Command::countdownSeconds` / `countdownLabel` in [Types.hpp](src/core/Types.hpp); paren-style parsing in [SceneParser.cpp](src/core/SceneParser.cpp); new `SlaveCountdownEntry` list rendered in the `DrawText` tail in [SlaveWindow.cpp](src/render/SlaveWindow.cpp) using the same `DrawTextCenteredSlave` glyph path as plain text; dispatch wired in [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) and [MasterWindow.cpp](src/render/MasterWindow.cpp).

---

## Version 1.6.6 — shipped

Bug-fix release plus one small feature. Quoted text can now contain `#` without being eaten by the comment stripper, `.ngk` scene files with Danish characters (`æøå`) in their names open correctly on Windows, and `play` videos gained optional fade-in / fade-out times.

### Added
- **Fade-in / fade-out on `play`.** New trailing syntax: `play video.mp4 FADE_IN_MS, FADE_OUT_MS`. Both numbers are integers in milliseconds; `0` (or omitting the entire spec) disables that side. New `Command::fadeInMs` / `Command::fadeOutMs` fields in [Types.hpp](src/core/Types.hpp); [SceneParser::ParseSingleCommand](src/core/SceneParser.cpp) reads the spec by anchoring on the comma, walking back through the fade-in digits, and requiring a whitespace gap to the filename so paths ending in digits (`movie2.mp4 1000, 2000`) are not eaten. [MediaPlayer::PlayVideo](src/render/MediaPlayer.cpp) gained two new tail parameters and stamps them onto the active `VideoPlayback` after activation; `VideoPlayback::Setup` now caches the source duration so the fade-out logic knows when to start ramping down. The brightness ramp is applied per-frame in `UpdateVideoFrame` as `SDL_SetTextureColorMod(tex, b, b, b)`, so 0 % is solid black, 100 % leaves the decoded frame untouched. Audio plays through both ramps at full volume — the spec is visual only. The navigator and debug "last command" line ([MasterWindow](src/render/MasterWindow.cpp), [PerformanceScreen](src/screens/PerformanceScreen.cpp)) print the fade pair when non-zero so the operator still sees the original cue syntax.

### Bugfixes
- **`#` inside quoted text no longer truncates the cue.** [SceneParser.cpp](src/core/SceneParser.cpp) used a plain `line.find('#')` everywhere it stripped line comments, so `text "Item #1"` lost everything from `#1` onward. New `FindCommentStart` helper walks the line tracking a `"`-quoted state and only returns the position of a `#` that sits outside any quoted string. Five call sites updated: top-level scene loop, `run{}` body, `loop(){}` body, `[Macro]` body, and `group{}` body. Quoted-string `#` characters now pass through to the text-command argument verbatim.
- **`.ngk` filenames with `æøå` open on Windows.** Scene names live in `schema.toml` as UTF-8 (per the TOML spec), but the previous `std::ifstream file(scenePath)` in [SceneParser::ParseFile](src/core/SceneParser.cpp) handed the narrow std::string to MinGW's libstdc++, which interpreted it through the system codepage (ANSI) — so the UTF-8 bytes for `æ`, `ø`, `å` resolved to a path that didn't exist. New [`Platform::Utf8ToPath`](src/core/Platform.cpp) helper converts UTF-8 → `std::wstring` via `MultiByteToWideChar(CP_UTF8, …)` on Windows and wraps it in a `std::filesystem::path`; `ParseFile` constructs `std::ifstream` from that path. POSIX takes the std::string as-is.

---

## Version 1.6.5 — shipped

Single bug-fix release: ungrouped `show` calls now stack on screen instead of replacing each other.

### Bugfixes
- **Multiple `show` images coexist on screen.** [SlaveWindow::ShowImage / ShowAnimation](src/render/SlaveWindow.cpp) used to wipe every existing ungrouped image before pushing the new entry, so a scene with `show pic1.png` followed by `show pic2.png` only ever showed the last one. Both paths now skip the pre-push erase and append unconditionally — same coexist semantics ungrouped particle systems got in 1.5. Grouped shows are unchanged (still append). `clear` and `clearImages` still drop every ungrouped image at once, so nothing about teardown changes.

---

## Version 1.6.4 — shipped

Bug-fix release. Four critical UX bugs cleared up: command-list scroll could clip the last command when an earlier visible row word-wrapped, slave text never wrapped at all, dropdown menus came back already-open after ESC + return, and re-entering a project showed the previous session's slave content rendered against the wrong (global) font size.

### Bugfixes
- **Command-list scroll now wrap-aware.** [MasterWindow::DrawNavigator](src/render/MasterWindow.cpp) computed its bottom-scroll clamp as `cmdCount - maxLines` — i.e. one row per command — so any visible command that wrapped onto a continuation row pushed the last command off the bottom of the panel. The clamp now walks backward from the last command, accumulating wrapped row counts, and uses the smallest index whose "from-here-to-end" total still fits as the maximum legal `cmdScroll`. Worked through the example in the bug report (20 commands, command 3 wraps to 2 rows): pre-fix, command 20 was unreachable; post-fix, the scroll backs up one row further when needed and command 20 is fully visible.
- **Word-wrap on the slave display.** Long lines on the slave used to render past the right edge of the targeted display because `SlaveWindow::DrawText` had no wrapping path at all. Added [`WrapEntryLines`/`TokenizeForWrap`/`MeasureLine`](src/render/SlaveWindow.cpp) — greedy fit on word boundaries against a budget of `targetWidth - 2 * margin` (margin ≈ 2.5%), preserving per-run bold/italic/colour/transparency across the wrap. Mid-word breaks are explicitly avoided: a single word longer than the budget keeps its style and overflows visually rather than being chopped. The draw loop was reshaped around per-entry visual lines so default-stack, `pos()`, animated `move()`, and `textD` subtitle anchoring all see the wrapped layout. Subtitle stacking (1.6.1) was extended to multi-line entries — the most-recently-pushed subtitle's *last* line still sits at `SubtitlePosY`, every earlier line (within or before that entry) moves one line height higher.
- **Dropdown menus no longer freeze open after ESC + return.** [OptionsScreen::OnEnter](src/screens/OptionsScreen.cpp), [LoadProjectScreen::OnEnter](src/screens/LoadProjectScreen.cpp), and [NewProjectScreen::OnEnter](src/screens/NewProjectScreen.cpp) now `Close()` every dropdown they own. Previously the dropdown's `expanded=true` flag survived the screen exit (because `Quit` switched screens immediately, without closing the active dropdown), so the next visit drew the panel already-open until the user moved focus onto it. Affects all five dropdowns on the Options screen, both dropdowns on Load Project, and the Revy-type dropdown on New Project.
- **Slave content no longer carries over between project sessions.** [PerformanceScreen::OnEnter / OnExit](src/screens/PerformanceScreen.cpp) now call `slaveUI.ClearAll()` + `slaveUI.StopParticlesAll()` + `media.StopAll()`. The bug surfaced as a font-size regression: text rendered at the project font size persisted on the slave through the project exit, then was redrawn against the global font size set by `OnEnter`. Clearing on both sides means the slave is always blank at the moment a project re-opens — covers text, images, GIF animations, particles, audio, and video — and the next scene's `ApplySceneOptionsCascade` is the only thing that ever puts content back.

---

## Version 1.6.3 — shipped

Final sub-version of the 1.6 cycle. Audio routing, project audio hotkeys, parser-warning surfacing via a new Overview Debug popup, a process-wide log file, and a baseline VSCode extension for `.ngk` syntax highlighting.

### Added
- **Audio output device picker.** New `Config::audioDeviceName` (string; empty = system default) persisted as `[audio] device` in `config.toml`. [MediaPlayer::OpenAudioDeviceFor](src/render/MediaPlayer.cpp) honours it via `SDL_OpenAudioDevice(name, ...)`, with a single-retry fallback to `nullptr` if the saved device is unplugged at boot. [OptionsScreen](src/screens/OptionsScreen.cpp) gets a new `Audio Output` dropdown enumerating `SDL_GetNumAudioDevices(0)` / `SDL_GetAudioDeviceName(i, 0)`; option 0 is `Default`. Lets production setups route SatyrAV sound to a specific HDMI / USB interface independent of the system default.
- **F1–F12 audio hotkeys** for one-tap playback during a performance. New `[Hotkeys]` table in `schema.toml` (`F1 = "knock.mp3"`, …) parsed into `RevySchema::functionKeyAudio`. `InputAction::FunctionKey` collapses all twelve keys into one action with a numeric side channel on `InputHandler` (`GetLastFunctionKey()`); suppressed in text-input mode so typing in a search field doesn't fire a hotkey. [CueEngine::GetFunctionKeyAudio](src/core/CueEngine.cpp) returns the bound filename; [PerformanceScreen](src/screens/PerformanceScreen.cpp) resolves through the same path lookup as `play` (so `"knock.mp3"` picks up `<project>/sound/knock.mp3`). New projects get a commented `[Hotkeys]` example block via [Project::GenerateDefaultSchema](src/core/Project.cpp). Help popup advertises the new row.
- **Hotkey toggle behaviour.** Pressing the *same* F-key while its audio is still playing stops the sound instead of restarting it — useful for long ambient cues (knocking, alarm) where the performer wants one key to drive both start and stop. Different F-key while one is playing replaces the running sound transparently. Tracked via a single `currentHotkeyNum` field on the screen.
- **Global logger.** New [core/Logger.{hpp,cpp}](src/core/Logger.cpp) singleton: per-day rotating file at `<SatyrAV>/log/satyrav-YYYY-MM-DD.log`, mirrored to stderr so dev-time visibility is unchanged. Mutex-guarded so the decode-thread sites in `MediaPlayer` can log safely. printf-style `Debugf`/`Infof`/`Warnf`/`Errorf` API; per-process file-level filter (default `Info`). New `Platform::GetDefaultLogsDir()` resolves `~/Documents/SatyrAV/log` on Windows / `~/satyrav/log` elsewhere. Initialised right after `Config::Load` and torn down in `App::Shutdown` with explicit `starting`/`shutting down cleanly` markers.
- **Overview Debug popup** (`D → O` chord while a project is open, mirroring `D → S` for the rect editor). New [ui/OverviewPopup.{hpp,cpp}](src/ui/OverviewPopup.cpp) walks every scene of the active project on open and reports per-scene parser warnings, the count of executable commands, and any `show` / `play` references that don't resolve to a file on disk. Modal — ESC closes, UP/DOWN scrolls, everything else is swallowed. Cached at open time so projects with many scenes don't re-scan per frame.
- **Parser warnings surfaced in Scene state.** [SceneParser.cpp::ParseSingleCommand](src/core/SceneParser.cpp) now flags bareword keywords that don't match any known command (typo of `txet "..."` for `text`, etc.) by setting `Command::unknownKeyword`; the per-scene parse loop lifts those into `Scene::warnings` recursively (covers compound + loop bodies). Behaviour fallback is unchanged — a flagged command still parses as `Text` so the scene runs — but the warning is now visible to the author via Overview Debug.
- **VSCode `.ngk` syntax extension.** New `VSCodeExt/` folder at repo root with `package.json`, `language-configuration.json`, `syntaxes/ngk.tmLanguage.json`, `README.md`, `.vscodeignore`. Grammar covers section headers, every command keyword (incl. all 1.6.1 text variants and `textD`), modifier wrappers (incl. `bold` / `it`), particle types, distribution constants, group / loop block keywords, strings, numbers, line comments. README documents three install paths — copy-folder (recommended, no toolchain), F5 dev-load, and `vsce package` (with workarounds for Node 18 hitting `vsce` 3.x's Node-20 requirement).

### Changed
- Debug popup footer hint now reads `Press S: Display rect    O: Overview Debug` to advertise the new chord alongside the existing one.
- New `[Hotkeys]` example block emitted by `GenerateDefaultSchema` so users discover the feature when poking around a fresh project's `schema.toml`.

### Bugfixes
- `MediaPlayer::OpenAudioDeviceFor` now retries with `nullptr` (system default) when the configured device name is no longer present — a USB interface unplugged between sessions would previously fail the open and silence all audio until the user fixed `config.toml` by hand.

---

Third sub-version of the 1.6 cycle. Load Project page redesigned around metadata: live search, Revy-type filter, sort, and grouped two-column rendering with creator authorship. Schema gains a `Creator` field so authorship is persisted. One pre-existing UX wart fixed along the way (typing `H`/`D` in any text field no longer trips the popup toggles).

### Added
- New `Load Project` screen layout in [LoadProjectScreen.{hpp,cpp}](src/screens/LoadProjectScreen.cpp). Full rewrite around a `ProjectInfo` struct populated from a single-pass `schema.toml` scan (no `.ngk` parsing, so the load is fast even on large project archives). New interleaved `LoadRow` model — group headers and project rows in one vector; navigation skips headers automatically. `MoveSelection`, `JumpSelectionToTop`/`Bottom`, plus a custom multi-column table render replace the old flat `ListView`.
- Live search field — case-insensitive substring match against the project's full Revy name, applied on every keystroke.
- `Revy` filter dropdown — auto-built from the live scan as `"All"` plus every distinct group alphabetically. Selecting `Kemi` (or any other revy) collapses the table to that revy only. Up/Down arrows flip the option live without expanding; ENTER toggles the panel. Selection survives a re-scan when the group still exists.
- `Sort` dropdown — `Year` (default, descending), `Recent` (folder mtime descending), `Name` (A→Z). Sort applies *within* each group so the section layout stays stable across modes.
- Grouped table rendering: each group gets a header row (`MatRevy`, `KemiRevy`, …) and projects underneath as two columns — full Revy name on the left, creator on the right (right-aligned). Empty creator (legacy projects authored before 1.6.2) renders as a dim em-dash so the column stays visually consistent.
- TAB cycles focus between Search → Filter → Sort → List. ESC defocuses to the list, then to MainMenu when already there. ENTER on the search field jumps to the list. Visible focus via existing widget border-colour flip.
- New `creatorName` and `year` (parsed) fields on [RevySchema](src/core/Types.hpp). [SchemaParser.cpp](src/core/SchemaParser.cpp) reads `[RevyData].Creator` (default empty) and extracts a trailing 4-digit year from `[RevyData].Revy` for sorting purposes. [Project.cpp::GenerateDefaultSchema](src/core/Project.cpp) writes `Creator = "<name>"` so new projects persist authorship; [Project.cpp::Load](src/core/Project.cpp) mirrors `creator` and `year` up to `ProjectData` so existing top-level callers see the persisted values.

### Changed
- Group key derivation strips both a trailing 4-digit year (e.g. `MatRevy 2025` → `MatRevy`) and a trailing `Jubi` suffix (e.g. `MatRevyJubi` → `MatRevy`), so jubilee editions and regular years share a single group.

### Bugfixes
- Pressing `H` or `D` while a text field has focus no longer trips the global Help/Debug popup toggles. The handler in [App.cpp](src/App.cpp) now suppresses those shortcuts while `InputHandler::IsTextInputMode()` is true. Affected the new Load Project search field most visibly, but the same trap existed on the New Project creator field since 1.0.

---

## Version 1.6.1 — shipped

Second sub-version of the 1.6 cycle. Real italic / bold typography on the slave display, plus a `textD` translation/subtitle command for bilingual revues. Built around four loaded font faces (regular / italic / bold / bold-italic) with a scene → project → global cascade and a shared `FONTS` directory so projects can reference fonts by short filename.

### Added
- Shared `FONTS` directory parallel to the projects folder. New `Platform::GetDefaultFontsDir()` resolves to `~/Documents/SatyrAV/FONTS` on Windows and `~/satyrav/FONTS` elsewhere; created automatically by [Config::EnsureDefaults](src/core/Config.cpp). Persisted as `paths.fonts` in `config.toml`. New `Config::ResolveFontPath(name)` accepts either an absolute path (passes through unchanged, drive-letter-aware on Windows) or a bare filename (resolved against `fontsDir`). Used by [App::Init](src/App.cpp) for the global font slots and by [PerformanceScreen::ApplySceneOptionsCascade](src/screens/PerformanceScreen.cpp) for project / scene overrides.
- Bundled DejaVu Sans family in `assets/fonts/` — `DejaVuSans.ttf`, `DejaVuSans-Oblique.ttf`, `DejaVuSans-Bold.ttf`, `DejaVuSans-BoldOblique.ttf`. Plus DejaVu Sans Mono (4 weights) and `DejaVuMathTeXGyre.ttf` for future code / math features. The CMake `install(DIRECTORY assets/ ...)` rule and the NSIS installer's `File /r "..\assets\*.*"` already pick these up — no changes needed to either. [App::Init](src/App.cpp) falls each unset font slot through to the bundled file via `FindAsset("fonts/...")`, so `textbf` / `textit` work out of the box on a fresh install. Last-resort fallback to OS DejaVu (Linux) or Arial (Windows) is preserved if `assets/fonts/` is somehow missing.
- Four loaded font faces in [TextRenderer.cpp](src/render/TextRenderer.cpp) — regular, italic, bold, bold-italic — keyed by `(bold ? 2 : 0) | (italic ? 1 : 0)`. New `Init` signature taking three optional style paths; new `SetSlaveFontPaths(regular, italic, bold, boldItalic)` to swap the slave-side path set on scene change. New private `PickSlaveFont(bold, italic)` walks bold-italic → bold → italic → regular for graceful degradation when only some variants are configured. `DrawTextSlave` / `DrawTextSlaveRotated` / `DrawTextCenteredSlave` / `MeasureSlave` all gained optional `(bool bold, bool italic)` params (default false) so existing call sites keep working.
- Per-segment style flags on [TextRun](src/core/Types.hpp) (`bool bold`, `bool italic`). [SlaveWindow::DrawText](src/render/SlaveWindow.cpp) passes them to measure / draw so bold's wider glyph metrics propagate correctly through the multi-run line layout — a regular `MeasureSlave` would have shifted later runs.
- `textbf "..."` / `textit "..."` / `textbfCont "..."` / `textitCont "..."` line-level keywords. Parser sets the line default style on every produced run via OR with any per-run flag set by inline wrappers, so `textbf "A " + it("B")` renders A bold and B bold-italic.
- Inline `bold(content)` / `it(content)` segment wrappers in [SceneParser::ParseModifiers](src/core/SceneParser.cpp), parsed alongside the existing `color(...)` / `trans(...)` / `rotate(...)` modifiers. `text "Plain " + bold("X") + " " + it("Y") + " mixed"` produces four runs with per-segment styling.
- `textD "primary", "secondary"` command for bilingual subtitles. Top-level comma split (whitespace-tolerant via the existing quote-aware `SplitArgs`). Both sides accept the full `+` / `bold(...)` / `it(...)` / `color(...)` machinery — secondary text lives in `Command::subtitleRuns`. [PerformanceScreen](src/screens/PerformanceScreen.cpp) pushes the primary as a normal centred line and the secondary as a separate bottom-anchored entry with project-configured italic / colour / transparency / posY defaults applied before any per-run override. Single-arg form (`textD "..."`) collapses to plain `Text` semantics. Multiple `textD` entries stack: the most-recently pushed subtitle sits at the configured anchor, older ones move one line up each — pre-pass in [SlaveWindow::DrawText](src/render/SlaveWindow.cpp) assigns per-entry `subtitleBottomY` to keep the layout stable across paint cycles.
- Project `[Options]` keys in [SchemaParser.cpp](src/core/SchemaParser.cpp): `FontItalic`, `FontBold`, `FontBoldItalic` (paths, bare or absolute), and the four `textD` defaults — `SubtitleItalic` (default `true`), `SubtitleColour` (named or `#RRGGBB`, default `#AAAAAA`), `SubtitleTransparency` (percent 0–100, default 0), `SubtitlePosY` (percent of canvas height, default 90.0). Per-scene `[Options]` keys `fontItalic` / `fontBold` / `fontBoldItalic` for scene-level overrides. New CueEngine accessors `GetEffectiveFontPath(bold, italic)`, `GetSubtitle{Italic,Colour,Transparency,PosY}` expose the cascade.
- New helper [PerformanceScreen::ApplySceneOptionsCascade](src/screens/PerformanceScreen.cpp) centralises the scene-change update for font size, outline, font-style face paths, background colour, and capitalize. Three previous in-line copies of this cascade collapsed into one call; the new font-paths step would otherwise have had to be added in three places.

### Changed
- Wordwrap in [MasterWindow::DrawNavigator](src/render/MasterWindow.cpp) and the slave text path now route through the existing measure helpers so adding bold (which widens glyphs) doesn't push later runs off-screen.

### Bugfixes
- `SetSlaveFontPaths` no longer wipes the Config-level italic / bold / bold-italic slots when a project doesn't override them. Empty path on any slot now means "keep existing" (matching the regular slot's behaviour); without this guard, every project-load left only the regular face loaded, and every styled run silently fell back to regular regardless of what the user had set in `config.toml`.

---

## Version 1.6.0 — shipped

First sub-version of the 1.6 cycle. Foundation pass: slave render correctness and the cheap accumulated bugs. Subtitle/text features (1.6.1), Load Project rework (1.6.2), and audio routing + diagnostics + tooling (1.6.3) follow. PNG transparency was originally planned for 1.6 but moved to 2.1 — SDL2 has no per-pixel-alpha window flag, and a workaround would mean hand-rolling D3D11 + DirectComposition; it falls out for free once the 2.0 SDL3 migration lands.

### Added
- `MediaPlayer::HasVideo()` in [MediaPlayer.cpp](src/render/MediaPlayer.cpp). True while a video is loaded — including when paused. Distinct from `IsVideoPlaying()`, which gates on `!videoPaused`.
- `Renderer::GetCanvasWidth/Height()` in [Renderer.cpp](src/render/Renderer.cpp). Returns the rect-authoring-space dims — `intendedSlaveW/H` in windowed-dev mode, slave bounds in fullscreen — so callers that work in `TargetedDisplay` coords don't pick up the live windowed-slave size by mistake.
- Footer hint in the debug popup: "Press S to edit Display rect" ([DebugPopup.cpp](src/ui/DebugPopup.cpp)). Documents the existing `D`→`S` chord; the planned `O` Overview Debug lands in 1.6.3.
- Troubleshooting section in [docs/building.md](docs/building.md) covering DLL placement, asset path, `config.toml` path quoting (1.5.1), the SDL2 cmake-prefix rewrite step, and blank-slave diagnosis.

### Changed
- `K`-pause now freezes the last decoded frame on the slave instead of going black. [SlaveWindow.cpp::DrawMedia](src/render/SlaveWindow.cpp) gates the video-texture draw on `HasVideo()` rather than `IsVideoPlaying()`. The YUV ring upload in `UpdateVideoFrame` is already paused-aware (skips uploads while paused), so the texture retains its last frame and the slave keeps presenting it. `K` again resumes from that frame.
- Long commands in the master command list wrap onto continuation rows instead of being truncated with `...`. [MasterWindow.cpp::DrawNavigator](src/render/MasterWindow.cpp) tracks a real y cursor, sizes the primed-row highlight to span every wrapped row, and only prints the line number on the first row. Wrap uses `TextRenderer::MeasureWidth` with binary-search fitting and word-aware breaks (the previous `cmdColW / 7` char-budget heuristic underestimated proportional-font widths and never wrapped in practice).
- Renamed `Shift+ENTER` action from `PrevCommand` to `StepBack` across [InputHandler](src/input/InputHandler.hpp), [PerformanceScreen](src/screens/PerformanceScreen.cpp), and the [HelpPopup](src/ui/HelpPopup.cpp). Help text now reads "Step back (cursor only — no undo)". The action only rewinds the prime cursor; in-flight audio/video/text remain live. True undo (snapshot/replay or per-command inverses) was deferred — not currently a production need.

### Bugfixes
- Windowed-slave display rect now behaves identically to fullscreen. Root cause: [DisplaySizePopup](src/ui/DisplaySizePopup.cpp) and the auto-rect-default in [PerformanceScreen::SetProject](src/screens/PerformanceScreen.cpp) used live slave-window dims, but `TargetedDisplay` rects are interpreted in production-slave (`intendedSlaveW/H`) pixel space. Three visible symptoms all resolved by switching the relevant call sites to `GetCanvasWidth/Height`:
  - `C` reset fills the full virtual slave instead of the live window's dims (a sub-rect inside the letterbox).
  - Images fill the visible (non-black-bar) area — previously the rect was undersized so the inner fit-to-rect compounded with the outer letterbox into a double-letterbox.
  - `pos` percentages are window-aspect-independent. In a portrait window, a `C` reset previously inherited the portrait dims and mapped `pos%` against them; now the rect is canvas-sized and `pos%` lands in the same relative spot regardless of the dev window's aspect.

---

## Version 1.5.1 - shipped
### Bugfixes
- Fixed `Naeste`&rarr;`Næste` inside `MasterWindow.cpp`

---

## Version 1.5.0 — shipped

### Added
- `<TYPE>.VX.DIST = NAME(p1, p2)` and `<TYPE>.VY.DIST = NAME(p1, p2)` scene/project tunables in [SceneParser.cpp](src/core/SceneParser.cpp) and [SchemaParser.cpp](src/core/SchemaParser.cpp). Reshape how each motion model samples its spawn velocity within its built-in `[vmin, vmax]` range via a new `SampleV` helper in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). `UNIFORM` (default) reproduces 1.4 behaviour exactly. Applies to `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `BROWNIAN`, `LIFECURVE` (both axes) and `OSCILLATION` (vy only). Formula-driven types (`ORBIT`, `NOISE`) and polar `SCATTER` ignore these.
- Gravity + settling on `bounce(true, …)` in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). Enabling bounce now also enables a constant downward gravity (≈ 900 px/s²) on every integrating particle type. Particles bounce off the floor with the existing 0.8 loss coefficient; once both |vx| and |vy| fall below the rest threshold, the particle locks in place (velocity and rotation zeroed) and pile-up forms on the floor. Settled particles still tick their lifetime, so piles fade out naturally. `ORBIT` and `NOISE` ignore gravity because their position is formula-driven.
- Windowed slave mode for single-monitor development. New `Slave Window Mode` dropdown in [OptionsScreen.cpp](src/screens/OptionsScreen.cpp) (Fullscreen / Windowed (dev)); persists as `display.slave_windowed` in `config.toml`. When on, [Renderer::InitSlave](src/render/Renderer.cpp) opens the slave as a resizable, decorated 1280×720 window on the primary display instead of fullscreen on the configured secondary display. `GetSlaveWidth/Height` and `CompositeTargetedDisplay` live-query the current drawable size so user-driven resizes propagate into the letterbox without needing an `SDL_WINDOWEVENT` pipeline. Saved TargetedDisplay rect coords stay in production-slave pixel space — the windowed path tracks `intendedSlaveW/H` (the configured slave display's bounds) and letterboxes that virtual slave inside the live window, so fullscreen and windowed render the same rect at proportional scale. Targeted-display rect, rendering, and input routing are identical to fullscreen; production setups leave the toggle off.
- Capture-mirror window (third OS window) for feeding a capture card / recorder. New `Capture Display` dropdown in [OptionsScreen.cpp](src/screens/OptionsScreen.cpp) (default **Off**); persists as `display.capture_enabled` + `display.capture_display` in `config.toml`. When a project is opened, [Renderer::InitCapture](src/render/Renderer.cpp) opens a borderless fullscreen-desktop window on the selected display at the slave's logical resolution, with `SDL_RenderSetLogicalSize` handling any upscale to the native panel. Each frame, [Renderer::UpdateCaptureFromSlaveTarget](src/render/Renderer.cpp) reads `slaveTarget` back to a CPU ARGB8888 buffer and uploads it to a streaming capture-side texture, producing a clean mirror of the **pre-targeted-display** composite — no rect crop, no rotation, no letterbox. Hard guards refuse if the picked display equals the slave's display; soft guard refuses while `slave_windowed` is on (setting is preserved so flipping back to fullscreen auto-restores the last choice). The capture window closes automatically when leaving the performance screen.
- Canvas-relative slave text sizing in [TextRenderer.cpp](src/render/TextRenderer.cpp). New `slaveScale` multiplier applied to every glyph draw / measure path on the slave (`DrawTextSlave`, `DrawTextCenteredSlave`, `DrawTextSlaveRotated`, `MeasureSlave`, `GetSlaveLineHeight`). [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) calls `text.SetSlaveScale(r.GetSlaveContentScale())` around slave-UI text draws so text scales in lockstep with `slaveTarget` dims — images, videos, GIFs, and particles already do, because they're canvas-relative. Result: the capture mirror shows constant-size text regardless of how the TargetedDisplay rect is resized via the `D`→`S` popup. Master-side UI is unaffected (scale is reset to 1.0 between slave and master passes).

### Changed
- Multiple concurrent ungrouped particle systems. [SlaveWindow.cpp](src/render/SlaveWindow.cpp) `StartParticles` / `StartParticlesPool` no longer drop existing ungrouped systems on a fresh start — ungrouped systems coexist like grouped ones. `clear`, `stop`, and `stopParticleCont` still act on every ungrouped system as a unit; smooth-stopping systems remain protected from later hard-stops.

### Bugfixes
- `config.toml` paths containing backslashes (`C:\Users\…`) no longer wedge the loader. [Config.cpp](src/core/Config.cpp) `Save()` now writes path values as TOML **literal** strings (`'…'`) instead of basic strings (`"…"`), so `\U`, `\n`, and friends aren't interpreted as escape sequences. Previously `toml::parse_file` would throw on reload and every config value silently reverted to its default.
- `DisplaySizePopup` sub-pixel precision stall. With `SHIFT+W/A/S/D` (0.1× move precision) and `SHIFT+Z/X` (scale precision), per-frame deltas at 120 Hz rounded to zero so the rect never moved or resized. [DisplaySizePopup.cpp](src/ui/DisplaySizePopup.cpp) now keeps float accumulators for move (`moveAccumX/Y`) and float mirrors of width/height for scale (`fWidth/fHeight`), draining whole pixels into the integer `TargetedDisplay` fields each frame. Scale precision bumped from 0.1× to 0.25× so `SHIFT+Z/X` feels responsive at small rect sizes.

---

## Version 1.4.1 — shipped

### Changed
- `trail(N, content)` reworked in [ParticleSystem.cpp](src/render/ParticleSystem.cpp). Past samples are now distance-gated (no stacking on slow particles), aged in wall-time, and faded with exponential decay + age-shrink scale instead of the old index-based ramp. Result: smoother streaks, less smearing.
- `trail(N, MODE, content)` — optional middle argument selects the look:
  - `GHOST` (default) — distance-gated past frames with exponential alpha decay and age-shrink scale.
  - `STRETCH` — single elongated quad per particle, rotated along the motion direction. Reads as real motion blur.
  - `GLOW` — Ghost with additive blending. Best for bright sprites.

### Added
- `trailGlow(N, content)` modifier — sugar for `trail(N, GLOW, content)`. Same geometry as Ghost, but `SDL_BLENDMODE_ADD` on the trail samples for a bloom look. Live particle still draws with normal alpha blend.

---

## Version 1.4.0 — shipped

### Added
- Animated GIF support — [MediaCache.cpp](src/render/MediaCache.cpp) decodes every frame to a texture via `IMG_LoadAnimation`; [SlaveWindow.cpp](src/render/SlaveWindow.cpp) advances the current frame by wall time. All position, rotation, spin, move, size, and transparency modifiers apply to the whole animation. New `ShowAnimation` path on the slave; the dispatcher in [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) routes `.gif` shows through it.
- Five new particle motion models in [ParticleSystem.cpp](src/render/ParticleSystem.cpp):
  - `BROWNIAN` — random-walk velocity drift, no directional gravity, mild damping.
  - `OSCILLATION` — sinusoidal X around a per-particle anchor column with linear Y descent. Each particle has its own amplitude, frequency, and phase.
  - `ORBIT` — circular motion around the screen centre at a random radius and angular velocity, random direction per particle.
  - `NOISE` — velocity steered by a 2D value-noise flow field; nearby particles follow similar currents.
  - `LIFECURVE` — linear motion with a triangular scale + alpha envelope (grow + fade in, shrink + fade out).
- `trail(N, content)` modifier — keeps each particle's last `N` transforms in a ring buffer and renders them behind the live particle with alpha ramped up toward the head.
- `bounce(true|false, content)` modifier — reflects particles off the four screen edges with an 0.8 energy-loss coefficient on the reflected velocity component.
- Per-particle-type `[Options]` tunables in scenes ([SceneParser.cpp](src/core/SceneParser.cpp)) and project-level `[Particle.<TYPE>]` tables in `schema.toml` ([SchemaParser.cpp](src/core/SchemaParser.cpp)). Cascade is scene → project → built-in defaults via `CueEngine::GetEffectiveParticleTuning`.
  - `<TYPE>.SPEED` — velocity multiplier (also scales angular velocity for `ORBIT` and oscillation frequency for `OSCILLATION`).
  - `<TYPE>.DENS` / `<TYPE>.DENSITY` — spawn-rate multiplier.
  - `<TYPE>.X.DIST = NAME(p1, p2)` — distribution for spawn X position. Supported: `UNIFORM`, `NORMAL`, `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`. Box-Muller for normals, Marsaglia-Tsang for gamma.
- `stopParticleCont [GROUP]` command — stops a particle system from spawning while letting every live particle finish its animation. The smooth-stop is sticky: subsequent `stop` / `clear` / `clearAll` commands no longer cut the ending short. Drained systems are GC'd from the slave's particle list automatically.
- `+` concatenation operator inside `text` and `textCont` — multiple styled segments on a single line. Per-segment modifiers (`color`, `trans`) override entry-level defaults; line-level modifiers (`pos`, `move`, `rotate`, `spin`, `size`) apply to the whole line, **leftmost wins** when more than one segment sets the same one. New `TextRun` struct on `Command::runs`; the slave's `DrawText` measures and renders runs left-to-right.
- `textCont "…"` command — appends its argument to the most recently pushed text entry across cues, so a line can be revealed piece by piece. Handles `\n` overflow by pushing fresh entries.

### Changed
- `Particle` carries four scratch floats (`auxA`–`auxD`) for per-model state (oscillation amp/freq/phase/baseX, orbit radius/angularVel/angle, etc.) and a trail ring buffer (`std::vector<TrailSample>` + `trailHead`).
- `RenderModifiers` gained `trailLength`, `bounceEdges`, `speedMul`, `densityMul`, and a `DistSpec xDist`. The dispatcher in [PerformanceScreen.cpp](src/screens/PerformanceScreen.cpp) stamps the resolved tuning onto a local copy of `mods` before calling `StartParticles` so `ParticleSystem` doesn't have to re-cascade.
- Particle `Draw` applies per-particle `scale` with centred scaling so animated scale (used by `LIFECURVE` and the trail) doesn't pull particles toward their top-left corner.
- `SlaveWindow::StopParticles`, `StopParticlesAll`, `ClearGroup`, and `ClearAll` now skip systems already in smooth-stop mode.

### Bugfixes
- *(none recorded for 1.4)*

---

## Version 1.3 — shipped

### Added
- `scale(content, [w,h])` modifier in [SceneParser.cpp](src/core/SceneParser.cpp) — pixel-dimension alias of `size()` with the argument order matching `random()` and the other content-first modifiers.
- `[Macro]` section in `.ngk` files for reusable command blocks. Expanded at parse time via `macro <Name>` in `[Commands]`. No parameters, no nesting.
- `group <Name>{ … }` block — named, addressable bucket of commands whose outputs can be cleared independently.
- `clear <Name>` — drop one group by name.
- `clearText` — drop only the text layer across all groups.
- `clearImages` — drop only the image layer across all groups.
- `clearAll` — drop text, images, and every group at once.
- `randomImages(folderName)` now picks a fresh random image on **every particle spawn** when combined with `particle()`, instead of picking once and reusing the same file.
- `U` key reloads all scene `.ngk` files in the current project without leaving it.
- New Project screen additions:
  - `SaTyR` as a selectable revy type.
  - Jubi toggle — folder becomes `<Revy>Jubi_<year>` (e.g. `MatRevyJubi_2019`).
  - "Other" revy type reveals a custom-name field — folder becomes `<CustomName>[Jubi]_<year>`.
- [docs/commands.md](docs/commands.md) — full reference of every section, command, block and modifier with syntax, behaviour, and an example per entry.
- [CHANGELOG.md](CHANGELOG.md) — this file.

### Changed
- Default generated example scene in [Project.cpp](src/core/Project.cpp) now uses the absolute-path resolver, so `show satyr-logo.png` and `[Preload] satyr-logo.png` work without the `pictures/` prefix.

### Bugfixes
- *(none yet in 1.3)*

---

## Version 1.2 — shipped

### Added
- Animation modifiers in [SceneParser.cpp](src/core/SceneParser.cpp) and [SlaveWindow.cpp](src/render/SlaveWindow.cpp):
  - `move(FROM, TO, DURATION_MS, content)` — linear tween of position over time. `FROM`/`TO` accept `[x,y]` percent pairs or side letters `N`/`S`/`E`/`W` (spawns off-screen against that edge).
  - `spin(degPerSec, content)` — continuous rotation at N degrees/second. Composes with the existing static `rotate()`.
- `run{ … }` block — multi-line equivalent of the `&` compound operator. One cue, one navigator row, all sub-commands fire at once.
- `comment "text"` command — visible in the master navigator as `# <text>`, never executable, skipped during navigation, excluded from the line-number count.
- Targeted-display rectangle — slave content renders into a configurable sub-rect (width, height, centre, rotation) inside the physical secondary display. Everything outside the rect is solid black.
- Hidden size menu at Debug popup (`D`) → `S`. Adjust with `Z`/`X` (scale), `Q`/`E` (rotate), `W`/`A`/`S`/`D` (move centre). Hold `SHIFT` for 10× precision. `C` resets to default (fill physical display). `ESC` persists to `schema.toml` and closes.
- `[Display]` section in `schema.toml` (Width, Height, CenterX, CenterY, Rotation) — per-project persistence of the targeted-display rect.

### Changed
- [Project.cpp](src/core/Project.cpp) `Save()` now round-trips `schema.toml` via toml++, preserving `[Options]`, `[RevyData]` and `[Structure.*]` while refreshing the `[Display]` section. Note: stand-alone `#` comments in the schema are not preserved on save.
- Slave layer stack now renders into an offscreen `SDL_Texture` sized to the targeted rect. [MediaPlayer.cpp](src/render/MediaPlayer.cpp) sizes the YUV decode target to the targeted rect rather than the physical display, keeping decode cost proportional to visible area.
- [TextRenderer.cpp](src/render/TextRenderer.cpp) gained `DrawTextSlaveRotated` / `MeasureSlave` so spinning text composes cleanly with `SDL_RenderCopyEx`.

### Bugfixes
- *(none recorded for 1.2)*

---

## Version 1.1 — shipped

### Added
- Hardware-accelerated video playback path in [MediaPlayer.cpp](src/render/MediaPlayer.cpp) — FFmpeg decode with YUV → SDL texture upload on the GPU.

### Changed
- Slave video rendering switched from software blit to GPU texture streaming, cutting CPU load for 1080p/4K clips.

### Bugfixes
- Long debugging pass on the hardware-video path: decoder lifecycle, colour-space mapping, resize handling, and teardown ordering across multi-cue scenes all stabilised.

---

## Version 1.0 — shipped

Initial release. All major functionality of the cue program in place.

### Added
- Dual-window architecture — master control window + slave projector window driven by the same cue engine.
- `.ngk` scene files with `[Options]`, `[Preload]` and `[Commands]` sections.
- Executable commands: `text`, `clear`, `show`, `play`, `stop`.
- Compound cues via `&` (e.g. `clear & text "Hi"`).
- `loop(count, clearEvery){ … }` block.
- Modifier chain on text/images: `color`, `trans`, `rotate`, `pos`, `size`, `random`, `particle`, `randomImages`.
- Project model ([Project.cpp](src/core/Project.cpp)) with `schema.toml` describing akter (acts) and their scene order.
- New Project screen with revy-type picker and year selector.
- Master navigator UI showing the active scene, current cue, and surrounding commands with line numbers.
- Debug popup (`D`) reporting window sizes, current scene, and last-executed command.
- Help popup (`H`) listing keyboard shortcuts.
- Project-wide `[Options]` (font, font size, text outline, background colour) overridable per scene.
- Asset resolver that finds files in `pictures/`, `movies/`, `sound/` subdirectories from bare names.
