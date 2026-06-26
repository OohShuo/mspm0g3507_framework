#!/usr/bin/env python3
"""
Build a LittleFS flash image for the VM from assets/vm_flash/ files.

Creates a 4 MiB image matching the W25Q32 layout:
  - Bytes 0 .. 1 MiB-1:   raw region (all 0xFF, for direct image caching)
  - Bytes 1 MiB .. 4 MiB:  LittleFS partition (3 MiB, 768 × 4 KiB blocks)

Usage:
    python scripts/mk_vm_flash.py                        # default paths
    python scripts/mk_vm_flash.py -o build/vm/vm_flash.bin
"""

import argparse
import os
import sys

FLASH_TOTAL = 4 * 1024 * 1024   # 4 MiB W25Q32
LFS_OFFSET  = 1 * 1024 * 1024   # LFS partition starts at 1 MiB
LFS_SIZE    = 3 * 1024 * 1024   # LFS partition = 3 MiB
BLOCK_SIZE  = 4096              # 4 KiB erase sector
READ_SIZE   = 256               # W25Q32 page size
PROG_SIZE   = 256
CACHE_SIZE  = 256
LOOKAHEAD   = 16
BLOCK_CYCLES = 100


def main() -> int:
    p = argparse.ArgumentParser(description="Build VM LittleFS flash image")
    p.add_argument("-o", "--output", default=None,
                   help="Output path (default: build/vm/vm_flash.bin)")
    p.add_argument("--assets-dir", default=None,
                   help="Assets directory (default: assets/vm_flash)")
    args = p.parse_args()

    # Resolve paths relative to the repo root
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_path = args.output or os.path.join(repo_root, "build", "vm", "vm_flash.bin")
    assets_dir = args.assets_dir or os.path.join(repo_root, "assets", "vm_flash")

    if not os.path.isdir(assets_dir):
        print(f"Error: assets directory not found: {assets_dir}", file=sys.stderr)
        return 1

    # Collect files to pack
    files_to_pack = []
    for name in sorted(os.listdir(assets_dir)):
        fpath = os.path.join(assets_dir, name)
        if os.path.isfile(fpath):
            files_to_pack.append((f"/{name}", fpath))

    if not files_to_pack:
        print("Warning: no files found in assets directory", file=sys.stderr)

    # Import littlefs
    try:
        from littlefs import LittleFS
        from littlefs.context import UserContext
    except ImportError:
        print("Error: littlefs-python is required: pip install littlefs-python", file=sys.stderr)
        return 1

    # Create LFS partition buffer (all 0xFF == erased flash)
    print(f"Creating {LFS_SIZE // (1024*1024)} MiB LittleFS partition "
          f"({LFS_SIZE // BLOCK_SIZE} blocks × {BLOCK_SIZE // 1024} KiB)...")
    lfs_buffer = bytearray([0xFF] * LFS_SIZE)
    context = UserContext(buffer=lfs_buffer)

    fs = LittleFS(
        context,
        mount=False,  # Don't auto-mount; format first
        block_size=BLOCK_SIZE,
        block_count=LFS_SIZE // BLOCK_SIZE,
        read_size=READ_SIZE,
        prog_size=PROG_SIZE,
        cache_size=CACHE_SIZE,
        lookahead_size=LOOKAHEAD,
        block_cycles=BLOCK_CYCLES,
    )

    # Format the filesystem
    print("Formatting...")
    fs.format()

    # Mount after format (required before file operations)
    fs.mount()

    # Add files
    print("Adding files:")
    for lfs_path, host_path in files_to_pack:
        with open(host_path, "rb") as fh:
            data = fh.read()
        with fs.open(lfs_path, "wb") as fh:
            fh.write(data)
        print(f"  {lfs_path}  ({len(data):,} bytes)")

    # Unmount to flush all data
    fs.unmount()

    # Build the full 4 MiB flash image
    print(f"\nAssembling {FLASH_TOTAL // (1024*1024)} MiB flash image...")
    raw_region = b'\xFF' * LFS_OFFSET
    lfs_data = bytes(lfs_buffer)
    full_image = raw_region + lfs_data
    assert len(full_image) == FLASH_TOTAL, \
        f"Image size {len(full_image)} != {FLASH_TOTAL}"

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as fh:
        fh.write(full_image)

    print(f"Wrote: {output_path}  ({len(full_image):,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
