#!/usr/bin/env python3
"""Generate the GitHub social preview card (1280x640).

Uses the app logo plus the bundled pixel fonts so the card matches the app's
visual identity. Output: docs/social-preview.png
"""
from __future__ import annotations

import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRAND = os.path.join(ROOT, "app", "assets", "branding")
FONTS = os.path.join(ROOT, "app", "assets", "fonts")
OUT = os.path.join(ROOT, "docs", "social-preview.png")

W, H = 1280, 640

BG = (18, 19, 22)          # NexoColors.background
SURFACE = (30, 32, 37)     # NexoColors.surface
GRID = (26, 28, 33)        # faint grid
PRIMARY = (168, 199, 250)  # NexoColors.primary
ON_SURFACE = (227, 227, 230)
MUTED = (154, 160, 166)    # NexoColors.onSurfaceVariant
ACCENT = (160, 214, 168)   # NexoColors.success


def font(name: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def load_logo(max_h: int) -> Image.Image:
    img = Image.open(os.path.join(BRAND, "app-logo.png")).convert("RGBA")
    bbox = img.getchannel("A").getbbox()
    if bbox:
        img = img.crop(bbox)
    scale = max_h / img.height
    return img.resize((max(1, round(img.width * scale)), max_h), Image.LANCZOS)


def pixelate(img: Image.Image, scale: int = 4) -> Image.Image:
    """Downscale then nearest-upscale for a chunky pixel-art feel."""
    small = img.resize(
        (max(1, img.width // scale), max(1, img.height // scale)), Image.LANCZOS
    )
    return small.resize((img.width, img.height), Image.NEAREST)


def main() -> None:
    canvas = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(canvas)

    # Pixel grid.
    step = 40
    for x in range(0, W, step):
        for y in range(0, H, step):
            draw.rectangle([x, y, x + 2, y + 2], fill=GRID)

    # Accent corner blocks.
    draw.rectangle([0, 0, 12, H], fill=SURFACE)
    draw.rectangle([W - 12, 0, W, H], fill=SURFACE)

    # Logo (left), pixel-scaled.
    logo = pixelate(load_logo(300), scale=4)
    lx, ly = 110, (H - logo.height) // 2
    canvas.paste(logo, (lx, ly), logo)

    # Text block.
    tx = lx + logo.width + 70
    title_font = font("PressStart2P-Regular.ttf", 84)
    body_font = font("VT323-Regular.ttf", 46)
    tag_font = font("VT323-Regular.ttf", 40)

    draw.text((tx, 150), "NEXO", font=title_font, fill=PRIMARY)

    draw.text(
        (tx, 270),
        "Offline-first study toolkit",
        font=body_font,
        fill=ON_SURFACE,
    )
    draw.text(
        (tx, 322),
        "for the M5Stack Cardputer",
        font=body_font,
        fill=ON_SURFACE,
    )

    # Feature chips.
    chips = ["Notes", "Homework", "Music", "Bluetooth", "Wi-Fi", "OTA"]
    cx, cy = tx, 410
    chip_font = font("VT323-Regular.ttf", 36)
    for chip in chips:
        tw = draw.textlength(chip, font=chip_font)
        w = int(tw) + 32
        if cx + w > W - 90:
            cx = tx
            cy += 56
        draw.rectangle([cx, cy, cx + w, cy + 44], outline=ACCENT, width=2)
        draw.text((cx + 16, cy + 6), chip, font=chip_font, fill=ACCENT)
        cx += w + 14

    # Footer.
    draw.text((tx, 560), "AGPL-3.0  ·  github.com/apiwishboon-spec/Nexo",
              font=tag_font, fill=MUTED)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    canvas.save(OUT)
    print("wrote", os.path.relpath(OUT, ROOT), canvas.size)


if __name__ == "__main__":
    main()
