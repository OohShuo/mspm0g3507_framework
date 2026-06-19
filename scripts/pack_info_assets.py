#!/usr/bin/env python3
"""Pack the info page artwork as external RGB565 Image_Asset files."""

from __future__ import annotations

import argparse
import pathlib
import re
import struct


ROOT = pathlib.Path(__file__).resolve().parent.parent
INFO_DIR = ROOT / "src" / "app" / "games" / "info"
DEFAULT_OUTPUT_DIR = ROOT / "build" / "info_assets"

GRAY4_LUT = (
    0x0000, 0x1082, 0x2104, 0x31A6,
    0x4228, 0x52AA, 0x632C, 0x73AE,
    0x8C51, 0x9CD3, 0xAD55, 0xBDD7,
    0xCE59, 0xDEFB, 0xEF7D, 0xFFFF,
)

ASSETS = (
    ("info_image_hitsz.h", "info_image_hitsz_data", None, 100, 100, 0x3001, "info_hitsz.r565"),
    ("info_image_oooshuo.h", "info_image_oooshuo_50_data", "info_image_oooshuo_50_palette", 50, 50, 0x3002, "info_oooshuo.r565"),
    ("info_image_morrow.h", "info_image_morrow_50_data", "info_image_morrow_50_palette", 50, 50, 0x3003, "info_morrow.r565"),
    ("info_image_polaris.h", "info_image_polaris_50_data", "info_image_polaris_50_palette", 50, 50, 0x3004, "info_polaris.r565"),
)


def parse_array(text: str, c_type: str, name: str) -> list[int]:
    pattern = rf"static const {c_type} {name}\[(?:\d+)?\]\s*=\s*\{{(.*?)\}};"
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f"{name}: array not found")
    width = 4 if c_type == "uint16_t" else 2
    return [int(value, 16) for value in re.findall(rf"0x([0-9a-fA-F]{{{width}}})u?", match.group(1))]


def unpack_packbits(data: list[int]) -> list[int]:
    out: list[int] = []
    index = 0
    while index < len(data):
        token = data[index]
        index += 1
        if token & 0x80:
            count = (token & 0x7F) + 3
            value = data[index]
            index += 1
            out.extend([value] * count)
        else:
            count = token + 1
            out.extend(data[index:index + count])
            index += count
    return out


def expand_pal4(data: list[int], palette: tuple[int, ...], width: int, height: int) -> list[int]:
    pixels: list[int] = []
    row_bytes = (width + 1) // 2
    for row in range(height):
        base = row * row_bytes
        for col in range(width):
            byte = data[base + col // 2]
            index = byte >> 4 if (col & 1) == 0 else byte & 0x0F
            pixels.append(palette[index])
    return pixels


def write_r565(path: pathlib.Path, width: int, height: int, asset_id: int, pixels: list[int]) -> None:
    payload = bytearray()
    for pixel in pixels:
        payload.extend(struct.pack("<H", pixel))
    header = struct.pack("<4sBBHHHI", b"R565", 1, 0, width, height, asset_id, len(payload))
    path.write_bytes(header + payload)


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack info page images for LittleFS")
    parser.add_argument("output", nargs="?", default=str(DEFAULT_OUTPUT_DIR), help="Output directory")
    args = parser.parse_args()

    output_dir = pathlib.Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    for header_name, data_name, palette_name, width, height, asset_id, output_name in ASSETS:
        text = (INFO_DIR / header_name).read_text(encoding="utf-8")
        data = unpack_packbits(parse_array(text, "uint8_t", data_name))
        palette = GRAY4_LUT if palette_name is None else tuple(parse_array(text, "uint16_t", palette_name))
        pixels = expand_pal4(data, palette, width, height)
        out_path = output_dir / output_name
        write_r565(out_path, width, height, asset_id, pixels)
        print(f"wrote {out_path} ({width}x{height}, {len(pixels) * 2} pixel bytes)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
