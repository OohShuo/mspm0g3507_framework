#!/usr/bin/env python3
"""Convert 100x100 PNG assets into RGB565 uint16_t C headers for the info page."""

from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ASSETS_DIR = ROOT / "assets"
OUTPUT_DIR = ROOT / "src" / "app" / "games" / "info"

# (png_filename, header_filename, array_name, w_macro, h_macro)
IMAGES = (
    ("hitsz100x100.png", "info_image_hitsz.h", "info_image_hitsz_data", "INFO_IMAGE_HITSZ_W", "INFO_IMAGE_HITSZ_H"),
    ("MizunoAkane100x100.png", "info_image_mizuno.h", "info_image_mizuno_data", "INFO_IMAGE_MIZUNO_W", "INFO_IMAGE_MIZUNO_H"),
    ("MizunoAkane25x25.png", "info_image_mizuno_25.h", "info_image_mizuno_25_data", "INFO_IMAGE_MIZUNO_25_W", "INFO_IMAGE_MIZUNO_25_H"),
)


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def generate_header(png_path: Path, output_path: Path, array_name: str, w_macro: str, h_macro: str) -> None:
    img = Image.open(png_path).convert("RGB")
    w, h = img.size

    pixels = []
    for y in range(h):
        for x in range(w):
            r, g, b = img.getpixel((x, y))
            pixels.append(rgb565(r, g, b))

    # Format array with 16 values per line
    lines = []
    lines.append(f"// Auto-generated from {png_path.name} ({w}x{h})")
    lines.append(f"// {len(pixels)} pixels, RGB565 format")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define {w_macro} {w}")
    lines.append(f"#define {h_macro} {h}")
    lines.append("")
    lines.append(f"static const uint16_t {array_name}[{len(pixels)}] = {{")

    per_line = 16
    for start in range(0, len(pixels), per_line):
        row = pixels[start:start + per_line]
        line = "    " + ", ".join(f"0x{v:04x}u" for v in row) + ","
        lines.append(line)

    lines.append("};")

    output_path.write_text("\n".join(lines) + "\n")
    print(f"Generated {output_path} ({w}x{h}, {len(pixels)} pixels)")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for png_name, header_name, array_name, w_macro, h_macro in IMAGES:
        png_path = ASSETS_DIR / png_name
        if not png_path.exists():
            print(f"ERROR: {png_path} not found, skipping")
            continue
        generate_header(png_path, OUTPUT_DIR / header_name, array_name, w_macro, h_macro)
    print("Done.")


if __name__ == "__main__":
    main()
