#!/usr/bin/env bash
# Build (if needed) and launch the interactive Cardputer emulator.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
"$here/build_emu.sh"
exec python3 "$here/emu.py"
