#!/usr/bin/env bash
# Build the web emulator (WebAssembly) into website/assets/emu/.
#
# Requires the Emscripten SDK on PATH (emcc). In CI this runs via the
# "emulator" GitHub Actions workflow; locally: `brew install emscripten`.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$(cd "$here/../.." && pwd)"
repo="$(cd "$fw/.." && pwd)"
out="$repo/website/assets/emu"

if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Install the Emscripten SDK (e.g. brew install emscripten)." >&2
    exit 1
fi

mkdir -p "$out"
emcc -std=c++17 -O2 -DNEXO_UI_PREVIEW \
    -I "$here/stubs" \
    -I "$fw/src" \
    -I "$fw/src/ui" \
    -I "$fw/lib/nexo_core/src" \
    "$here/emu_wasm.cpp" \
    "$fw/src/ui/ui.cpp" \
    "$here/stubs/host_audio_wasm.cpp" \
    "$fw/lib/nexo_core/src/nexo_types.cpp" \
    "$fw/lib/nexo_core/src/nexo_calc.cpp" \
    --no-entry \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createNexoEmu \
    -sEXPORTED_FUNCTIONS=_nexoemu_init,_nexoemu_press,_nexoemu_char,_nexoemu_tick,_nexoemu_frame,_nexoemu_width,_nexoemu_height \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web \
    -o "$out/nexo_emu.js"

echo "built $out/nexo_emu.js + nexo_emu.wasm"
