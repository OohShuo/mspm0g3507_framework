#!/usr/bin/env python3
"""
Convert JPG/PNG images to R565 format for W25Q32 / ST7789.

Outputs the MCU's streaming RGB565 image format (.r565) — same binary layout
as `flash_manager.py upload-image` sends to the device.

Usage:
    python scripts/img2r565.py assets/images/bg.jpg --width 240 --height 240
    python scripts/img2r565.py logo.png -o /tmp/logo.r565 -W 120 -H 120 --fit contain
    python scripts/img2r565.py icon.png --mask  # also generate 1-bit alpha mask
"""

import argparse
import sys

from flashmgr.client import pack_image_asset


def main() -> int:
    p = argparse.ArgumentParser(description="Convert image to R565 (RGB565) asset")
    p.add_argument("input", help="Input JPG/PNG file")
    p.add_argument("-o", "--output", default=None, help="Output .r565 path (default: <input_stem>.r565)")
    p.add_argument("-W", "--width", type=int, default=240, help="Target width (default: 240)")
    p.add_argument("-H", "--height", type=int, default=240, help="Target height (default: 240)")
    p.add_argument("--fit", choices=["cover", "contain", "stretch"], default="cover",
                   help="Fit mode: cover (crop), contain (letterbox), stretch (default: cover)")
    p.add_argument("--mask", action="store_true", help="Also encode 1-bit alpha mask")
    args = p.parse_args()

    output_path = args.output
    if output_path is None:
        import os
        base = os.path.splitext(os.path.basename(args.input))[0]
        output_path = f"{base}.r565"

    print(f"Input:  {args.input}")
    print(f"Output: {output_path}")
    print(f"Size:   {args.width}×{args.height}  fit={args.fit}  mask={args.mask}")

    try:
        pack_image_asset(
            source_path=args.input,
            output_path=output_path,
            width=args.width,
            height=args.height,
            fit=args.fit,
            with_mask=args.mask,
        )
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    import os as _os
    size = _os.path.getsize(output_path)
    data_size = args.width * args.height * 2
    mask_size = ((args.width + 7) // 8) * args.height if args.mask else 0
    header_size = 16
    print(f"Wrote:  {size:,} bytes (header={header_size} + pixels={data_size:,} + mask={mask_size})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
