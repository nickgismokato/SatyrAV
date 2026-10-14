# Building SatyrAV

## Dependencies

- CMake 3.20+
- **SDL3 3.2 or newer, SDL3_ttf, SDL3_image** *(2.0.0 — see the SDL3 note below; 1.x needed SDL2)*
- FFmpeg 5.1+ (`libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libswresample`)
- C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- toml++ (fetched automatically via CMake FetchContent)

> **Runtime requirement (1.1.0+):** SatyrAV prefers SDL's D3D11 backend on Windows, which is present on all supported Windows versions; if it is unavailable the renderer falls back to whatever SDL picks. Video decoding uses **D3D11VA hardware acceleration** on Windows — any GPU with H.264/HEVC decode support works (Intel HD 4000+, NVIDIA Kepler+, AMD GCN+). On Linux, SDL falls through to OpenGL and video decoding uses FFmpeg's multi-threaded software decoders.

> **SDL3 packaging varies (2.0.0).** Debian 13 packages it (SDL 3.2.10, SDL3_ttf 3.2.2, SDL3_image 3.2.4); Ubuntu 24.04 does not. Where it is missing, build SDL3 from source into a prefix in your home directory — no root needed — and point CMake at it. 2.0.0 was developed against SDL 3.4.14 and has also been built and run against the three Debian 13 versions (built from source at those versions, not installed from the Debian packages themselves). The MinGW cross-build is unaffected: `setup-mingw-deps.sh` downloads official SDL3 MinGW binaries.

## Linux

### Install dependencies

```bash
sudo apt install build-essential cmake git \
    libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev libswresample-dev
```

On **Debian 13** (and anything else that packages SDL3) add the three SDL3 packages and skip the next section:

```bash
sudo apt install libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev
```

### Build SDL3 from source (only if `libsdl3-dev` is unavailable)

This is the route for Ubuntu 24.04, and it needs no root access.

```bash
mkdir -p ~/sdl3-src && cd ~/sdl3-src
for p in SDL/3.4.14/SDL3-3.4.14 SDL_ttf/3.2.2/SDL3_ttf-3.2.2 SDL_image/3.4.4/SDL3_image-3.4.4; do
  repo=${p%%/*}; rest=${p#*/}; ver=${rest%%/*}; dir=${rest#*/}
  wget -q "https://github.com/libsdl-org/$repo/releases/download/release-$ver/$dir.tar.gz"
  tar xzf "$dir.tar.gz"
  cmake -S "$dir" -B "build-$dir" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$HOME/sdl3-prefix" \
        -DCMAKE_PREFIX_PATH="$HOME/sdl3-prefix" \
        -DSDL_X11_XTEST=OFF -DSDLTTF_VENDORED=OFF -DSDLTTF_HARFBUZZ=OFF \
        -DSDLIMAGE_VENDORED=OFF
  cmake --build "build-$dir" -j"$(nproc)" && cmake --install "build-$dir"
done
```

Two flags there are not optional on a stock Debian/Ubuntu box:

- `-DSDL_X11_XTEST=OFF` — SDL3 hard-fails to configure without `libxtst-dev`. SatyrAV never uses input synthesis, so switching it off is cheaper than adding the dependency.
- `-DSDLTTF_VENDORED=OFF -DSDLIMAGE_VENDORED=OFF` — the SDL3_ttf / SDL3_image *release tarballs* do not bundle their `external/` submodules, so a vendored build fails with "Could not find CMakeLists.txt for harfbuzz/zlib". Building against system libraries avoids it. Dropping harfbuzz means no complex text shaping in the Linux build; that is fine for Latin scripts, and the Windows binaries do ship harfbuzz.

### Build

```bash
mkdir build && cd build
cmake ..                                          # SDL3 from distribution packages
cmake .. -DCMAKE_PREFIX_PATH=$HOME/sdl3-prefix    # SDL3 built from source
make -j$(nproc)
```

