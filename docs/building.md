# Building SatyrAV

## Dependencies

- CMake 3.20+
- SDL2 **2.0.16+** (the video pipeline uses `SDL_UpdateNVTexture`), SDL2_ttf, SDL2_image **2.6+** (animated GIF decoding in 1.4 uses `IMG_LoadAnimation`; the MinGW setup script pins 2.8.4)
- FFmpeg 5.1+ (`libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libswresample`)
- C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- toml++ (fetched automatically via CMake FetchContent)

> **Runtime requirement (1.1.0+):** SatyrAV's renderer requests SDL's D3D11 backend on Windows, which is present on all supported Windows versions. Video decoding uses **D3D11VA hardware acceleration** on Windows — any GPU with H.264/HEVC decode support works (Intel HD 4000+, NVIDIA Kepler+, AMD GCN+). On Linux, SDL falls through to OpenGL and video decoding uses FFmpeg's multi-threaded software decoders.

## Linux (Debian 13)

### Install dependencies

```bash
sudo apt install build-essential cmake \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
    libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev libswresample-dev
```

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

The binary is `build/satyr-av`. Run it from the project root so it can find `assets/`.

## Windows (cross-compile from Debian via MinGW)

### Install MinGW toolchain

```bash
sudo apt install g++-mingw-w64-x86-64
```

### Download Windows dependencies

Run the setup script to download SDL2 and FFmpeg MinGW development libraries:

```bash
./setup-mingw-deps.sh
```

This downloads and extracts SDL2, SDL2_ttf, SDL2_image, and FFmpeg (GyanD builds) into `deps-win/`.

### Build

```bash
mkdir build-win && cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-toolchain.cmake
make -j$(nproc)
```

### Package

Copy the executable and all required DLLs:

```bash
cp satyr-av.exe ../dist-win/
cp deps-win/SDL2/bin/*.dll ../dist-win/
cp deps-win/ffmpeg/bin/*.dll ../dist-win/
# MinGW runtime DLLs
cp $(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll) ../dist-win/
cp $(x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll) ../dist-win/
cp $(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll) ../dist-win/
```

### NSIS installer

Install NSIS and build the installer:

```bash
sudo apt install nsis
makensis installer/satyr-av.nsi
```

## Unit tests *(1.8.1)*

> **Tests are OFF by default.** Every build command shown above produces a
> normal SatyrAV build with **no** test code compiled, **no** test framework
> downloaded, and **no** extra dependency of any kind. You never have to opt
> out — only in.

The core suite covers the SDL-independent modules in `src/core/`
(`SceneParser`, `SchemaParser`, `CueEngine`, `Config`, `Project`, `Types`).

### Building with tests

Add one flag at configure time:

```bash
cmake .. -DSATYR_BUILD_TESTS=ON
```

