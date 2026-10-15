#!/usr/bin/env python3
"""Convert preview PPMs to PNGs and build a labelled contact sheet."""
import glob
import os
import sys

from PIL import Image, ImageDraw

SCALE = 2


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "out"
    ppms = sorted(glob.glob(os.path.join(outdir, "*.ppm")))
    if not ppms:
        print("no PPMs found", file=sys.stderr)
        return 1

    tiles = []
    for p in ppms:
        im = Image.open(p).convert("RGB")
        png = p[:-4] + ".png"
        im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST).save(png)
        os.remove(p)
        tiles.append((os.path.splitext(os.path.basename(p))[0], im))
        print("png  ", os.path.basename(png))

    cols = 2
    pad = 8
    label_h = 14
    tw = tiles[0][1].width * SCALE
    th = tiles[0][1].height * SCALE
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new(
        "RGB",
        (cols * tw + (cols + 1) * pad, rows * (th + label_h) + (rows + 1) * pad),
        (12, 12, 14),
    )
    draw = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(tiles):
        c, r = i % cols, i // cols
        x = pad + c * (tw + pad)
        y = pad + r * (th + label_h + pad)
        draw.text((x, y), name, fill=(200, 200, 200))
        sheet.paste(im.resize((tw, th), Image.NEAREST), (x, y + label_h))
    sheet.save(os.path.join(outdir, "contact_sheet.png"))
    print("sheet", os.path.join(outdir, "contact_sheet.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
