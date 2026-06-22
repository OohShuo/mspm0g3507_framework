#!/usr/bin/env python3
"""Convert airplane battle artwork into compact firmware-side 16-color palette + 4-bit indexed assets.

Uses the same compression technique as the info page avatar images:
  - 16-entry RGB565 palette (index 0 = transparent)
  - 4-bit indices, 2 pixels per byte

This saves ~50% memory compared to the old raw RGB565 + bitmask format.
"""

from __future__ import annotations

from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / "assets" / "images"
OUTPUT_DIR = ROOT / "src" / "app" / "games" / "air_battle"


@dataclass(frozen=True)
class Sprite:
    symbol: str
    filename: str
    size: tuple[int, int]


SPRITES = (
    Sprite("air_sprite_hero", "hero.png", (36, 30)),
    Sprite("air_sprite_mob", "mob.png", (26, 18)),
    Sprite("air_sprite_elite", "elite.png", (34, 22)),
    Sprite("air_sprite_elite_pro", "elitePro.png", (34, 28)),
    Sprite("air_sprite_boss", "boss.png", (64, 43)),
    Sprite("air_sprite_bullet_hero", "bullet_hero.png", (4, 10)),
    Sprite("air_sprite_bullet_enemy", "bullet_enemy.png", (4, 8)),
    Sprite("air_sprite_prop_blood", "prop_blood.png", (14, 13)),
    Sprite("air_sprite_prop_bomb", "prop_bomb.png", (14, 14)),
    Sprite("air_sprite_prop_bullet", "prop_bulletPlus.png", (14, 14)),
)


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def format_array(values: list[int], width: int, suffix: str = "u") -> str:
    lines = []
    for start in range(0, len(values), width):
        row = values[start : start + width]
        lines.append("    " + ", ".join(f"0x{value:04x}{suffix}" for value in row) + ",")
    return "\n".join(lines)


def format_bytes(values: list[int], width: int = 16) -> str:
    lines = []
    for start in range(0, len(values), width):
        row = values[start : start + width]
        lines.append("    " + ", ".join(f"0x{value:02x}u" for value in row) + ",")
    return "\n".join(lines)


# ── Median-cut color quantizer (same algorithm as tools/png_to_pal4.py) ──

def quantize_median_cut(pixels: list[tuple[int, int, int]], num_colors: int = 16):
    """Quantize a list of (r,g,b) tuples to at most `num_colors` palette entries."""
    if len(pixels) <= num_colors:
        unique = list(OrderedDict.fromkeys(pixels))
        return unique + [unique[-1]] * (num_colors - len(unique))

    boxes = [list(pixels)]

    while len(boxes) < num_colors:
        # Find the box with the largest range in any channel
        largest_idx = 0
        largest_range = -1
        for i, box in enumerate(boxes):
            if len(box) <= 1:
                continue
            r_min = min(p[0] for p in box)
            r_max = max(p[0] for p in box)
            g_min = min(p[1] for p in box)
            g_max = max(p[1] for p in box)
            b_min = min(p[2] for p in box)
            b_max = max(p[2] for p in box)
            max_range = max(r_max - r_min, g_max - g_min, b_max - b_min)
            if max_range > largest_range:
                largest_range = max_range
                largest_idx = i

        if largest_range < 1:
            break

        box = boxes.pop(largest_idx)
        # Determine which channel has the largest range
        r_min = min(p[0] for p in box)
        r_max = max(p[0] for p in box)
        g_min = min(p[1] for p in box)
        g_max = max(p[1] for p in box)
        b_min = min(p[2] for p in box)
        b_max = max(p[2] for p in box)

        if (r_max - r_min) >= (g_max - g_min) and (r_max - r_min) >= (b_max - b_min):
            channel = 0  # R
        elif (g_max - g_min) >= (b_max - b_min):
            channel = 1  # G
        else:
            channel = 2  # B

        box.sort(key=lambda p: p[channel])
        median = len(box) // 2
        boxes.append(box[:median])
        boxes.append(box[median:])

    # Compute palette: average of each box
    palette = []
    for box in boxes:
        if len(box) == 0:
            palette.append((0, 0, 0))
        else:
            r = sum(p[0] for p in box) // len(box)
            g = sum(p[1] for p in box) // len(box)
            b = sum(p[2] for p in box) // len(box)
            palette.append((r, g, b))

    while len(palette) < num_colors:
        palette.append(palette[-1])

    return palette[:num_colors]


