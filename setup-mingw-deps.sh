#!/usr/bin/env bash
# setup-mingw-deps.sh — Download SDL3 + FFmpeg MinGW dev libs for cross-compilation.
# Run this once on your Debian/Ubuntu/WSL machine before cross-compiling.
#
# (2.0.0) Migrated from SDL2 to SDL3. If you have an old SDL2 mingw-deps
# directory from a 1.x build, delete it first — mixing the two confuses
# CMake's find_package, which will happily pick up stale SDL2 configs:
#   rm -rf ~/mingw-deps
#
# Usage:
#   ./setup-mingw-deps.sh [DEST_DIR]
#   Default DEST_DIR: ~/mingw-deps

set -euo pipefail

DEST="${1:-$HOME/mingw-deps}"
TMP="$(mktemp -d)"

trap 'rm -rf "$TMP"' EXIT

echo "=== Installing MinGW cross-compile dependencies into $DEST ==="
mkdir -p "$DEST"

# ── SDL3 ──────────────────────────────────────────────────────
# (2.0.0) Versions verified to exist as -mingw.tar.gz release assets.
SDL3_VER="3.4.14"
SDL3_TTF_VER="3.2.2"
SDL3_IMAGE_VER="3.4.4"

echo "[1/5] Downloading SDL3 ${SDL3_VER}..."
wget -qO "$TMP/sdl3.tar.gz" \
	"https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VER}/SDL3-devel-${SDL3_VER}-mingw.tar.gz"
tar xzf "$TMP/sdl3.tar.gz" -C "$TMP"
cp -r "$TMP/SDL3-${SDL3_VER}/x86_64-w64-mingw32/"* "$DEST/"

echo "[2/5] Downloading SDL3_ttf ${SDL3_TTF_VER}..."
wget -qO "$TMP/sdl3_ttf.tar.gz" \
	"https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL3_TTF_VER}/SDL3_ttf-devel-${SDL3_TTF_VER}-mingw.tar.gz"
tar xzf "$TMP/sdl3_ttf.tar.gz" -C "$TMP"
cp -r "$TMP/SDL3_ttf-${SDL3_TTF_VER}/x86_64-w64-mingw32/"* "$DEST/"

echo "[3/5] Downloading SDL3_image ${SDL3_IMAGE_VER}..."
wget -qO "$TMP/sdl3_image.tar.gz" \
	"https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMAGE_VER}/SDL3_image-devel-${SDL3_IMAGE_VER}-mingw.tar.gz"
tar xzf "$TMP/sdl3_image.tar.gz" -C "$TMP"
cp -r "$TMP/SDL3_image-${SDL3_IMAGE_VER}/x86_64-w64-mingw32/"* "$DEST/"

# ── FFmpeg ────────────────────────────────────────────────────
# Using gyan.dev shared+dev build (most common MinGW FFmpeg source)
FFMPEG_VER="7.1"
echo "[4/5] Downloading FFmpeg ${FFMPEG_VER} (shared, mingw64)..."
wget -qO "$TMP/ffmpeg.7z" \
	"https://github.com/GyanD/codexffmpeg/releases/download/${FFMPEG_VER}/ffmpeg-${FFMPEG_VER}-full_build-shared.7z"

if command -v 7z &>/dev/null; then
	7z x -o"$TMP/ffmpeg" "$TMP/ffmpeg.7z" -y >/dev/null
elif command -v 7zz &>/dev/null; then
	7zz x -o"$TMP/ffmpeg" "$TMP/ffmpeg.7z" -y >/dev/null
else
	echo ""
	echo "ERROR: 7z not found. Install with: sudo apt install p7zip-full"
	echo "Then re-run this script."
	exit 1
fi