This adds a second target, `satyr-tests`, alongside `satyr-av`, and fetches
[doctest](https://github.com/doctest/doctest) via CMake `FetchContent` the same
way toml++ is fetched. Then build and run:

```bash
make -j$(nproc) && ctest --output-on-failure
```

Or run the binary directly for doctest's per-case output:

```bash
./tests/satyr-tests
```

The flag is sticky per build directory — it lives in `CMakeCache.txt`. To go
back to a test-free build, either configure a fresh directory or turn it off
explicitly:

```bash
cmake .. -DSATYR_BUILD_TESTS=OFF
```

### Running the tests without SDL2 or FFmpeg

Everything under `src/core/` links without the media libraries, so the suite
can be configured on its own — useful for CI, or on a machine where SDL2 and
FFmpeg are not installed at all:

```bash
cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/satyr-tests
```

See [tests/README.md](../tests/README.md) for the file-by-file breakdown and
for what is deliberately left untested.

## Windows icon

The Windows executable embeds `assets/satyr-av-icon.ico` via `src/satyr-av.rc`. This is compiled automatically by MinGW's windres. The icon contains 16x16, 32x32, 48x48, and 256x256 sizes.

## VERSION file

The project version is read from the `VERSION` file at the repository root. CMake passes it as `SATYR_AV_VERSION` at compile time. To bump the version, edit the `VERSION` file.

## Troubleshooting

The build pipeline copies SDL2 / FFmpeg DLLs and the `assets/` folder into the install location automatically — earlier versions required moving those by hand. The notes below cover what still goes wrong, grouped by *when* it bites: configure, compile, launch, or during a show.

### Configure / CMake

- **CMake can't find SDL2 (cross-compile).** Re-run `./setup-mingw-deps.sh`; the `[5/5] Fixing SDL2 cmake config paths` step rewrites the hardcoded prefixes inside the bundled cmake configs. Without it, the SDL2 imports point at `/tmp/tardir/...` and `find_package` silently fails.
- **CMake can't find SDL2 (native Linux).** `libsdl2-dev` alone is not enough — `libsdl2-ttf-dev` **and** `libsdl2-image-dev` are separate packages, and a missing one fails at `find_package` with the same "could not find" wording. Install all three from the command in *Install dependencies*.
- **`SDL_UpdateNVTexture` undeclared.** SDL2 is older than 2.0.16. The video pipeline needs it for the NV12 upload path; upgrade SDL2 rather than patching the call out.
- **`IMG_LoadAnimation` undeclared.** SDL2_image is older than 2.6. Animated-GIF `show` needs it. The MinGW setup script pins 2.8.4; on Linux, upgrade the distro package.
- **CMake reconfigures against the wrong toolchain.** `CMAKE_TOOLCHAIN_FILE` is only read on a *fresh* cache. If you built natively in `build/` and then pass the MinGW toolchain into the same directory, it is ignored. Use a separate build directory (`build/` vs `build-win/`) — never re-point an existing one.
- **Version didn't change after editing `VERSION`.** CMake reads the file at configure time and bakes it in as `SATYR_AV_VERSION`. Re-run `cmake ..` in the build directory; a plain `make` will not pick it up.

### Compile / link

- **`undefined reference to av_*` / `SDL_*` at link time only.** Almost always a bitness or import-library mismatch — a 32-bit SDL2/FFmpeg drop under a 64-bit MinGW toolchain, or a stale `deps-win/`. Delete `deps-win/` and re-run `./setup-mingw-deps.sh`.
- **toml++ download fails behind a proxy/offline.** toml++ comes in via CMake `FetchContent`, so the first configure needs network access. Either configure once while online (the download is cached in the build directory) or point `FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS` at a local checkout.
- **Out-of-memory / killed during `make -j$(nproc)`.** The render and parser translation units are large. Drop to `make -j4` or `make -j2` on machines with less than ~8 GB RAM.
- **`satyr-tests` target doesn't exist / `ctest` reports "No tests were found".** Tests are opt-in. Re-configure with `-DSATYR_BUILD_TESTS=ON` — see *Unit tests* above. The flag is cached per build directory, so it must be passed to `cmake`, not to `make`.
- **`ctest` says the test FAILED but doctest printed `Status: SUCCESS!` above it.** CTest judges a test purely by its **process exit code**, never by parsing output — so this always means the binary crashed *after* finishing its assertions. On MinGW builds before 1.8.2 this was doctest's `INFO()`/`CAPTURE()` context stack, a `thread_local std::vector` whose destructor crashes at process exit when libstdc++ is a DLL. Fixed in 1.8.2 by linking the test binary with `-static-libgcc -static-libstdc++` (see `tests/CMakeLists.txt`). If you hit the same shape of failure again, run the binary directly and check `echo $?` — a non-zero code with clean doctest output means a crash during shutdown, not a failing test.
- **doctest download fails behind a proxy/offline.** Same situation as toml++: the first configure with `-DSATYR_BUILD_TESTS=ON` needs network. Point `FETCHCONTENT_SOURCE_DIR_DOCTEST` at a local checkout to build offline.

### Launch

- **`SDL2.dll` / `avcodec-*.dll` / `SDL2_ttf.dll` not found on launch.** The runtime DLLs must sit next to `satyr-av.exe`. If the installer or build copy step was skipped, copy them manually from `deps-win/SDL2/bin/`, `deps-win/ffmpeg/bin/`, and the MinGW runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) into the executable's directory. See the **Package** section above for the canonical command list.
- **Crash with no window, or font/icon missing.** SatyrAV resolves `assets/` relative to the working directory. Either run the binary from the repo root, or copy the `assets/` folder next to `satyr-av.exe` so it can be found at runtime.
- **`config.toml` reload error after editing paths by hand.** From 1.5.1 the config writer emits paths as TOML literal strings (`'C:\Users\…'`). If you hand-edit a path with backslashes inside double quotes, `\U` etc. are read as escape sequences and the config silently reverts to defaults — wrap path values in single quotes instead.
- **Settings don't stick between runs.** `config.toml` is never written to the build directory — it lives at `Documents\SatyrAV\config.toml` on Windows and `~/.config/satyrav/config.toml` elsewhere (note: *not* alongside `~/satyrav/projects/`). If that location is read-only or on a disconnected network drive, saves fail silently — check the day's log.

### During a show

- **Black slave window, no error.** Confirm the configured slave display index actually exists (`Display N` in the debug popup, `D`). On a single-monitor dev box, enable `Slave Window Mode → Windowed (dev)` in the Options screen so the slave opens as a normal resizable window on the primary display.
- **Slave content is drawn far too small, or off in a corner.** The project's `[Display]` rect in `schema.toml` is set to something narrower than the physical screen. Set all five keys to `0` to go back to "fill the display", or re-open `D → S` and adjust, then ESC to save.
- **A scene is missing from the navigator.** Scenes are listed by name in `schema.toml` under `[Structure.AktN] schema = [...]`; a `.ngk` file that isn't listed there is never loaded. The name in the array must match the filename without the `.ngk` extension.
- **A cue does nothing, or shows raw text instead of running.** An unrecognised leading keyword falls through to `text` by design, so a typo like `txet "Hi"` prints the argument instead of failing loudly. Open the Overview Debug popup (`D → O`) — it lists every parser warning and every `show` / `play` path that doesn't resolve to a file on disk.
- **No sound, or sound on the wrong output.** The audio device is a *global* setting, not per project: Options → `Audio Output`. If the saved device was unplugged since the last run, SatyrAV falls back to the system default and logs it.
- **Video plays but stutters or drops to software decode.** D3D11VA needs a GPU with decode support for the codec in question; unusual codecs (VP9 profile 2, AV1 on older cards) fall back to FFmpeg's software decoders. See [video-pipeline.md](video-pipeline.md).

> **Logs.** Every run appends to `<SatyrAV>/log/satyrav-YYYY-MM-DD.log` (`Documents\SatyrAV\log\` on Windows, `~/satyrav/log/` elsewhere) and mirrors to stderr. When something fails without a visible error, that file is the first place to look.
