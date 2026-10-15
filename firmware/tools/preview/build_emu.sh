#!/usr/bin/env bash
# Build the interactive emulator core (real ui.cpp on a fake M5Cardputer).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$(cd "$here/../.." && pwd)"
m5gfx="$fw/.pio/libdeps/m5stack-cardputer/M5GFX/src/lgfx/Fonts"

if [ ! -f "$m5gfx/glcdfont.h" ]; then
    echo "glcdfont.h not found. Run 'pio run -e m5stack-cardputer' once to fetch M5GFX." >&2
    exit 1
fi

mkdir -p "$here/out"
clang++ -std=c++17 -O2 -DNEXO_UI_PREVIEW \
    -I "$here/stubs" \
    -I "$fw/src" \
    -I "$fw/src/ui" \
    -I "$fw/lib/nexo_core/src" \
    -I "$m5gfx" \
    "$here/emu_main.cpp" \
    "$fw/src/ui/ui.cpp" \
    "$here/stubs/host_audio.cpp" \
    "$fw/lib/nexo_core/src/nexo_types.cpp" \
    "$fw/lib/nexo_core/src/nexo_calc.cpp" \
    -o "$here/out/nexo_emu"

echo "built $here/out/nexo_emu"