FFDIR="$TMP/ffmpeg/ffmpeg-${FFMPEG_VER}-full_build-shared"
if [ -d "$FFDIR" ]; then
	mkdir -p "$DEST/include" "$DEST/lib" "$DEST/bin"
	cp -r "$FFDIR/include/"* "$DEST/include/"
	cp -r "$FFDIR/lib/"* "$DEST/lib/"
	cp "$FFDIR/bin/"*.dll "$DEST/bin/"

	# Generate MinGW .dll.a import libs from the DLLs
	# (The GyanD build ships MSVC .lib files which MinGW can't always use)
	echo "    Generating MinGW import libraries from DLLs..."
	if command -v gendef &>/dev/null; then
		for dll in "$DEST/bin/"*.dll; do
			dllbase=$(basename "$dll")
			libname=$(echo "$dllbase" | sed -E 's/-[0-9]+\.dll$/.dll/' | sed 's/\.dll$//')
			outlib="$DEST/lib/lib${libname}.dll.a"
			[ -f "$outlib" ] && continue
			gendef - "$dll" > "$TMP/${dllbase}.def" 2>/dev/null || continue
			x86_64-w64-mingw32-dlltool \
				-d "$TMP/${dllbase}.def" \
				-l "$outlib" \
				-D "$dllbase" 2>/dev/null || true
		done
		echo "    Done."
	else
		echo "    gendef not found — skipping .dll.a generation."
		echo "    Install with: sudo apt install mingw-w64-tools"
		echo "    MinGW can still link against the .lib files directly."
	fi
else
	echo "WARNING: FFmpeg extraction path not found at $FFDIR"
	echo "You may need to download FFmpeg dev libs manually."
	echo "Place headers in $DEST/include and .lib/.a files in $DEST/lib"
fi

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "[5/5] Fixing SDL cmake config paths..."
# The SDL2 MinGW cmake configs contain hardcoded build paths like
# /tmp/tardir/SDL2-2.30.10/build-mingw/install-x86_64-w64-mingw32
# We need to replace ALL prefix paths with our actual DEST directory.
for f in "$DEST/lib/cmake/"*/*.cmake; do
	[ -f "$f" ] || continue
	# Replace the hardcoded prefix in set_and_check / get_filename_component
	# by finding whatever path is assigned to PACKAGE_PREFIX_DIR or used as prefix
	sed -i -E "s|/tmp/tardir/[^ \"]+/install-x86_64-w64-mingw32|${DEST}|g" "$f"
	sed -i -E "s|/opt/local/x86_64-w64-mingw32|${DEST}|g" "$f"
	# Also catch any generic prefix variable that resolves to a nonexistent path
	sed -i -E "s|\\\$\\{PACKAGE_PREFIX_DIR\\}/bin|${DEST}/bin|g" "$f"
done

echo ""
echo "=== Done! Dependencies installed to: $DEST ==="
echo ""
echo "Verifying key files..."
MISSING=0
for f in \
	"$DEST/lib/cmake/SDL3/SDL3Config.cmake" \
	"$DEST/include/SDL3/SDL.h" \
	"$DEST/include/SDL3_ttf/SDL_ttf.h" \
	"$DEST/include/SDL3_image/SDL_image.h" \
	"$DEST/include/libavcodec/avcodec.h" \
	"$DEST/include/libavformat/avformat.h"; do
	if [ -f "$f" ]; then
		echo "  OK: $(basename "$f")"
	else
		echo "  MISSING: $f"
		MISSING=1
	fi
done

# Check for at least one FFmpeg linkable file
if ls "$DEST/lib/"*avcodec* 1>/dev/null 2>&1; then
	echo "  OK: FFmpeg libs found"
else
	echo "  WARNING: No FFmpeg lib files found — cmake may fail"
	echo "           Check that p7zip-full is installed and re-run"
	MISSING=1
fi

echo ""
if [ "$MISSING" -eq 1 ]; then
	echo "Some files are missing — cross-compile may fail."
fi
echo "To cross-compile SatyrAV:"
echo "  mkdir build-win && cd build-win"
echo "  cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/MinGW-w64.cmake \\"
echo "        -DMINGW_DEPS_DIR=$DEST .."
echo "  make -j\$(nproc)"
