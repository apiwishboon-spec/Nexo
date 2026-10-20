#!/usr/bin/env bash
# Build and run the host UI render test (no Cardputer hardware required).
# Compiles the real src/ui/ui.cpp against the fake M5Cardputer stubs and checks
# that animation frames repaint incrementally instead of clearing the panel.
# Used by CI.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$(cd "$here/../.." && pwd)"
out="$here/out"

cxx="${CXX:-}"
if [ -z "$cxx" ]; then
    if command -v clang++ >/dev/null 2>&1; then
        cxx=clang++
    elif command -v g++ >/dev/null 2>&1; then
        cxx=g++
    else
        echo "error: no C++ compiler found (clang++ or g++)" >&2
        exit 1
    fi
fi

mkdir -p "$out"
"$cxx" -std=c++17 -O2 -Wall -DNEXO_UI_PREVIEW \
    -I "$here/stubs" \
    -I "$fw/src" \
    -I "$fw/src/ui" \
    -I "$fw/lib/nexo_core/src" \
    "$here/ui_render_test.cpp" \
    "$fw/src/ui/ui.cpp" \
    "$here/stubs/host_audio.cpp" \
    "$fw/lib/nexo_core/src/nexo_types.cpp" \
    "$fw/lib/nexo_core/src/nexo_calc.cpp" \
    -o "$out/nexo_render_test"

"$out/nexo_render_test"
