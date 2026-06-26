#!/usr/bin/env python3
"""
Convert R565 files back to JPG/PNG images.

Usage:
    python scripts/r5652img.py nl.r565
    python scripts/r5652img.py nl.r565 -o output.png
    python scripts/r5652img.py nl.r565 --format jpg
"""

import argparse
import struct
import sys

HEADER_SIZE = 16
MAGIC = b"R565"
VERSION = 1
FLAG_MASK = 0x01


def r565_to_rgb888(pixel_le: bytes) -> tuple[int, int, int]:
    """Convert a single RGB565 (little-endian) pixel to RGB888."""
    raw = pixel_le[0] | (pixel_le[1] << 8)
    r = (raw >> 8) & 0xF8
    g = (raw >> 3) & 0xFC
    b = (raw << 3) & 0xF8
    return (r, g, b)


def read_r565(path: str):
    """Parse a .r565 file, return (pixels_rgb888_list, width, height)."""
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        raise ValueError(f"File too small ({len(data)} bytes), need at least {HEADER_SIZE}")

    magic, version, flags, width, height, crc16, pixel_data_size = struct.unpack_from(
        "<4sBBHHHI", data, 0
    )

    if magic != MAGIC:
        raise ValueError(f"Bad magic: {magic!r}, expected {MAGIC!r}")
    if version != VERSION:
        raise ValueError(f"Unsupported version: {version}, expected {VERSION}")

    expected_pixel_size = width * height * 2
    if pixel_data_size != expected_pixel_size:
        raise ValueError(
            f"Pixel data size mismatch: header says {pixel_data_size}, "
            f"but {width}×{height}×2 = {expected_pixel_size}"
        )

    total_expected = HEADER_SIZE + expected_pixel_size
    has_mask = (flags & FLAG_MASK) != 0
    if has_mask:
        mask_size = ((width + 7) // 8) * height
        total_expected += mask_size

    if len(data) < total_expected:
        raise ValueError(
            f"File truncated: {len(data)} bytes, expected at least {total_expected}"
        )

    pixel_data = data[HEADER_SIZE:HEADER_SIZE + expected_pixel_size]
    pixels = []
    for i in range(0, len(pixel_data), 2):
        pixels.append(r565_to_rgb888(pixel_data[i:i + 2]))

    return pixels, width, height


def main() -> int:
    p = argparse.ArgumentParser(description="Convert R565 asset back to image")
    p.add_argument("input", help="Input .r565 file")
    p.add_argument("-o", "--output", default=None, help="Output image path")
    p.add_argument(
        "--format", choices=["jpg", "png"], default=None,
        help="Output format (default: inferred from extension, falls back to jpg)"
    )
    args = p.parse_args()

    try:
        from PIL import Image
    except ImportError as exc:
        print("Error: Pillow is required: pip install pillow", file=sys.stderr)
        return 1

    try:
        pixels, width, height = read_r565(args.input)
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(f"R565:  {args.input}")
    print(f"Size:  {width}×{height}  pixels={len(pixels):,}")

    # Build output path
    output_path = args.output
    if output_path is None:
        import os
        base = os.path.splitext(os.path.basename(args.input))[0]
        fmt = args.format or "jpg"
        output_path = f"{base}.{fmt}"
    else:
        fmt = args.format
        if fmt is None:
            ext = output_path.rsplit(".", 1)[-1].lower()
            if ext in ("jpg", "jpeg", "png"):
                fmt = "jpg" if ext in ("jpg", "jpeg") else "png"
            else:
                fmt = "jpg"

    # Create image
    img = Image.new("RGB", (width, height))
    img.putdata(pixels)

    save_kwargs = {}
    if fmt == "jpg":
        save_kwargs["quality"] = 95
    img.save(output_path, format="JPEG" if fmt == "jpg" else "PNG".upper(), **save_kwargs)

    import os as _os
    size = _os.path.getsize(output_path)
    print(f"Wrote: {output_path} ({size:,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
