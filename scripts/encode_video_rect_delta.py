#!/usr/bin/env python3
"""
Bad Apple Rect Delta encoder — converts video to BARD (.bard) rect5 delta format.

Usage:
    python tools/encode_video_rect_delta.py \\
        --input assets/badapple.mp4 \\
        --out-bard assets/badapple_240x180_24fps.bard \\
        --out-c src/app/video/badapple_builtin_video.h \\
        --symbol g_badapple_bard_builtin \\
        --width 240 --height 180 --display-x 0 --display-y 70 \\
        --fps 24 --max-seconds 5
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

# ═══════════════════════════════════════════════════════════════════
#  Argument parsing
# ═══════════════════════════════════════════════════════════════════

def parse_args():
    p = argparse.ArgumentParser(description="Encode video to BARD rect5 delta format")
    p.add_argument("--input", required=True, help="Input video file")
    p.add_argument("--out-bard", required=True, help="Output .bard file")
    p.add_argument("--out-c", default=None, help="Optional C header output")
    p.add_argument("--symbol", default="g_badapple_bard_builtin", help="C array symbol name")
    p.add_argument("--width", type=int, default=240, help="Encoded width (max 256 for rect5)")
    p.add_argument("--height", type=int, default=180, help="Encoded height (max 256 for rect5)")
    p.add_argument("--display-x", type=int, default=0, help="Screen X offset")
    p.add_argument("--display-y", type=int, default=70, help="Screen Y offset")
    p.add_argument("--fps", type=int, default=24, help="Output frame rate")
    p.add_argument("--threshold", type=int, default=128, help="Binarization threshold 0-255")
    p.add_argument("--fit", choices=["contain", "cover", "stretch"], default="contain")
    p.add_argument("--max-seconds", type=float, default=None, help="Limit to first N seconds")
    p.add_argument("--start", type=float, default=0.0, help="Start from N seconds")
    p.add_argument("--invert", action="store_true", help="Invert black/white")
    p.add_argument("--loop", action="store_true", default=True, help="Set header loop flag (default: on)")
    p.add_argument("--preview-dir", default=None, help="Export decoded verification frames as PNG")
    return p.parse_args()


# ═══════════════════════════════════════════════════════════════════
#  Video preprocessing
# ═══════════════════════════════════════════════════════════════════

def read_video_frames(args):
    """
    Read video via ffmpeg, resample to target FPS, resize, binarize.
    Extracts frames to a temp directory as PNG, then loads with PIL.

    Returns list of 2D numpy bool arrays (True=white, False=black).
    """
    # Check video exists
    if not os.path.isfile(args.input):
        print(f"ERROR: Input file not found: {args.input}")
        sys.exit(1)

    # Probe video duration with ffprobe
    try:
        result = subprocess.run(
            ["ffprobe", "-v", "quiet", "-show_entries", "format=duration",
             "-of", "csv=p=0", args.input],
            capture_output=True, text=True, timeout=15)
        duration = float(result.stdout.strip()) if result.stdout.strip() else 0
    except (ValueError, subprocess.TimeoutExpired):
        duration = 0

    print(f"Input: {args.input}")
    print(f"  Duration: {duration:.2f}s")

    if duration <= 0:
        print("ERROR: Cannot determine video duration")
        sys.exit(1)

    # Calculate target
    start_time = args.start
    end_time = duration if args.max_seconds is None else start_time + args.max_seconds
    frame_count = int((end_time - start_time) * args.fps)

    print(f"  Target: {frame_count} frames @ {args.fps} fps "
          f"({args.width}x{args.height})")

    # Extract frames to temp directory as PNG (avoids pixel format issues)
    tmpdir = tempfile.mkdtemp(prefix="bard_encode_")

    ffmpeg_cmd = [
        "ffmpeg",
        "-ss", str(start_time),
        "-t", str(end_time - start_time),
        "-i", args.input,
        "-vf", (
            f"fps={args.fps},"
            f"scale={args.width}:{args.height}:force_original_aspect_ratio=decrease,"
            f"pad={args.width}:{args.height}:(ow-iw)/2:(oh-ih)/2:black"
        ),
        "-v", "quiet",
        "-y",
        os.path.join(tmpdir, "frame_%04d.png")
    ]

    subprocess.run(ffmpeg_cmd, check=True, timeout=300)

    # Load frames from PNG
    png_files = sorted([
        f for f in os.listdir(tmpdir) if f.startswith("frame_") and f.endswith(".png")
    ])

    frames = []
    for i, png_name in enumerate(png_files):
        img = Image.open(os.path.join(tmpdir, png_name)).convert("L")
        gray = np.asarray(img, dtype=np.uint8).copy()
        img.close()
        frame_bw = gray >= args.threshold

        if args.invert:
            frame_bw = ~frame_bw

        frames.append(frame_bw)

        if (i + 1) % 50 == 0 or i == 0:
            print(f"  Preprocessing frame {i + 1}/{len(png_files)}")

    # Cleanup temp files
    import shutil
    shutil.rmtree(tmpdir, ignore_errors=True)

    if not frames:
        print("ERROR: No frames extracted — check ffmpeg command and input file")
        sys.exit(1)

    print(f"  Extracted {len(frames)} frames")
    return frames


# ═══════════════════════════════════════════════════════════════════
#  Rect5 delta encoding
# ═══════════════════════════════════════════════════════════════════

def encode_rects_for_color(changed_mask):
    """
    Generate rects for changed pixels of a single color.

    Phase 1: Scan each row for contiguous runs.
    Phase 2: Vertically merge runs with identical (x, width) on consecutive rows.

    Returns list of (x, y, w, h, color) tuples.
    """
    h, w = changed_mask.shape
    rects = []

    # Phase 1: Per-row runs
    runs_by_row = {}  # y -> list of (x, run_width)
    for y in range(h):
        row = changed_mask[y]
        x = 0
        row_runs = []
        while x < w:
            if row[x]:
                run_start = x
                while x < w and row[x]:
                    x += 1
                row_runs.append((run_start, x - run_start))
            else:
                x += 1
        if row_runs:
            runs_by_row[y] = row_runs

    if not runs_by_row:
        return rects

    # Phase 2: Vertical merge
    # Only merge runs on CONSECUTIVE rows. Gapped rows break the merge.
    active = {}  # key=(x, w) -> (y_start, y_last)
    prev_y = None

    for y in sorted(runs_by_row.keys()):
        new_active = {}
        row_run_set = set(runs_by_row.get(y, []))

        # Close any active strip that has a row gap (y > y_last + 1)
        for key, (y_start, y_last) in active.items():
            rx, rw = key
            # Check for gap: if current y is not adjacent to y_last, close it
            if y > y_last + 1:
                rects.append((rx, y_start, rw, y_last - y_start + 1))
            elif (rx, rw) in row_run_set:
                # Same run continues on consecutive row
                new_active[key] = (y_start, y)
            else:
                # Run stopped on this row — close it
                rects.append((rx, y_start, rw, y_last - y_start + 1))

        # Start new strips for runs not matched
        for (rx, rw) in row_run_set:
            if (rx, rw) not in active or prev_y is None or y > prev_y + 1:
                new_active[(rx, rw)] = (y, y)

        prev_y = y
        active = new_active

    # Close remaining
    for key, (y_start, y_end) in active.items():
        rx, rw = key
        rects.append((rx, y_start, rw, y_end - y_start + 1))

    return rects


def encode_frame_delta(prev_frame, curr_frame):
    """
    Encode one delta frame as rect5 list.
    Each rect is (x, y, w, h, color) where color 1=white, 0=black.
    """
    changed = prev_frame != curr_frame

    if not np.any(changed):
        return []  # Empty frame — no changes

    white_changed = changed & (curr_frame == True)
    black_changed = changed & (curr_frame == False)

    white_rects = encode_rects_for_color(white_changed)
    black_rects = encode_rects_for_color(black_changed)

    # Tag rects with their color
    result = []
    for (x, y, w, h) in white_rects:
        result.append((x, y, w, h, 1))
    for (x, y, w, h) in black_rects:
        result.append((x, y, w, h, 0))

    return result


# ═══════════════════════════════════════════════════════════════════
#  Verification
# ═══════════════════════════════════════════════════════════════════

def decode_frame(prev_recon, rects):
    """Reconstruct a frame from rects, applied on top of prev_recon."""
    recon = prev_recon.copy()
    for x, y, w, h, color in rects:
        recon[y:y + h, x:x + w] = (color == 1)
    return recon


def verify_encoding(frames, all_rects):
    """Verify that rect decoding reproduces every frame exactly."""
    if not frames:
        print("ERROR: No frames to verify")
        return False

    recon = np.zeros((frames[0].shape[0], frames[0].shape[1]), dtype=bool)
    for i, (frame, rects) in enumerate(zip(frames, all_rects)):
        recon = decode_frame(recon, rects)
        if not np.array_equal(recon, frame):
            mismatch = np.sum(recon != frame)
            print(f"ERROR: Frame {i} verification FAILED — {mismatch} pixels differ")
            return False
    print("Verification: ALL FRAMES PASS")
    return True


# ═══════════════════════════════════════════════════════════════════
#  BARD binary format (.bard)
# ═══════════════════════════════════════════════════════════════════

BARD_MAGIC = b"BARD"
BARD_HEADER_SIZE = 64
BARD_INDEX_ENTRY_SIZE = 8
BARD_RECT5_SIZE = 5
LFS_CAPACITY_HINT = 2 * 1024 * 1024  # 2 MiB LittleFS partition


def _u16le(v):
    return struct.pack("<H", v)


def _u32le(v):
    return struct.pack("<I", v)


def write_bard(frames, all_rects, args):
    """Write .bard file. Returns total file size in bytes."""
    frame_count = len(frames)
    encoded_w = args.width
    encoded_h = args.height

    # Validate rect5 dimensions
    if encoded_w > 256 or encoded_h > 256:
        print("ERROR: rect5 only supports encoded_width <= 256 and encoded_height <= 256.")
        print("Use --width 240 --height 180 --display-y 70, or implement rect6 first.")
        sys.exit(1)

    # Build frame payloads
    frame_data = []
    total_data_size = 0
    max_bytes = 0
    max_rects = 0
    total_rects = 0

    for rects in all_rects:
        buf = bytearray()
        buf += _u16le(len(rects))  # rect_count
        for x, y, w, h, color in rects:
            # Validate bounds
            if not (0 <= x < encoded_w and 0 <= y < encoded_h and
                    1 <= w <= encoded_w - x and 1 <= h <= encoded_h - y and
                    color in (0, 1)):
                print(f"ERROR: Invalid rect: x={x} y={y} w={w} h={h} color={color}")
                sys.exit(1)
            buf.append(x)
            buf.append(y)
            buf.append(w - 1)  # w_minus_1
            buf.append(h - 1)  # h_minus_1
            buf.append(color)
        frame_data.append(bytes(buf))
        total_data_size += len(buf)
        max_bytes = max(max_bytes, len(buf))
        max_rects = max(max_rects, len(rects))
        total_rects += len(rects)

    # Build frame index
    payload_offset = BARD_HEADER_SIZE + frame_count * BARD_INDEX_ENTRY_SIZE

    index_buf = bytearray()
    current_offset = payload_offset
    for fd in frame_data:
        index_buf += _u32le(current_offset)
        index_buf += _u32le(len(fd))
        current_offset += len(fd)

    # Build header (64 bytes, little-endian) per flash_mgr spec
    flags = 0x01  # bit0: loop (always loop for screensaver)
    if getattr(args, 'loop', True):
        flags |= 0x01

    header = bytearray(BARD_HEADER_SIZE)
    header[0:4]   = BARD_MAGIC                     # magic 'BARD'
    header[4:6]   = _u16le(1)                      # version
    header[6:8]   = _u16le(BARD_HEADER_SIZE)       # header_size
    header[8:10]  = _u16le(encoded_w)              # width
    header[10:12] = _u16le(encoded_h)              # height
    header[12:14] = _u16le(args.display_x)         # display_x
    header[14:16] = _u16le(args.display_y)         # display_y
    header[16:18] = _u16le(args.fps)               # fps_num
    header[18:20] = _u16le(1)                      # fps_den
    header[20:24] = _u32le(frame_count)            # frame_count
    header[24:28] = _u32le(BARD_HEADER_SIZE)       # index_offset (64)
    header[28:32] = _u32le(payload_offset)         # payload_offset
    header[32:36] = _u32le(payload_offset + total_data_size)  # total_size
    header[36:38] = _u16le(1)                      # rect_format = rect5
    header[38:40] = _u16le(flags)                  # flags
    header[40:44] = _u32le(0)                      # crc32 (reserved)
    # reserved[20] at 0x2C–0x3F — already zero

    # Write file
    out_dir = os.path.dirname(args.out_bard)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.out_bard, "wb") as f:
        f.write(bytes(header))
        f.write(bytes(index_buf))
        for fd in frame_data:
            f.write(fd)

    total_size = payload_offset + total_data_size

    # Print statistics
    print(f"\n─── Encoding Statistics ───")
    print(f"Encoded size: {encoded_w}x{encoded_h}")
    print(f"FPS: {args.fps}")
    print(f"Frame count: {frame_count}")
    print(f"Duration: {frame_count / args.fps:.2f}s")
    print(f"Total .bard size: {total_size} bytes ({total_size / 1024:.1f} KiB)")
    print(f"  Header: {BARD_HEADER_SIZE} bytes")
    print(f"  Index: {frame_count * BARD_INDEX_ENTRY_SIZE} bytes")
    print(f"  Data: {total_data_size} bytes ({total_data_size / 1024:.1f} KiB)")
    if frame_count > 0:
        print(f"Average: {total_data_size / frame_count:.1f} bytes/frame")
        print(f"Max: {max_bytes} bytes/frame")
        print(f"Average rects/frame: {total_rects / frame_count:.1f}")
        print(f"Max rects/frame: {max_rects}")
    print(f"LittleFS partition: ~{LFS_CAPACITY_HINT // 1024} KiB")
    if total_size > LFS_CAPACITY_HINT:
        print("WARNING: Video exceeds W25Q32 LittleFS partition!")
    else:
        print("OK: Video fits in W25Q32 LittleFS partition.")

    # Suggest upload command
    bard_name = os.path.basename(args.out_bard)
    print(f"\nUpload to device via flash_mgr:")
    print(f"  python scripts/flash_manager.py <PORT> upload {args.out_bard} /{bard_name}")
    print(f"  python scripts/flash_manager.py <PORT> info /{bard_name}")

    return total_size


# ═══════════════════════════════════════════════════════════════════
#  C header generation
# ═══════════════════════════════════════════════════════════════════

def write_c_header(bard_bytes, args):
    """Write a C header with the .bard file as a const uint8_t array."""
    size = len(bard_bytes)
    symbol = args.symbol
    define_name = symbol.upper() + "_SIZE"

    lines = []
    lines.append("// Auto-generated by tools/encode_video_rect_delta.py — DO NOT EDIT")
    lines.append(f"// Input: {args.input}")
    lines.append(f"// Encoded: {args.width}x{args.height} @ {args.fps} fps")
    lines.append(f"// Size: {size} bytes ({size / 1024:.1f} KiB)")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append(f"#define {define_name} {size}u")
    lines.append("")
    lines.append(f"const uint8_t {symbol}[] = {{")

    for i in range(0, size, 16):
        chunk = bard_bytes[i:i + 16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_vals},")

    lines.append("};")
    lines.append("")
    lines.append(f"const uint32_t {symbol}_size = {define_name};")
    lines.append("")

    out_dir = os.path.dirname(args.out_c)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.out_c, "w") as f:
        f.write("\n".join(lines))

    print(f"C header written to: {args.out_c}")


# ═══════════════════════════════════════════════════════════════════
#  Preview export
# ═══════════════════════════════════════════════════════════════════

def export_preview(frames, all_rects, args):
    """Export decoded verification frames as PNG for visual inspection."""
    os.makedirs(args.preview_dir, exist_ok=True)

    recon = np.zeros((args.height, args.width), dtype=bool)
    for i, (frame, rects) in enumerate(zip(frames, all_rects)):
        recon = decode_frame(recon, rects)
        img = Image.fromarray((recon.astype(np.uint8)) * 255, mode="L")
        path = os.path.join(args.preview_dir, f"frame_{i:04d}.png")
        img.save(path)
        img.close()

    print(f"Preview frames exported to: {args.preview_dir}")


# ═══════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════

def main():
    args = parse_args()

    # 1. Read and preprocess video
    frames = read_video_frames(args)
    if not frames:
        print("ERROR: No frames extracted")
        sys.exit(1)

    print(f"Encoding {len(frames)} frames to rect5 delta...")

    # 2. Encode delta frames
    prev = np.zeros((args.height, args.width), dtype=bool)
    all_rects = []
    for i, frame in enumerate(frames):
        rects = encode_frame_delta(prev, frame)
        all_rects.append(rects)
        prev = frame.copy()

        if (i + 1) % 50 == 0:
            print(f"  Encoded {i + 1}/{len(frames)}")

    # 3. Verify encoding correctness
    if not verify_encoding(frames, all_rects):
        print("ERROR: Encoding verification failed — aborting")
        sys.exit(1)

    # 4. Write .bard file
    write_bard(frames, all_rects, args)

    # 5. Write C header if requested
    if args.out_c:
        with open(args.out_bard, "rb") as f:
            bard_bytes = f.read()
        write_c_header(bard_bytes, args)

    # 6. Export preview frames if requested
    if args.preview_dir:
        export_preview(frames, all_rects, args)

    print("\nDone!")


if __name__ == "__main__":
    main()