Run one of the two `cmake` lines, not both. The binary is `build/satyr-av`. Run it from the project root so it can find `assets/`. If SDL3 came from a home-directory prefix, the loader needs to be told where it lives:

```bash
LD_LIBRARY_PATH=$HOME/sdl3-prefix/lib ./build/satyr-av
```

## Windows (cross-compile from Debian/Ubuntu via MinGW)

### Install MinGW toolchain

```bash
sudo apt install g++-mingw-w64-x86-64 mingw-w64-tools p7zip-full
```

`mingw-w64-tools` provides `gendef`, which the setup script needs to generate MinGW import libraries from the FFmpeg DLLs. Without it the FFmpeg link step has nothing to link against.

### Download Windows dependencies

Run the setup script to download SDL3 and FFmpeg MinGW development libraries:

```bash
rm -rf ~/mingw-deps && ./setup-mingw-deps.sh
```

This downloads and extracts SDL3, SDL3_ttf, SDL3_image, and FFmpeg (GyanD builds) into `~/mingw-deps`.

> **Delete any pre-2.0.0 `mingw-deps` first.** If an old SDL2 sysroot is left in place, `find_package` can pick up stale SDL2 configs and produce confusing errors part-way through the build. The `rm -rf` above is the whole fix.

### Build

```bash
mkdir build-win && cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/MinGW-w64.cmake \
         -DMINGW_DEPS_DIR=$HOME/mingw-deps
make -j$(nproc)
```

The result is `build-win/satyr-av.exe`, a **GUI-subsystem** binary — launching it opens no console window. That also means `stdout`/`stderr` are discarded on Windows; the log under `Documents\SatyrAV\log` is the diagnostic channel.

### Package

The build already copies every required DLL next to the executable as a post-build step (SDL3, FFmpeg, and the MinGW runtime — the runtime DLLs are located by asking the compiler, so they always match its threading model). So packaging is just:

```bash
mkdir -p ../dist-win
cp satyr-av.exe *.dll ../dist-win/
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

### Running the tests without SDL or FFmpeg

Everything under `src/core/` links without the media libraries, so the suite
can be configured on its own — useful for CI, or on a machine where SDL3 and
FFmpeg are not installed at all (which, given SDL3 packaging, is common):

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

The build pipeline copies SDL3 / FFmpeg DLLs and the `assets/` folder into the install location automatically — earlier versions required moving those by hand. The notes below cover what still goes wrong, grouped by *when* it bites: configure, compile, launch, or during a show.

### Configure / CMake

- **CMake can't find SDL3 (cross-compile).** Re-run `./setup-mingw-deps.sh`; the `[5/5] Fixing SDL cmake config paths` step rewrites the hardcoded prefixes inside the bundled cmake configs. Without it, the imports point at `/tmp/tardir/...` and `find_package` silently fails. If you upgraded from a 1.x checkout, `rm -rf ~/mingw-deps` **first** — a leftover SDL2 sysroot is picked up in preference and produces errors that look nothing like the real cause.
- **CMake can't find SDL3 (native Linux).** *(2.0.0)* `find_package(SDL3 REQUIRED CONFIG)` fails outright rather than falling back. On Debian 13 install `libsdl3-dev`, `libsdl3-ttf-dev` **and** `libsdl3-image-dev` — they are three separate packages and a missing one fails with the same wording. On a distribution without SDL3 packages (Ubuntu 24.04), build it from source into `~/sdl3-prefix` as shown in *Build SDL3 from source* and pass `-DCMAKE_PREFIX_PATH=$HOME/sdl3-prefix`.
- **SDL3 itself won't configure: `Couldn't find dependency package for XTEST`.** *(2.0.0)* `libxtst-dev` is missing. Either install it or configure SDL3 with `-DSDL_X11_XTEST=OFF` — SatyrAV never uses input synthesis.
- **SDL3_ttf / SDL3_image: `Could not find CMakeLists.txt for harfbuzz` (or `zlib`).** *(2.0.0)* The release tarballs do not include their `external/` submodules, so vendored builds cannot work from a tarball. Configure with `-DSDLTTF_VENDORED=OFF` / `-DSDLIMAGE_VENDORED=OFF` to use system libraries.
- **CMake reconfigures against the wrong toolchain.** `CMAKE_TOOLCHAIN_FILE` is only read on a *fresh* cache. If you built natively in `build/` and then pass the MinGW toolchain into the same directory, it is ignored. Use a separate build directory (`build/` vs `build-win/`) — never re-point an existing one.
- **Version didn't change after editing `VERSION`.** CMake reads the file at configure time and bakes it in as `SATYR_AV_VERSION`. Re-run `cmake ..` in the build directory; a plain `make` will not pick it up.

