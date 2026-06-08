#!/usr/bin/env python3
"""
Process a video file for 1-bit delta playback on an ST7789 LCD (240x240).

Pipeline:
  1. ffmpeg  → 240×240 grayscale PNG frames
  2. encode_badapple.py → 1-bit delta C header

Memory budget:
  MSPM0G3507: 128 KB flash total, ~22 KB for code → ~106 KB for delta data.
  The script warns if the generated data exceeds this budget.

Usage:
  # Basic — auto-calculates max frames from budget
  python tools/process_video.py --input my_video.mp4

  # Full control
  python tools/process_video.py --input video.mp4 --fps 6 --start 10 --max-frames 50

  # Keep intermediate PNGs for inspection
  python tools/process_video.py --input clip.mp4 --keep-frames
"""

import argparse
import os
import subprocess
import sys

FLASH_BUDGET_BYTES = 106 * 1024  # 108544 — conservative after ~22 KB code


# ═══════════════════════════════════════════════════════════════════
#  Helpers
# ═══════════════════════════════════════════════════════════════════

def ffmpeg_available() -> bool:
    try:
        subprocess.run(["ffmpeg", "-version"], capture_output=True, check=True)
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def probe_video(path: str) -> dict:
    """Return {width, height, fps, frames} from ffprobe."""
    meta = {"width": 0, "height": 0, "fps": 0, "frames": 0}
    try:
        r = subprocess.run(
            ["ffprobe", "-v", "error", "-select_streams", "v:0",
             "-show_entries", "stream=width,height,r_frame_rate,nb_frames",
             "-of", "csv=p=0", path],
            capture_output=True, text=True, timeout=30)
        parts = r.stdout.strip().split(",")
        if len(parts) >= 4 and parts[0].isdigit():
            meta["width"] = int(parts[0])
            meta["height"] = int(parts[1])
            fps_frac = parts[2].split("/")
            meta["fps"] = round(int(fps_frac[0]) / int(fps_frac[1])) if len(fps_frac) == 2 else int(fps_frac[0])
            meta["frames"] = int(parts[3]) if parts[3].isdigit() else 0
    except Exception:
        pass
    return meta


# ═══════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="Process video for 1-bit delta playback on ST7789 LCD")
    parser.add_argument("--input", "-i", required=True,
                        help="Input video file")
    parser.add_argument("--output-name", "-n",
                        help="C array name (default: derived from filename)")
    parser.add_argument("--fps", type=int, default=4,
                        help="Target frame rate (default: 4)")
    parser.add_argument("--start", type=int, default=1,
                        help="First frame to encode, 1-based (default: 1)")
    parser.add_argument("--max-frames", type=int, default=0,
                        help="Max frames to encode (0 = auto-calculate from budget)")
    parser.add_argument("--max-size", type=int, default=FLASH_BUDGET_BYTES,
                        help=f"Max delta data bytes (default: {FLASH_BUDGET_BYTES})")
    parser.add_argument("--frames-dir", default="img",
                        help="Directory for extracted PNG frames (default: img/)")
    parser.add_argument("--header", default="src/hal/lcd/lcd_video_delta_data.h",
                        help="Output C header path")
    parser.add_argument("--keep-frames", action="store_true",
                        help="Keep extracted PNGs after encoding")
    args = parser.parse_args()

    # ── Validate ──
    if not os.path.isfile(args.input):
        print(f"Error: '{args.input}' not found.", file=sys.stderr)
        sys.exit(1)
    if not ffmpeg_available():
        print("Error: ffmpeg not found on PATH.", file=sys.stderr)
        sys.exit(1)

    # ── Derive array name from filename if not given ──
    stem = os.path.splitext(os.path.basename(args.input))[0]
    if args.output_name is None:
        safe = stem.lower().replace("-", "_").replace(" ", "_").replace(".", "_")
        args.output_name = f"g_{safe}_delta"

    # ── Probe source ──
    meta = probe_video(args.input)
    print(f"Input:  {args.input}")
    print(f"        {meta['width']}×{meta['height']}  ~{meta['fps']} fps  "
          f"{meta['frames'] if meta['frames'] else 'unknown'} frames")

    # ── 1. Extract frames ──
    os.makedirs(args.frames_dir, exist_ok=True)
    pattern = os.path.join(args.frames_dir, "frame_%05d.png")
    print(f"\n[1/3] Extracting frames  →  {pattern}  @ {args.fps} fps")

    subprocess.run([
        "ffmpeg", "-i", args.input,
        "-vf", f"fps={args.fps},scale=240:240:flags=lanczos,format=gray",
        "-start_number", "1", pattern,
        "-y", "-loglevel", "error",
    ], check=True)

    frame_files = sorted(
        f for f in os.listdir(args.frames_dir)
        if f.startswith("frame_") and f.endswith(".png")
    )
    total = len(frame_files)
    if total == 0:
        print("Error: no frames extracted.", file=sys.stderr)
        sys.exit(1)
    print(f"        {total} frames extracted")

    # ── 2. Determine encode range ──
    start = max(1, args.start)
    available = total - (start - 1)
    if available <= 0:
        print(f"Error: --start {args.start} exceeds total frames ({total}).", file=sys.stderr)
        sys.exit(1)

    if args.max_frames > 0:
        encode_count = min(args.max_frames, available)
    else:
        # Heuristic: ~4000 bytes/frame is a safe overestimate for 240×240 1-bit delta
        encode_count = min(max(1, args.max_size // 4000), available)

    print(f"\n[2/3] Encoding {encode_count} delta frames  (start={start})")
    print(f"      array name: {args.output_name}")

    encoder = os.path.join(os.path.dirname(__file__), "encode_badapple.py")
    subprocess.run([
        sys.executable, encoder,
        "--input", args.frames_dir,
        "--output", args.header,
        "--name", args.output_name,
        "--start-frame", str(start),
        "--max-frames", str(encode_count),
        "--fps", str(args.fps),
    ], check=True)

    # ── 3. Verify size ──
    total_bytes = 0
    try:
        with open(args.header) as f:
            for line in f:
                if line.startswith("// Total delta bytes:"):
                    total_bytes = int(line.split(":")[1].strip())
                    break
    except (OSError, ValueError):
        pass

    print(f"\n{'='*55}")
    print(f"[3/3] Delta data  {total_bytes:,} bytes  ({total_bytes/1024:.1f} KB)")
    if total_bytes > args.max_size:
        over = total_bytes - args.max_size
        print(f"⚠️  OVER BUDGET by {over:,} bytes!  Budget: {args.max_size:,} bytes")
        print(f"    Reduce --fps, --start, or --max-frames.")
    else:
        pct = total_bytes / args.max_size * 100
        print(f"✅  Within budget ({pct:.1f}% of {args.max_size/1024:.1f} KB)")

    # ── Cleanup ──
    if not args.keep_frames:
        for f in frame_files:
            os.remove(os.path.join(args.frames_dir, f))
        print(f"\nCleaned up {len(frame_files)} extracted frames.")
    else:
        print(f"\nFrames kept in {args.frames_dir}/")

    # ── Usage hint ──
    print(f"\nOutput: {args.header}")
    print(f"\nTo use in lcd_test.c:")
    print(f"    Lcd_Video_Play(g_lcd, {args.output_name}_data, {args.output_name}_frames,")
    print(f"                   {encode_count}U, {args.fps}U, 0);")


if __name__ == "__main__":
    main()
