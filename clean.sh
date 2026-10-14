#!/usr/bin/env bash
# clean.sh — Remove all build artifacts and temporary files
set -euo pipefail

echo "Cleaning SatyrAV build artifacts..."

rm -rf build/ build-win/ build-*/
rm -f compile_commands.json CMakeUserPresets.json
rm -f *.tar.gz SatyrAV-Installer.exe

echo "Done."
