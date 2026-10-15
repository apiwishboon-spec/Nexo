#!/usr/bin/env bash
# Build the interactive emulator core (real ui.cpp on a fake M5Cardputer).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$(cd "$here/../.." && pwd)"

mkdir -p "$here/out"
clang++ -std=c++17 -O2 -DNEXO_UI_PREVIEW \
    -I "$here/stubs" \
    -I "$fw/src" \
    -I "$fw/src/ui" \
    -I "$fw/lib/nexo_core/src" \
    "$here/emu_main.cpp" \
    "$fw/src/ui/ui.cpp" \
    "$here/stubs/host_audio.cpp" \
    "$fw/lib/nexo_core/src/nexo_types.cpp" \
    "$fw/lib/nexo_core/src/nexo_calc.cpp" \
    -o "$here/out/nexo_emu"

echo "built $here/out/nexo_emu"
