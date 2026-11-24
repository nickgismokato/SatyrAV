# Unit tests

The core regression suite. It covers `src/core/` only —
`SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project`, `Types`, the
show log, subtitles and project scanner. The render tests (below) cover the
SDL side.

Those modules have **no SDL and no FFmpeg dependency**, which is the whole
point: it builds and runs on a machine with no media libraries, and it
survives changes to the SDL side untouched.

## Tests are off by default

A normal build of SatyrAV never compiles, downloads, or links anything
test-related. Nothing changes unless you opt in:

```bash
cmake .. -DSATYR_BUILD_TESTS=ON
```

With the flag off, the `satyr-tests` target does not exist and doctest is not
even fetched.

## Running them

From an existing build directory:

```bash
cmake .. -DSATYR_BUILD_TESTS=ON && make -j$(nproc) && ctest --output-on-failure
```

Or run the binary directly for doctest's own per-case output:

```bash
./tests/satyr-tests
```

Useful doctest flags: `--list-test-cases`, `-ts=<file>` to filter by suite,
`-tc="*cascade*"` to filter by case name, `-s` to show successful assertions.

### Without SDL3 / FFmpeg

`tests/CMakeLists.txt` can be configured on its own, which skips the root
`find_package(SDL3 …)` entirely — handy for CI or a machine that has no media
libraries installed:

```bash
cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/satyr-tests
```

## Layout

| File | Covers |
|---|---|
| `main.cpp` | doctest entry point (the implementation is compiled once, here) |
| `helpers.hpp` | `TempDir` RAII scratch directory, `ParseScene` / `ParseOneCommand` |
| `test_types.cpp` | `RevyTypeName`, `Colour::FromHex`, neutral-default invariants |
| `test_scene_parser.cpp` | the `.ngk` language — sections, every keyword, quoting, modifiers, `+` runs, compound `&`, `run{}` / `loop(){}` / `group{}` / `[Macro]` |
| `test_schema_parser.cpp` | `schema.toml` → `RevySchema`, every option present and absent |
| `test_cue_engine.cpp` | navigation, comment-skipping indices, dispatch, the options cascade, `ReloadScenes` |
| `test_config.cpp` | `config.toml` round-trip, the Windows-backslash regression, `ResolveFontPath` |
| `test_project.cpp` | project scaffolding and the generators |
| `test_latex_symbols.cpp` | LaTeX symbol macros in displayed text |
| `test_cue_notes.cpp` | the `[Cue]` section, `\Name` references and their warnings |
| `test_option_names.cpp` | one name per option in scenes and `schema.toml`, the shared colour reader, hex in `[Options]`, `TextColour` and the background cascade |
| `test_mark_log.cpp` | the `O` mark-line log — file and folder names, appending, one file per day, what a mark points at |
| `test_backstage.cpp` | the `[Backstage]` block, times and lists, the lineup across akter, the ticker line format, the Url, and the `schema.toml` defaults |
| `test_backstage_send.cpp` | sending, against a small HTTP server inside the test: the exact request, failures, newest-lineup-wins (POSIX only; the Windows sender is checked against a local server by hand) |
| `test_text_plan.cpp` | fixed text centre — which cues form a block, where it ends, and the `FixedTextCentre` option |
| `test_show_log.cpp` | the show log — file names, run numbers, the `.log` line |
| `test_subtitles.cpp` | the per-act `.srt` / `.ass` — text and media cues from slave snapshots, bold / italic / font tags, file names per act |
| `test_project_scanner.cpp` | the project scanner — every kind of finding with its file and line, and a new project scanning clean |

## Render tests

`tests/render/` holds a second suite, `satyr-render-tests`, that runs the
**real app**: `Renderer`, `SlaveWindow`, `TextRenderer`, `MediaPlayer`,
`PerformanceScreen` and `App`, with SDL's off-screen video driver and dummy
audio driver — no window opens and nothing plays. It needs SDL3 and FFmpeg,
so it only exists in the root build with `-DSATYR_BUILD_TESTS=ON`, and
`ctest` runs it next to the core suite.

```bash
cmake --build . --target satyr-render-tests
./tests/render/satyr-render-tests        # run from the source root (assets/)
```

Before SDL starts, `render_main.cpp` points `HOME` / `USERPROFILE` at a
scratch folder, so `config.toml`, the logs and anything the app writes stay
out of your own folders.

| File | Covers |
|---|---|
| `render_main.cpp` | entry point: off-screen drivers, scratch home, working directory |
| `render_helpers.hpp` | `OffscreenRig` (SDL, master + windowed slave, fonts), reading the slave canvas back |
| `test_render_slave.cpp` | text drawn in its colour and cleared, background colour, `layer()` order, image file names / ids, font family names |
| `test_render_video.cpp` | a clip encoded in the test: play, seek, **seek while paused** (the new frame shows at once), restart, past the end |
| `test_render_app.cpp` | the whole app driven with key presses: cues, an `[Animation]` running its passes, the seek and scanner keys, the show log and per-act subtitles it writes — and that nothing is written with the show log off |

`App::Frame` is one pass of the main loop, split out of `App::Run` so these
tests can step the app frame by frame.

## What is deliberately not tested

Coverage is not the goal — catching regressions that are invisible until a
show is. The following are out of scope on purpose:

- **Most of the UI.** `ui/` popups, the menu screens and `input/` are
  checked by running the program. The slave, the video pipeline and the
  performance screen are covered by the render tests above; the zero-copy
  video path (Windows, D3D11) and real audio output are not, since the
  off-screen and dummy drivers have neither.
- **Stubs and placeholders.** `CueEngine::StopAllMedia` / `ToggleVideo` /
  `ToggleMusic` and `SceneParser::ParseLoop` have no behaviour yet. A test
  would pin the placeholder rather than the feature. Each carries a
  `NOT UNIT-TESTED` note at its declaration explaining why.
- **Thin OS wrappers.** `Platform::*` is a handful of `GetHomeDir` /
  `SHGetFolderPath` calls; a test would restate the implementation, and the
  results differ per machine.
- **Debug-only output.** `Command::argument` is asserted where it is
  load-bearing (`show` / `play` filenames, `clear` group names) and left
  alone where it only feeds a log line.

### Characterization tests

One case is named `KNOWN LIMITATION:` — `&` inside quoted text splits the cue,
because `ParseCommandLine` splits on `&` without the quote tracking that
`FindCommentStart` has for `#`. It asserts what the parser does *today* so the
day someone fixes it the test fails loudly and gets updated, rather than the
fix going unnoticed.

## Adding a test

`tests/CMakeLists.txt` globs `*.cpp`, so a new file is picked up on the next
configure. Include `<doctest/doctest.h>` and `helpers.hpp`; use `TempDir` for
anything that touches disk so the suite never writes outside the temp
directory.