def find_nearest(pixel: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    """Find the index of the nearest palette color (Euclidean distance in RGB)."""
    best_idx = 0
    best_dist = float('inf')
    for i, pal in enumerate(palette):
        dr = pixel[0] - pal[0]
        dg = pixel[1] - pal[1]
        db = pixel[2] - pal[2]
        dist = dr * dr + dg * dg + db * db
        if dist < best_dist:
            best_dist = dist
            best_idx = i
    return best_idx


# ── Sprite loading (pal4 format) ──

def load_sprite_pal4(sprite: Sprite) -> tuple[list[int], list[int]]:
    """Load a sprite, blend against edge background, quantize to 16-color palette,
    and pack into 4-bit indices.

    Returns (palette_565, data_bytes) where:
      - palette_565: 16 uint16_t RGB565 values (index 0 = transparent black)
      - data_bytes: packed 4-bit indices, 2 pixels per byte, row-major
    """
    image = Image.open(SOURCE_DIR / sprite.filename).convert("RGBA")
    image.thumbnail(sprite.size, Image.LANCZOS)
    canvas = Image.new("RGBA", sprite.size, (0, 0, 0, 0))
    x = (sprite.size[0] - image.width) // 2
    y = (sprite.size[1] - image.height) // 2
    canvas.alpha_composite(image, (x, y))

    edge_background = (9, 27, 54)  # dark blue, matches original

    # Separate opaque and transparent pixels (blend semi-transparent against edge bg)
    opaque_pixels: list[tuple[int, int, int]] = []
    blended_pixels: list[tuple[int, int, int, bool]] = []  # (r, g, b, is_opaque)

    for r, g, b, a in canvas.getdata():
        if a < 64:
            blended_pixels.append((0, 0, 0, False))
        else:
            blend = a / 255.0
            rb = round(r * blend + edge_background[0] * (1.0 - blend))
            gb = round(g * blend + edge_background[1] * (1.0 - blend))
            bb = round(b * blend + edge_background[2] * (1.0 - blend))
            blended_pixels.append((rb, gb, bb, True))
            opaque_pixels.append((rb, gb, bb))

    # Palette: index 0 = transparent black
    palette_rgb: list[tuple[int, int, int]] = [(0, 0, 0)]

    if opaque_pixels:
        # Quantize to 15 colors (indices 1..15)
        colors_15 = quantize_median_cut(opaque_pixels, 15)
        palette_rgb += colors_15
    else:
        palette_rgb += [(0, 0, 0)] * 15

    # Map each pixel to palette index
    indices = []
    for r, g, b, is_opaque in blended_pixels:
        if not is_opaque:
            indices.append(0)
        else:
            indices.append(find_nearest((r, g, b), palette_rgb))

    # Convert palette to RGB565
    palette_565 = [rgb565(*c) for c in palette_rgb]

    # Pack 4-bit indices (2 per byte, row-major)
    w, h = sprite.size
    data_bytes = []
    for row in range(h):
        for col in range(0, w, 2):
            hi = indices[row * w + col]
            lo = indices[row * w + col + 1] if col + 1 < w else 0
            data_bytes.append((hi << 4) | lo)

    return palette_565, data_bytes


def generate() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    background = Image.open(SOURCE_DIR / "bg2.jpg").convert("RGB")
    background = ImageOps.fit(
        background, (80, 107), method=Image.LANCZOS, centering=(0.5, 0.5)
    )
    background_pixels = [rgb565(*pixel) for pixel in background.getdata()]

    # ── Header ──
    header = """#pragma once

#include <stdint.h>

#define AIR_BACKGROUND_WIDTH  80
#define AIR_BACKGROUND_HEIGHT 107
#define AIR_BACKGROUND_SCALE  3

/* Sprite stored as 16-color palette + 4-bit indexed data (2 pixels per byte).
   Index 0 is always transparent. Use Game_Graphics_Draw_Pal4_Bitmap to render. */
typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t* palette;  /* 16-entry RGB565 palette */
    const uint8_t* data;      /* 4-bit indices, 2 pixels per byte, row-major */
} Air_sprite;

extern const uint16_t air_background[AIR_BACKGROUND_WIDTH * AIR_BACKGROUND_HEIGHT];
"""
    for sprite in SPRITES:
        header += f"extern const Air_sprite {sprite.symbol};\n"

    # ── Source ──
    source = """#include "air_assets.h"

/* Generated by scripts/generate_air_battle_assets.py. */
const uint16_t air_background[AIR_BACKGROUND_WIDTH * AIR_BACKGROUND_HEIGHT] = {
"""
    source += format_array(background_pixels, 10)
    source += "\n};\n\n"

    total_pixels = 0
    total_bytes = 0

    for sprite in SPRITES:
        w, h = sprite.size
        palette_565, data_bytes = load_sprite_pal4(sprite)
        pixel_count = w * h
        sprite_bytes = 32 + len(data_bytes)
        total_pixels += pixel_count
        total_bytes += sprite_bytes

        # Palette
        source += f"static const uint16_t {sprite.symbol}_palette[16] = {{\n"
        source += format_array(palette_565, 8)
        source += "\n};\n"

        # 4-bit data
        source += f"static const uint8_t {sprite.symbol}_data[{len(data_bytes)}] = {{\n"
        source += format_bytes(data_bytes)
        source += "\n};\n"

        # Sprite struct
        source += (
            f"const Air_sprite {sprite.symbol} = "
            f"{{{w}u, {h}u, {sprite.symbol}_palette, {sprite.symbol}_data}};\n\n"
        )

        old_pixel_bytes = pixel_count * 2  # raw RGB565
        old_mask_bytes = (pixel_count + 7) // 8
        old_total = old_pixel_bytes + old_mask_bytes
        print(f"  {sprite.symbol}: {w}x{h} = {pixel_count} px → {sprite_bytes} B "
              f"(was {old_total} B, saved {old_total - sprite_bytes} B, "
              f"{(old_total - sprite_bytes) * 100 // old_total}%)")

    print(f"\nTotal: {total_bytes} B (vs ~{total_pixels * 2 + (total_pixels + 7) // 8} B uncompressed)")

    (OUTPUT_DIR / "air_assets.h").write_text(header.rstrip() + "\n", encoding="ascii")
    (OUTPUT_DIR / "air_assets.c").write_text(source.rstrip() + "\n", encoding="ascii")
    print(f"\nWrote {OUTPUT_DIR / 'air_assets.h'}")
    print(f"Wrote {OUTPUT_DIR / 'air_assets.c'}")


if __name__ == "__main__":
    generate()
