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

## Windows icon

The Windows executable embeds `assets/satyr-av-icon.ico` via `src/satyr-av.rc`. This is compiled automatically by MinGW's windres. The icon contains 16x16, 32x32, 48x48, and 256x256 sizes.

## VERSION file

The project version is read from the `VERSION` file at the repository root. CMake passes it as `SATYR_AV_VERSION` at compile time. To bump the version, edit the `VERSION` file.

## Troubleshooting

The build pipeline now copies SDL2 / FFmpeg DLLs and the `assets/` folder into the install location automatically — earlier versions required moving those by hand. The notes below are kept as a reference if a build still misbehaves.

- **`SDL2.dll` / `avcodec-*.dll` / `SDL2_ttf.dll` not found on launch.** The runtime DLLs must sit next to `satyr-av.exe`. If the installer or build copy step was skipped, copy them manually from `deps-win/SDL2/bin/`, `deps-win/ffmpeg/bin/`, and the MinGW runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) into the executable's directory. See the **Package** section above for the canonical command list.
- **Crash with no window, or font/icon missing.** SatyrAV resolves `assets/` relative to the working directory. Either run the binary from the repo root, or copy the `assets/` folder next to `satyr-av.exe` so it can be found at runtime.
- **`config.toml` reload error after editing paths by hand.** From 1.5.1 the config writer emits paths as TOML literal strings (`'C:\Users\…'`). If you hand-edit a path with backslashes inside double quotes, `\U` etc. are read as escape sequences and the config silently reverts to defaults — wrap path values in single quotes instead.
- **CMake can't find SDL2 (cross-compile).** Re-run `./setup-mingw-deps.sh`; the `[5/5] Fixing SDL2 cmake config paths` step rewrites the hardcoded prefixes inside the bundled cmake configs. Without it, the SDL2 imports point at `/tmp/tardir/...` and `find_package` silently fails.
- **Black slave window, no error.** Confirm the configured slave display index actually exists (`Display N` in the debug popup, `D`). On a single-monitor dev box, enable `Slave Window Mode → Windowed (dev)` in the Options screen so the slave opens as a normal resizable window on the primary display.