### Compile / link

- **`undefined reference to av_*` / `SDL_*` at link time only.** Almost always a bitness or import-library mismatch — a 32-bit SDL3/FFmpeg drop under a 64-bit MinGW toolchain, or a stale `~/mingw-deps`. Delete `~/mingw-deps` and re-run `./setup-mingw-deps.sh`.
- **`undefined reference to av_*` only, with SDL linking fine.** The FFmpeg `.dll.a` import libraries were never generated because `gendef` was missing when the setup script ran. Install `mingw-w64-tools` and re-run `./setup-mingw-deps.sh`.
- **`undefined reference to WinMain`.** *(2.0.0)* The SDL3 entry-point wiring is incomplete: `src/main.cpp` must `#include <SDL3/SDL_main.h>` **and** the target must link `SDL3::SDL3main`. SDL3 split these apart; SDL2 got it via `SDL2main` alone.
- **toml++ download fails behind a proxy/offline.** toml++ comes in via CMake `FetchContent`, so the first configure needs network access. Either configure once while online (the download is cached in the build directory) or point `FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS` at a local checkout.
- **Out-of-memory / killed during `make -j$(nproc)`.** The render and parser translation units are large. Drop to `make -j4` or `make -j2` on machines with less than ~8 GB RAM.
- **`satyr-tests` target doesn't exist / `ctest` reports "No tests were found".** Tests are opt-in. Re-configure with `-DSATYR_BUILD_TESTS=ON` — see *Unit tests* above. The flag is cached per build directory, so it must be passed to `cmake`, not to `make`.
- **`ctest` says the test FAILED but doctest printed `Status: SUCCESS!` above it.** CTest judges a test purely by its **process exit code**, never by parsing output — so this always means the binary crashed *after* finishing its assertions. On MinGW builds before 1.8.2 this was doctest's `INFO()`/`CAPTURE()` context stack, a `thread_local std::vector` whose destructor crashes at process exit when libstdc++ is a DLL. Fixed in 1.8.2 by linking the test binary with `-static-libgcc -static-libstdc++` (see `tests/CMakeLists.txt`). If you hit the same shape of failure again, run the binary directly and check `echo $?` — a non-zero code with clean doctest output means a crash during shutdown, not a failing test.
- **doctest download fails behind a proxy/offline.** Same situation as toml++: the first configure with `-DSATYR_BUILD_TESTS=ON` needs network. Point `FETCHCONTENT_SOURCE_DIR_DOCTEST` at a local checkout to build offline.

### Launch

