#!/usr/bin/env bash
# clean.sh — Remove all build artifacts and temporary files
set -euo pipefail

# Run from the repository root, wherever the script is called from.
cd "$(dirname "$0")"

echo "Cleaning SatyrAV build artifacts..."

# CMake build trees: build/, build-win/, build-tests/, ...
rm -rf build/ build-win/ build-*/
rm -f compile_commands.json CMakeUserPresets.json

# Packaged Windows build (docs/building.md, "Package")
rm -rf dist-win/

# Release archives and the NSIS installer (written next to where makensis ran)
rm -f *.tar.gz *.7z SatyrAV-Installer.exe installer/SatyrAV-Installer.exe

# VSCode extension packages made by `vsce package`
rm -f VSCodeExt/*.vsix

echo "Done."
