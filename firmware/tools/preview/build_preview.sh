#!/usr/bin/env bash
# Build and run the host UI preview (no Cardputer hardware required).
# Renders every screen in ui.cpp to firmware/tools/preview/out/*.png
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$(cd "$here/../.." && pwd)"
out="$here/out"

mkdir -p "$out"
clang++ -std=c++17 -O2 -Wall -DNEXO_UI_PREVIEW \
    -I "$here/stubs" \
    -I "$fw/src" \
    -I "$fw/src/ui" \
    -I "$fw/lib/nexo_core/src" \
    "$here/preview_main.cpp" \
    "$fw/src/ui/ui.cpp" \
    "$here/stubs/host_audio.cpp" \
    "$fw/lib/nexo_core/src/nexo_types.cpp" \
    "$fw/lib/nexo_core/src/nexo_calc.cpp" \
    -o "$out/nexo_preview"

"$out/nexo_preview" "$out"
python3 "$here/render_png.py" "$out"
echo "Preview images in $out"