- **`SDL3.dll` / `avcodec-*.dll` / `SDL3_ttf.dll` not found on launch.** The runtime DLLs must sit next to `satyr-av.exe`. If the installer or build copy step was skipped, copy them from `~/mingw-deps/bin/` along with the MinGW runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) into the executable's directory. See the **Package** section above.
- **A console window opens alongside the app.** *(2.0.0)* The executable was linked as a console binary. `WIN32_EXECUTABLE TRUE` must be set on the `satyr-av` target — SDL2 got this implicitly from `SDL2::SDL2main`'s `-mwindows`, and `SDL3::SDL3main` deliberately does not add it.
- **Nothing is printed anywhere when something fails on Windows.** Expected: the release binary is GUI-subsystem, so `stdout`/`stderr` are discarded. Use the log at `Documents\SatyrAV\log\satyrav-YYYY-MM-DD.log`. Note that many low-level failures still only write to `stderr` and are therefore invisible — see the *stderr → Logger* item in [TODO.md](TODO.md).
- **Crash with no window, or font/icon missing.** SatyrAV resolves `assets/` relative to the working directory. Either run the binary from the repo root, or copy the `assets/` folder next to `satyr-av.exe` so it can be found at runtime.
- **`config.toml` reload error after editing paths by hand.** From 1.5.1 the config writer emits paths as TOML literal strings (`'C:\Users\…'`). If you hand-edit a path with backslashes inside double quotes, `\U` etc. are read as escape sequences and the config silently reverts to defaults — wrap path values in single quotes instead.
- **Settings don't stick between runs.** `config.toml` is never written to the build directory — it lives at `Documents\SatyrAV\config.toml` on Windows and `~/.config/satyrav/config.toml` elsewhere (note: *not* alongside `~/satyrav/projects/`). If that location is read-only or on a disconnected network drive, saves fail silently — check the day's log.

### During a show

- **Video is wrong on one machine only (black, garbled, wrong colours, or it stutters where it did not before 2.0.0).** *(2.0.0)* Open the Debug popup (`D`) while the clip plays and read the `Decode:` line. If it says `Hardware (zero-copy)`, set `zero_copy = false` under `[video]` in `config.toml` and restart: that puts video back on the pre-2.0.0 route and tells you whether the graphics driver's handling of the new path is at fault. The log records which path every clip chose and why.
- **The Debug popup says `Software` on Windows.** Hardware decode is not available for that clip's codec on that GPU, or the D3D11 renderer could not be created. It will play, at a much higher CPU cost; a 4K clip may stutter. The log has the reason.

- **Black slave window, no error.** Confirm the configured slave display index actually exists (`Display N` in the debug popup, `D`). On a single-monitor dev box, enable `Slave Window Mode → Windowed (dev)` in the Options screen so the slave opens as a normal resizable window on the primary display.
- **Slave content is drawn far too small, or off in a corner.** The project's `[Display]` rect in `schema.toml` is set to something narrower than the physical screen. Set all five keys to `0` to go back to "fill the display", or re-open `D → S` and adjust, then ESC to save.
- **A scene is missing from the navigator.** Scenes are listed by name in `schema.toml` under `[Structure.AktN] schema = [...]`; a `.ngk` file that isn't listed there is never loaded. The name in the array must match the filename without the `.ngk` extension.
- **A cue does nothing, or shows raw text instead of running.** An unrecognised leading keyword falls through to `text` by design, so a typo like `txet "Hi"` prints the argument instead of failing loudly. Open the Overview Debug popup (`D → O`) — it lists every parser warning and every `show` / `play` path that doesn't resolve to a file on disk.
- **No sound, or sound on the wrong output.** The audio device is a *global* setting, not per project: Options → `Audio Output`. If the saved device was unplugged since the last run, SatyrAV falls back to the system default and logs it. *(2.0.1)* If no output can be opened at all — or the device stops taking sound mid-clip — videos play silently instead of freezing on their first frame, and the log says so once. Before 2.0.1 a missing device froze every video.
- **Video plays but stutters or drops to software decode.** D3D11VA needs a GPU with decode support for the codec in question; unusual codecs (VP9 profile 2, AV1 on older cards) fall back to FFmpeg's software decoders. See [video-pipeline.md](video-pipeline.md).

> **Logs.** Every run appends to `<SatyrAV>/log/satyrav-YYYY-MM-DD.log` (`Documents\SatyrAV\log\` on Windows, `~/satyrav/log/` elsewhere) and mirrors to stderr. When something fails without a visible error, that file is the first place to look.
