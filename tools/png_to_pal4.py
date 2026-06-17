#!/usr/bin/env python3
"""
Convert a 50×50 RGBA PNG to a C header with 16-color palette + 4-bit indexed data.

Output format:
  - palette: uint16_t[16] (RGB565, same as before)
  - data: uint8_t[N] (4-bit indices, 2 pixels per byte, row-major)

Usage:
  python png_to_pal4.py input.png output.h [--name my_image]
"""

import argparse
import struct
import sys
from PIL import Image
from collections import OrderedDict


def rgb_to_565(r, g, b):
    """Convert 8-bit R,G,B to 16-bit RGB565."""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def quantize_median_cut(pixels, num_colors=16):
    """
    Simple median-cut color quantizer.
    pixels: list of (r,g,b) tuples.
    Returns palette as list of (r,g,b) tuples.
    """
    if len(pixels) <= num_colors:
        # Fewer unique colors than palette size — use them directly
        unique = list(OrderedDict.fromkeys(pixels))
        # Pad to num_colors by repeating last
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
            r_range = r_max - r_min
            g_range = g_max - g_min
            b_range = b_max - b_min
            max_range = max(r_range, g_range, b_range)
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

    # Pad to exactly num_colors
    while len(palette) < num_colors:
        palette.append(palette[-1])

    return palette[:num_colors]


def find_nearest(pixel, palette):
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


def png_to_pal4(input_path, output_path, name="my_image", width=50, height=50):
    """Convert RGBA PNG to 16-color palette C header.

    Index 0 is always reserved for transparency (black, 0x0000).
    Indices 1–15 hold the quantized image colors.
    """
    img = Image.open(input_path).convert("RGBA")
    if img.size != (width, height):
        print(f"Warning: image is {img.size}, expected {width}x{height}")

    pixels = list(img.getdata())

    # Separate opaque and transparent pixels
    opaque_pixels = []
    for r, g, b, a in pixels:
        if a >= 64:
            opaque_pixels.append((r, g, b))

    # Index 0 → transparent (black, blends with black background)
    palette_rgb = [(0, 0, 0)]

    if not opaque_pixels:
        # All transparent — pad palette with black
        palette_rgb += [(0, 0, 0)] * 15
        indices = [0] * len(pixels)
    else:
        # Quantize opaque pixels to 15 colors (indices 1..15)
        colors_15 = quantize_median_cut(opaque_pixels, 15)
        palette_rgb += colors_15

        # Map each pixel
        indices = []
        for r, g, b, a in pixels:
            if a < 64:
                indices.append(0)  # transparent → index 0
            else:
                # Search in palette[1:] and add 1 to the result
                indices.append(find_nearest((r, g, b), palette_rgb))

    # Convert palette to RGB565
    palette_565 = [rgb_to_565(*c) for c in palette_rgb]

    # Pack 4-bit indices (2 per byte, row-major)
    row_bytes = (width + 1) // 2
    data_bytes = []
    for row in range(height):
        for col in range(0, width, 2):
            hi = indices[row * width + col]
            lo = indices[row * width + col + 1] if col + 1 < width else 0
            data_bytes.append((hi << 4) | lo)

    # Write C header
    name_upper = name.upper()
    with open(output_path, "w") as f:
        f.write("// Auto-generated from {} ({:d}x{:d})\n".format(
            input_path.replace("\\", "/").split("/")[-1], width, height))
        f.write("// {:d} pixels, 16-color palette + 4-bit indices (2 pixels per byte)\n".format(
            width * height))
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write("#define {}_W {:d}\n".format(name_upper, width))
        f.write("#define {}_H {:d}\n".format(name_upper, height))
        f.write("\n")

        # Palette
        f.write("static const uint16_t {}_palette[16] = {{".format(name))
        for i, c in enumerate(palette_565):
            if i % 8 == 0:
                f.write("\n    ")
            f.write("0x{:04x}u".format(c))
            if i < 15:
                f.write(", ")
        f.write("\n};\n\n")

        # Data
        f.write("static const uint8_t {}_data[{:d}] = {{".format(
            name, len(data_bytes)))
        for i, b in enumerate(data_bytes):
            if i % 16 == 0:
                f.write("\n    ")
            f.write("0x{:02x}u".format(b))
            if i < len(data_bytes) - 1:
                f.write(", ")
        f.write("\n};\n")

    print(f"Wrote {output_path}")
    print(f"  Palette: 16 colors × 2 bytes = 32 bytes")
    print(f"  Data: {len(data_bytes)} bytes (4-bit, 2 pixels/byte)")
    print(f"  Total: {32 + len(data_bytes)} bytes")


def main():
    parser = argparse.ArgumentParser(
        description="Convert RGBA PNG to 16-color palette C header")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("output", help="Output .h file")
    parser.add_argument("--name", default="my_image",
                        help="C identifier prefix (default: my_image)")
    parser.add_argument("--width", type=int, default=50,
                        help="Image width (default: 50)")
    parser.add_argument("--height", type=int, default=50,
                        help="Image height (default: 50)")
    args = parser.parse_args()

    png_to_pal4(args.input, args.output, args.name, args.width, args.height)


if __name__ == "__main__":
    main()
