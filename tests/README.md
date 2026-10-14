# Unit tests

The core regression suite, added in **1.8.1**. It covers `src/core/` only —
`SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project`, and `Types`.

Those modules have **no SDL and no FFmpeg dependency**, which is the whole
point: the suite survived the 2.0.0 SDL3 migration untouched, and was the
regression net while SDL2 was replaced.

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
| `test_config.cpp` | `config.toml` round-trip, the 1.5.1 Windows-backslash regression, `ResolveFontPath` |
| `test_project.cpp` | project scaffolding and the 1.8.0 generators |
| `test_latex_symbols.cpp` | *(1.8.2)* LaTeX symbol macros in displayed text |
| `test_cue_notes.cpp` | *(1.8.4)* the `[Cue]` section, `\Name` references and their warnings |
| `test_option_names.cpp` | *(2.0.1)* one name per option in scenes and `schema.toml`, the shared colour reader, hex in `[Options]`, `TextColour` and the background cascade |
| `test_mark_log.cpp` | *(2.0.1)* the `O` mark-line log — file and folder names, appending, one file per day, what a mark points at |
| `test_backstage.cpp` | *(2.0.2)* the `[Backstage]` block, times and lists, the lineup across akter, the ticker line format, the Url, and the `schema.toml` defaults |
| `test_backstage_send.cpp` | *(2.0.2)* sending, against a small HTTP server inside the test: the exact request, failures, newest-lineup-wins (POSIX only; the Windows sender is checked against a local server by hand) |
| `test_text_plan.cpp` | *(2.0.2)* fixed text centre — which cues form a block, where it ends, and the `FixedTextCentre` option |

## What is deliberately not tested

Coverage is not the goal — catching regressions that are invisible until a
show is. The following are out of scope on purpose:

- **Everything visual.** `render/`, `ui/`, `screens/`, `input/`. These are
  SDL-bound and are verified by running the program. Render / audio /
  `MediaPlayer` tests are deferred to **2.1.0**. They were held back until
  the SDL3 backend landed in 2.0.0, since testing against SDL2 would have
  meant writing them twice.
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
