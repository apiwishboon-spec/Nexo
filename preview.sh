#!/usr/bin/env bash
# Nexo Cardputer emulator - build the host core and launch the Tk window.
#
#   ./preview.sh            interactive emulator window
#   ./preview.sh stills     render every screen to firmware/tools/preview/out/*.png
#
# Runs the real firmware/src/ui/ui.cpp on the host, no ESP32 hardware needed.
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
prev="$root/firmware/tools/preview"
m5gfx="$root/firmware/.pio/libdeps/m5stack-cardputer/M5GFX/src/lgfx/Fonts"
mode="${1:-emu}"

# --- prerequisites ----------------------------------------------------------
if ! command -v clang++ >/dev/null 2>&1; then
    echo "error: clang++ not found (install Xcode command line tools)." >&2
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 not found." >&2
    exit 1
fi
if ! python3 -c "import PIL" >/dev/null 2>&1; then
    echo "error: Pillow missing. Install with: python3 -m pip install pillow" >&2
    exit 1
fi

# The M5GFX glcd font is reused for pixel-accurate text. Fetch deps once.
if [ ! -f "$m5gfx/glcdfont.h" ]; then
    echo "Fetching PlatformIO dependencies (one time)..."
    if command -v pio >/dev/null 2>&1; then
        (cd "$root/firmware" && pio pkg install -e m5stack-cardputer)
    elif [ -x "$HOME/Library/Python/3.13/bin/pio" ]; then
        (cd "$root/firmware" && "$HOME/Library/Python/3.13/bin/pio" pkg install -e m5stack-cardputer)
    else
        echo "error: PlatformIO 'pio' not found; needed once for M5GFX headers." >&2
        exit 1
    fi
fi

# --- build / run ------------------------------------------------------------
case "$mode" in
    stills)
        exec "$prev/build_preview.sh"
        ;;
    emu|"")
        if ! python3 -c "import tkinter" >/dev/null 2>&1; then
            echo "error: Tkinter missing (needed for the emulator window)." >&2
            exit 1
        fi
        "$prev/build_emu.sh"
        exec python3 "$prev/emu.py"
        ;;
    *)
    echo "usage: $0 [emu|stills]" >&2
    exit 2
    ;;
esac
