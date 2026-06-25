# Bad Apple Rect Delta 1-bit Screensaver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a space-efficient Bad Apple video screensaver using rect5 delta encoding (BARD format), with both builtin C array and W25Q32 raw flash playback sources.

**Architecture:** New Python script (`tools/encode_video_rect_delta.py`) encodes video to rect5 delta format (.bard) with frame index. New MCU module (`src/app/video/`) decodes BARD format and draws rectangles via `Game_Graphics_Fill_Rect`. A new "SCREENSAVER" menu entry in the game registry lets users choose between existing PIPES and new BAD APPLE modes.

**Tech Stack:** Python 3 + OpenCV for encoding; C (C99) for MCU playback; W25Q32 raw flash via `Storage_Raw_*` APIs; ST7789 display via `Game_Graphics_Fill_Rect`.

**Spec reference:** `badapple_rect_delta_prompt.md` (1120 lines, in repo root)

---

## File Structure

```
NEW FILES:
  tools/encode_video_rect_delta.py           # Video → .bard + C header encoder
  src/app/video/badapple_video.h             # Player public API
  src/app/video/badapple_video.c             # Player: BARD parse, time-driven decode, rect drawing
  src/app/video/badapple_builtin_video.h     # Generated: short demo clip as C array
  src/app/games/screensaver_select/          # New screensaver selection menu
    screensaver_select.h
    screensaver_select.c

MODIFIED FILES:
  config/app_config.h                        # Add BadApple feature flags + flash addresses
  src/bsp/time/bsp_time.h                    # Add Bsp_Get_Time_Ms() inline wrapper
  src/app/game_console/game_registry.h       # Add game_icon_screensaver, game_id_screensaver
  src/app/game_console/game_registry.c       # Register SCREENSAVER entry
```

---

### Task 1: Add `Bsp_Get_Time_Ms()` compatibility wrapper

**Files:**
- Modify: `src/bsp/time/bsp_time.h`

**Why:** The spec requires `Bsp_Get_Time_Ms()` for playback timing, but only `Bsp_Get_Tick_Ms()` exists. The FreeRTOS tick rate is 1 kHz, so tick count == milliseconds.

- [ ] **Step 1: Add inline wrapper to bsp_time.h**

Append after the existing `Bsp_Get_Tick_Ms()` declaration:

```c
/* Compatibility alias — FreeRTOS tick is 1 kHz, so tick count equals milliseconds. */
static inline uint32_t Bsp_Get_Time_Ms(void) {
    return Bsp_Get_Tick_Ms();
}
```

- [ ] **Step 2: Verify compilation**

Run: check that nothing breaks (the function is static inline, so it's only compiled when referenced).

No commit yet — this is infrastructure for later tasks.

---

### Task 2: Add feature flags and flash address defines to app_config.h

**Files:**
- Modify: `config/app_config.h`

- [ ] **Step 1: Add BadApple configuration macros**

Append after the existing `AIR_BATTLE_BG_CACHE_*` defines:

```c
/* ── Bad Apple video ── */
#define BADAPPLE_VIDEO_USE_BUILTIN          1
#define BADAPPLE_VIDEO_USE_RAW_FLASH        1
#define BADAPPLE_VIDEO_ENABLE_RAW_INSTALL   1

/* Raw flash slot: first 768 KiB of the 2 MiB raw area.
   AIR_BATTLE_BG_CACHE starts at 1 MiB, so these do not overlap. */
#define BADAPPLE_VIDEO_RAW_ADDRESS          (0u * 1024u)
#define BADAPPLE_VIDEO_RAW_CAPACITY         (768u * 1024u)
```

- [ ] **Step 2: Commit**

```bash
git add config/app_config.h src/bsp/time/bsp_time.h
git commit -m "feat: add Bsp_Get_Time_Ms wrapper and BadApple feature flags"
```

---

### Task 3: Create video encoding script (Part 1 — video preprocessing)

**Files:**
- Create: `tools/encode_video_rect_delta.py`

- [ ] **Step 1: Create script skeleton with argument parsing**

```python
#!/usr/bin/env python3
"""
Bad Apple Rect Delta encoder — converts video to BARD (.bard) rect5 delta format.

Usage:
    python tools/encode_video_rect_delta.py \
        --input assets/badapple.mp4 \
        --out-bard assets/badapple_240x180_24fps.bard \
        --out-c src/app/video/badapple_builtin_video.h \
        --symbol g_badapple_bard_builtin \
        --width 240 --height 180 --display-x 0 --display-y 70 \
        --fps 24 --max-seconds 5
"""

import argparse
import struct
import sys
import os
import numpy as np

try:
    import cv2
except ImportError:
    print("ERROR: opencv-python required. Run: pip install opencv-python pillow numpy")
    sys.exit(1)

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
    p.add_argument("--preview-dir", default=None, help="Export decoded verification frames as PNG")
    return p.parse_args()
```

- [ ] **Step 2: Implement video reading and frame extraction**

```python
def read_video_frames(args):
    """
    Read video, resample to target FPS, resize, binarize.
    Returns list of 2D numpy bool arrays (True=white, False=black).
    """
    cap = cv2.VideoCapture(args.input)
    if not cap.isOpened():
        print(f"ERROR: Cannot open video: {args.input}")
        sys.exit(1)

    src_fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    duration = total_frames / src_fps if src_fps > 0 else 0
    print(f"Input: {args.input}")
    print(f"  Source FPS: {src_fps:.2f}, frames: {total_frames}, duration: {duration:.2f}s")

    # Frame timing
    frame_interval = 1.0 / args.fps
    start_time = args.start
    end_time = duration if args.max_seconds is None else start_time + args.max_seconds

    frames = []
    timestamps = []
    t = start_time
    while t < end_time:
        timestamps.append(t)
        t += frame_interval

    print(f"  Target: {len(timestamps)} frames @ {args.fps} fps")

    for i, target_t in enumerate(timestamps):
        # Seek to target time
        cap.set(cv2.CAP_PROP_POS_MSEC, target_t * 1000.0)
        ret, frame = cap.read()
        if not ret:
            print(f"WARNING: Could not read frame at t={target_t:.3f}s, using black")
            frame_bw = np.zeros((args.height, args.width), dtype=bool)
        else:
            # Convert to grayscale
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # Resize
            if args.fit == "contain":
                # Scale to fit, preserving aspect ratio, pad with black
                src_h, src_w = gray.shape
                scale = min(args.width / src_w, args.height / src_h)
                new_w = int(src_w * scale)
                new_h = int(src_h * scale)
                resized = cv2.resize(gray, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
                canvas = np.zeros((args.height, args.width), dtype=np.uint8)
                x_off = (args.width - new_w) // 2
                y_off = (args.height - new_h) // 2
                canvas[y_off:y_off+new_h, x_off:x_off+new_w] = resized
                gray = canvas
            elif args.fit == "cover":
                src_h, src_w = gray.shape
                scale = max(args.width / src_w, args.height / src_h)
                new_w = int(src_w * scale)
                new_h = int(src_h * scale)
                resized = cv2.resize(gray, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
                x_off = (new_w - args.width) // 2
                y_off = (new_h - args.height) // 2
                gray = resized[y_off:y_off+args.height, x_off:x_off+args.width]
            else:  # stretch
                gray = cv2.resize(gray, (args.width, args.height), interpolation=cv2.INTER_LINEAR)

            # Threshold
            _, bw = cv2.threshold(gray, args.threshold, 255, cv2.THRESH_BINARY)
            frame_bw = bw > 0  # True = white

        if args.invert:
            frame_bw = ~frame_bw

        frames.append(frame_bw)

        if (i + 1) % 50 == 0 or i == 0:
            print(f"  Frame {i+1}/{len(timestamps)}")

    cap.release()
    return frames
```

- [ ] **Step 3: Commit**

```bash
git add tools/encode_video_rect_delta.py
git commit -m "feat: add video rect delta encoder - argument parsing and preprocessing"
```

---

### Task 4: Create video encoding script (Part 2 — rect5 delta encoding)

**Files:**
- Modify: `tools/encode_video_rect_delta.py`

- [ ] **Step 1: Implement run-generation and vertical merging**

Append to the script:

```python
# ── Rect5 encoding ──

def encode_rects_for_color(changed_mask, target_color, curr_frame):
    """
    Generate rects for a specific color change.
    
    changed_mask: bool array, True where pixel needs to change to target_color
    target_color: 0 or 1
    curr_frame: current frame bool array (for safe-expansion validation)
    
    Returns list of (x, y, w, h, color).
    """
    h, w = changed_mask.shape
    rects = []
    
    # Phase 1: Scan rows for runs
    runs_by_row = {}  # y -> list of (x, run_w)
    for y in range(h):
        row = changed_mask[y]
        x = 0
        while x < w:
            if row[x]:
                run_start = x
                while x < w and row[x]:
                    x += 1
                run_w = x - run_start
                if y not in runs_by_row:
                    runs_by_row[y] = []
                runs_by_row[y].append((run_start, run_w))
            else:
                x += 1
    
    if not runs_by_row:
        return rects
    
    # Phase 2: Vertical merge — same x, same w, same color, consecutive y
    # Track active vertical strips
    active = {}  # key=(x, w, color) -> (y_start, y_current)
    
    for y in sorted(runs_by_row.keys()):
        new_active = {}
        row_runs = {(rx, rw): True for rx, rw in runs_by_row.get(y, [])}
        
        # Process existing active strips
        for key, (y_start, _) in active.items():
            if row_runs.get(key):
                # Continue the strip
                new_active[key] = (y_start, y)
            else:
                # Close the strip
                x, rw = key
                rects.append((x, y_start, rw, y - y_start + 1, target_color))
        
        # Start new strips for runs not matched
        for (rx, rw) in row_runs:
            if (rx, rw) not in active:
                new_active[(rx, rw)] = (y, y)
        
        active = new_active
    
    # Close remaining active strips
    for key, (y_start, y_end) in active.items():
        x, rw = key
        rects.append((x, y_start, rw, y_end - y_start + 1, target_color))
    
    return rects


def encode_frame_delta(prev_frame, curr_frame):
    """
    Encode one delta frame.
    prev_frame, curr_frame: bool 2D arrays (True=white)
    Returns list of BardRect5 tuples: (x, y, w, h, color)
    """
    h, w = curr_frame.shape
    changed = prev_frame != curr_frame
    
    if not np.any(changed):
        return []  # No change — empty frame
    
    white_changed = changed & (curr_frame == True)
    black_changed = changed & (curr_frame == False)
    
    rects = []
    rects.extend(encode_rects_for_color(white_changed, 1, curr_frame))
    rects.extend(encode_rects_for_color(black_changed, 0, curr_frame))
    
    return rects
```

- [ ] **Step 2: Implement encoding verification and .bard serialization**

```python
# ── Verification ──

def decode_frame(prev_recon, rects):
    """Reconstruct frame from rects, starting from prev_recon."""
    recon = prev_recon.copy()
    for x, y, w, h, color in rects:
        recon[y:y+h, x:x+w] = (color == 1)
    return recon


def verify_encoding(frames, all_rects):
    """Verify that rect decoding reproduces original frames exactly."""
    recon = np.zeros((frames[0].shape[0], frames[0].shape[1]), dtype=bool)
    for i, (frame, rects) in enumerate(zip(frames, all_rects)):
        recon = decode_frame(recon, rects)
        if not np.array_equal(recon, frame):
            mismatch = np.sum(recon != frame)
            print(f"ERROR: Frame {i} verification FAILED — {mismatch} pixels differ")
            return False
    print("Verification: ALL FRAMES PASS")
    return True


# ── BARD binary format ──

BARD_MAGIC = b"BARD"
BARD_HEADER_SIZE = 64
BARD_INDEX_ENTRY_SIZE = 8
BARD_RECT5_SIZE = 5


def _u16le(v):
    return struct.pack("<H", v)


def _u32le(v):
    return struct.pack("<I", v)


def write_bard(frames, all_rects, args):
    """Write .bard file."""
    frame_count = len(frames)
    encoded_w = args.width
    encoded_h = args.height
    
    # Validate rect5 dimensions
    if encoded_w > 256 or encoded_h > 256:
        print("ERROR: rect5 only supports encoded_width <= 256 and encoded_height <= 256.")
        print("Use --width 240 --height 180 --display-y 70, or implement rect6 first.")
        sys.exit(1)
    
    # Build frame data blobs
    frame_data = []
    total_data_size = 0
    max_bytes = 0
    max_rects = 0
    total_rects = 0
    
    for rects in all_rects:
        buf = bytearray()
        buf += _u16le(len(rects))  # rect_count
        for x, y, w, h, color in rects:
            assert 0 <= x < encoded_w
            assert 0 <= y < encoded_h
            assert 1 <= w <= encoded_w - x
            assert 1 <= h <= encoded_h - y
            assert color in (0, 1)
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
    data_offset = BARD_HEADER_SIZE + frame_count * BARD_INDEX_ENTRY_SIZE
    
    index_buf = bytearray()
    current_offset = data_offset
    for fd in frame_data:
        index_buf += _u32le(current_offset)
        index_buf += _u32le(len(fd))
        current_offset += len(fd)
    
    # Build header
    flags = 0x03  # bit0: loop, bit1: first frame assumes black screen
    
    header = bytearray(BARD_HEADER_SIZE)
    header[0:4] = BARD_MAGIC
    header[4] = 1       # version
    header[5] = 1       # rect_format = rect5
    header[6:8] = _u16le(BARD_HEADER_SIZE)
    header[8:10] = _u16le(encoded_w)
    header[10:12] = _u16le(encoded_h)
    header[12:14] = _u16le(args.width)   # screen_width (same as encoded when rect5)
    header[14:16] = _u16le(320)          # screen_height
    header[16:18] = _u16le(args.display_x)
    header[18:20] = _u16le(args.display_y)
    header[20:22] = _u16le(args.fps)
    header[22:24] = _u16le(1)            # fps_den
    header[24:28] = _u32le(frame_count)
    header[28:32] = _u32le(BARD_HEADER_SIZE)  # index_offset
    header[32:36] = _u32le(BARD_INDEX_ENTRY_SIZE)
    header[36:40] = _u32le(data_offset)
    header[40:44] = _u32le(data_offset + total_data_size)  # total_size
    header[44:48] = _u32le(0)            # payload_crc32 (optional, 0 for now)
    header[48:52] = _u32le(flags)
    # reserved[16] — already zero
    
    # Write file
    with open(args.out_bard, "wb") as f:
        f.write(bytes(header))
        f.write(bytes(index_buf))
        for fd in frame_data:
            f.write(fd)
    
    total_size = data_offset + total_data_size
    
    # Print statistics
    print(f"\n─── Encoding Statistics ───")
    print(f"Encoded size: {encoded_w}x{encoded_h}")
    print(f"FPS: {args.fps}")
    print(f"Frame count: {frame_count}")
    print(f"Duration: {frame_count / args.fps:.2f}s")
    print(f"Total .bard size: {total_size} bytes ({total_size/1024:.1f} KiB)")
    print(f"  Header: {BARD_HEADER_SIZE} bytes")
    print(f"  Index: {frame_count * BARD_INDEX_ENTRY_SIZE} bytes")
    print(f"  Data: {total_data_size} bytes ({total_data_size/1024:.1f} KiB)")
    print(f"Average: {total_data_size/frame_count:.1f} bytes/frame")
    print(f"Max: {max_bytes} bytes/frame")
    print(f"Average rects/frame: {total_rects/frame_count:.1f}")
    print(f"Max rects/frame: {max_rects}")
    print(f"Estimated raw flash slot usage: {total_size} bytes / {BADAPPLE_VIDEO_RAW_CAPACITY} bytes")
    
    BADAPPLE_VIDEO_RAW_CAPACITY = 768 * 1024
    if total_size > BADAPPLE_VIDEO_RAW_CAPACITY:
        print("WARNING: Video does NOT fit in BADAPPLE_VIDEO_RAW_CAPACITY!")
    else:
        print("Video fits in BADAPPLE_VIDEO_RAW_CAPACITY.")
    
    return total_size
```

- [ ] **Step 3: Commit**

```bash
git add tools/encode_video_rect_delta.py
git commit -m "feat: add rect5 delta encoding, verification, and .bard serialization"
```

---

### Task 5: Create video encoding script (Part 3 — C header output and main)

**Files:**
- Modify: `tools/encode_video_rect_delta.py`

- [ ] **Step 1: Implement C header generation**

```python
# ── C header output ──

def write_c_header(bard_bytes, args):
    """Write a C header with the .bard file as a const uint8_t array."""
    size = len(bard_bytes)
    symbol = args.symbol
    
    lines = []
    lines.append("// Auto-generated by tools/encode_video_rect_delta.py — DO NOT EDIT")
    lines.append(f"// Input: {args.input}")
    lines.append(f"// Size: {size} bytes, {size/1024:.1f} KiB")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append(f"#define {symbol.upper()}_SIZE {size}u")
    lines.append("")
    lines.append(f"const uint8_t {symbol}[] = {{")
    
    for i in range(0, size, 16):
        chunk = bard_bytes[i:i+16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_vals},")
    
    lines.append("};")
    lines.append("")
    lines.append(f"const uint32_t {symbol}_size = {symbol.upper()}_SIZE;")
    lines.append("")
    
    out_dir = os.path.dirname(args.out_c)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    
    with open(args.out_c, "w") as f:
        f.write("\n".join(lines))
    
    print(f"C header written to: {args.out_c}")


# ── Preview export ──

def export_preview(frames, all_rects, args):
    """Export decoded verification frames as PNG for inspection."""
    os.makedirs(args.preview_dir, exist_ok=True)
    
    recon = np.zeros((args.height, args.width), dtype=bool)
    for i, (frame, rects) in enumerate(zip(frames, all_rects)):
        recon = decode_frame(recon, rects)
        # Convert bool to 0/255 uint8
        img = (recon.astype(np.uint8)) * 255
        path = os.path.join(args.preview_dir, f"frame_{i:04d}.png")
        cv2.imwrite(path, img)
    
    print(f"Preview frames exported to: {args.preview_dir}")
```

- [ ] **Step 2: Implement main() function**

```python
# ── Main ──

def main():
    args = parse_args()
    
    # Read and preprocess
    frames = read_video_frames(args)
    if not frames:
        print("ERROR: No frames extracted")
        sys.exit(1)
    
    print(f"Encoding {len(frames)} frames to rect5 delta...")
    
    # Encode delta frames
    prev = np.zeros((args.height, args.width), dtype=bool)
    all_rects = []
    for i, frame in enumerate(frames):
        rects = encode_frame_delta(prev, frame)
        all_rects.append(rects)
        prev = frame.copy()
        
        if (i + 1) % 50 == 0:
            print(f"  Encoded {i+1}/{len(frames)}")
    
    # Verify
    if not verify_encoding(frames, all_rects):
        sys.exit(1)
    
    # Write .bard
    write_bard(frames, all_rects, args)
    
    # Write C header if requested
    if args.out_c:
        # Read back the .bard file we just wrote
        with open(args.out_bard, "rb") as f:
            bard_bytes = f.read()
        write_c_header(bard_bytes, args)
    
    # Export preview if requested
    if args.preview_dir:
        export_preview(frames, all_rects, args)
    
    print("\nDone!")


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Test the script with a short clip**

```bash
python tools/encode_video_rect_delta.py \
    --input assets/badapple.mp4 \
    --out-bard /tmp/test.bard \
    --width 240 --height 180 --display-x 0 --display-y 70 \
    --fps 24 --max-seconds 3
```

Expected: script prints statistics, verification passes, /tmp/test.bard is created.

- [ ] **Step 4: Commit**

```bash
git add tools/encode_video_rect_delta.py
git commit -m "feat: add C header output, preview export, and main for encode script"
```

---

### Task 6: Generate builtin video C header (short demo clip)

**Files:**
- Create: `src/app/video/badapple_builtin_video.h`

- [ ] **Step 1: Generate the header**

First download or place a Bad Apple video at `assets/badapple.mp4`, then:

```bash
python tools/encode_video_rect_delta.py \
    --input assets/badapple.mp4 \
    --out-bard assets/badapple_240x180_24fps.bard \
    --out-c src/app/video/badapple_builtin_video.h \
    --symbol g_badapple_bard_builtin \
    --width 240 --height 180 --display-x 0 --display-y 70 \
    --fps 24 --max-seconds 5
```

If no video file is available yet, create a placeholder header:

```c
// Placeholder — replace with output of tools/encode_video_rect_delta.py
#pragma once
#include <stdint.h>
#include <stddef.h>

#define G_BADAPPLE_BARD_BUILTIN_SIZE 0u

const uint8_t g_badapple_bard_builtin[] = {};

const uint32_t g_badapple_bard_builtin_size = 0;
```

- [ ] **Step 2: Commit**

```bash
git add src/app/video/badapple_builtin_video.h assets/badapple_240x180_24fps.bard
git commit -m "feat: add generated BadApple BARD builtin video header (5s demo)"
```

---

### Task 7: Create badapple_video.h (player public API)

**Files:**
- Create: `src/app/video/badapple_video.h`

- [ ] **Step 1: Write the header**

```c
#pragma once

#include <stdint.h>
#include "st7789.h"

/* ── Video source ── */
typedef enum {
    badapple_video_source_builtin,
    badapple_video_source_raw_flash,
} BadappleVideoSource;

/* ── Public API ── */
uint8_t Badapple_Video_Init(St7789* lcd, BadappleVideoSource source);
void    Badapple_Video_Update(void);
void    Badapple_Video_Stop(void);
uint8_t Badapple_Video_Is_Active(void);

/* ── Development: install builtin to raw flash ── */
#if BADAPPLE_VIDEO_ENABLE_RAW_INSTALL
uint8_t Badapple_Video_Install_Builtin_To_Raw(void);
#endif
```

- [ ] **Step 2: Commit**

```bash
git add src/app/video/badapple_video.h
git commit -m "feat: add badapple_video.h public API header"
```

---

### Task 8: Create badapple_video.c (Part 1 — source abstraction, header parsing, init)

**Files:**
- Create: `src/app/video/badapple_video.c`

- [ ] **Step 1: Write includes, constants, and static state**

```c
#include "badapple_video.h"

#include <stddef.h>
#include <string.h>

#include "app_config.h"
#include "bsp_time.h"
#include "game_graphics.h"
#include "storage.h"

/* ── Constants ── */
#define SCREEN_WIDTH                 240
#define SCREEN_HEIGHT                320

#define BARD_MAGIC                   "BARD"
#define BARD_HEADER_SIZE             64u
#define BARD_INDEX_ENTRY_SIZE        8u
#define BARD_RECT_FORMAT_RECT5       1u
#define BARD_RECT5_SIZE              5u
#define BARD_FLAG_LOOP               (1u << 0)
#define BARD_FLAG_FIRST_FRAME_BLACK  (1u << 1)

#define BADAPPLE_COLOR_BLACK         0x0000u
#define BADAPPLE_COLOR_WHITE         0xFFFFu

#define BADAPPLE_RECT_CACHE_COUNT    16u
#define BADAPPLE_MAX_FRAMES_PER_UPDATE 2u
#define BADAPPLE_CATCHUP_MAX         12u

/* ── Static buffers ── */
static St7789* g_lcd = NULL;
static uint8_t g_header_buf[BARD_HEADER_SIZE];
static uint8_t g_index_buf[BARD_INDEX_ENTRY_SIZE];
static uint8_t g_rect_cache[BADAPPLE_RECT_CACHE_COUNT * BARD_RECT5_SIZE];

/* ── Parsed header fields ── */
static uint16_t g_encoded_w, g_encoded_h;
static uint16_t g_display_x, g_display_y;
static uint16_t g_fps_num, g_fps_den;
static uint32_t g_frame_count;
static uint32_t g_index_offset;
static uint32_t g_data_offset;
static uint32_t g_total_size;
static uint32_t g_flags;

/* ── Playback state ── */
static BadappleVideoSource g_source = badapple_video_source_builtin;
static uint32_t g_start_ms = 0;
static uint32_t g_current_frame = 0;
static uint8_t g_active = 0;
```

- [ ] **Step 2: Implement little-endian read helpers and source read abstraction**

```c
/* ── Little-endian read helpers ── */
static uint16_t rd_u16_le(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* ── Unified read from current source ── */
#if BADAPPLE_VIDEO_USE_BUILTIN
#include "badapple_builtin_video.h"
#endif

static uint8_t badapple_read(uint32_t offset, void* dst, uint32_t size) {
    if (dst == NULL || size == 0) { return 0; }

    if (g_source == badapple_video_source_builtin) {
#if BADAPPLE_VIDEO_USE_BUILTIN
        if (offset + size > g_badapple_bard_builtin_size) { return 0; }
        memcpy(dst, &g_badapple_bard_builtin[offset], size);
        return 1;
#else
        (void)offset;
        return 0;
#endif
    } else {
        /* raw flash */
#if BADAPPLE_VIDEO_USE_RAW_FLASH
        if (offset + size > g_total_size) { return 0; }
        if (offset + size > BADAPPLE_VIDEO_RAW_CAPACITY) { return 0; }
        return Storage_Raw_Read(BADAPPLE_VIDEO_RAW_ADDRESS + offset, dst, size);
#else
        (void)offset;
        return 0;
#endif
    }
}
```

- [ ] **Step 3: Implement header parsing and Init**

```c
/* ── Header parsing ── */
static uint8_t badapple_parse_header(void) {
    if (!badapple_read(0, g_header_buf, BARD_HEADER_SIZE)) { return 0; }

    /* Magic */
    if (g_header_buf[0] != 'B' || g_header_buf[1] != 'A' ||
        g_header_buf[2] != 'R' || g_header_buf[3] != 'D') {
        return 0;
    }

    uint8_t version     = g_header_buf[4];
    uint8_t rect_format = g_header_buf[5];
    uint16_t header_sz  = rd_u16_le(&g_header_buf[6]);

    if (version != 1 || rect_format != BARD_RECT_FORMAT_RECT5 || header_sz != BARD_HEADER_SIZE) {
        return 0;
    }

    g_encoded_w   = rd_u16_le(&g_header_buf[8]);
    g_encoded_h   = rd_u16_le(&g_header_buf[10]);
    /* screen_width  = rd_u16_le(&g_header_buf[12]); */
    /* screen_height = rd_u16_le(&g_header_buf[14]); */
    g_display_x   = rd_u16_le(&g_header_buf[16]);
    g_display_y   = rd_u16_le(&g_header_buf[18]);
    g_fps_num     = rd_u16_le(&g_header_buf[20]);
    g_fps_den     = rd_u16_le(&g_header_buf[22]);
    g_frame_count = rd_u32_le(&g_header_buf[24]);
    g_index_offset = rd_u32_le(&g_header_buf[28]);
    /* index_entry_size = rd_u32_le(&g_header_buf[32]); */
    g_data_offset = rd_u32_le(&g_header_buf[36]);
    g_total_size  = rd_u32_le(&g_header_buf[40]);
    g_flags       = rd_u32_le(&g_header_buf[48]);

    if (g_encoded_w == 0 || g_encoded_h == 0 || g_frame_count == 0 ||
        g_encoded_w > 256 || g_encoded_h > 256) {
        return 0;
    }

    if (g_fps_den == 0) { g_fps_den = 1; }
    if (g_fps_num == 0) { g_fps_num = 24; }

    return 1;
}

/* ── Public: Init ── */
uint8_t Badapple_Video_Init(St7789* lcd, BadappleVideoSource source) {
    if (lcd == NULL) { return 0; }

    g_lcd = lcd;
    g_source = source;
    g_current_frame = 0;
    g_active = 0;

    if (!badapple_parse_header()) { return 0; }

    /* Black out the full screen */
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BADAPPLE_COLOR_BLACK);

    g_start_ms = Bsp_Get_Time_Ms();
    g_active = 1;
    return 1;
}

void Badapple_Video_Stop(void) {
    g_active = 0;
}

uint8_t Badapple_Video_Is_Active(void) {
    return g_active;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/app/video/badapple_video.c
git commit -m "feat: add badapple_video.c - source abstraction, header parsing, init"
```

---

### Task 9: Create badapple_video.c (Part 2 — frame decoding and time-driven update)

**Files:**
- Modify: `src/app/video/badapple_video.c`

- [ ] **Step 1: Implement single-frame decode and draw**

Append to the file:

```c
/* ── Decode and draw one frame ── */
static void badapple_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color_bit) {
    int32_t sx = (int32_t)g_display_x + (int32_t)x;
    int32_t sy = (int32_t)g_display_y + (int32_t)y;

    /* Bounds check */
    if (sx < 0 || sy < 0 || (int32_t)(sx + w) > SCREEN_WIDTH ||
        (int32_t)(sy + h) > SCREEN_HEIGHT || w == 0 || h == 0) {
        g_active = 0;
        return;
    }

    uint16_t color = color_bit ? BADAPPLE_COLOR_WHITE : BADAPPLE_COLOR_BLACK;
    Game_Graphics_Fill_Rect(g_lcd, sx, sy, (int32_t)w, (int32_t)h, color);
}

static uint8_t badapple_decode_frame(uint32_t frame_idx) {
    if (frame_idx >= g_frame_count) { return 0; }

    /* 1. Read frame index entry */
    uint32_t idx_addr = g_index_offset + frame_idx * BARD_INDEX_ENTRY_SIZE;
    if (!badapple_read(idx_addr, g_index_buf, BARD_INDEX_ENTRY_SIZE)) { return 0; }

    uint32_t frame_offset = rd_u32_le(&g_index_buf[0]);
    uint32_t frame_size   = rd_u32_le(&g_index_buf[4]);

    if (frame_size < 2) { return 0; }  /* Need at least rect_count */

    /* 2. Read rect_count */
    uint8_t count_buf[2];
    if (!badapple_read(frame_offset, count_buf, 2)) { return 0; }
    uint16_t rect_count = rd_u16_le(count_buf);
    uint32_t payload_offset = frame_offset + 2u;

    if (frame_size != 2u + (uint32_t)rect_count * BARD_RECT5_SIZE) {
        /* Size mismatch — stop playback */
        g_active = 0;
        return 0;
    }

    /* 3. Read and draw rects in chunks */
    uint16_t rects_left = rect_count;
    while (rects_left > 0) {
        uint16_t chunk_count = rects_left;
        if (chunk_count > BADAPPLE_RECT_CACHE_COUNT) {
            chunk_count = BADAPPLE_RECT_CACHE_COUNT;
        }
        uint32_t chunk_bytes = (uint32_t)chunk_count * BARD_RECT5_SIZE;

        if (!badapple_read(payload_offset, g_rect_cache, chunk_bytes)) { return 0; }

        for (uint16_t i = 0; i < chunk_count; i++) {
            const uint8_t* p = &g_rect_cache[i * BARD_RECT5_SIZE];
            uint8_t rx = p[0];
            uint8_t ry = p[1];
            uint8_t rw = (uint8_t)(p[2] + 1u);
            uint8_t rh = (uint8_t)(p[3] + 1u);
            uint8_t color_bit = p[4] & 1u;
            badapple_draw_rect(rx, ry, rw, rh, color_bit);
            if (!g_active) { return 0; }  /* Bounds check failed */
        }

        payload_offset += chunk_bytes;
        rects_left -= chunk_count;
    }

    return 1;
}
```

- [ ] **Step 2: Implement time-driven Update**

```c
/* ── Public: Update ── */
void Badapple_Video_Update(void) {
    if (!g_active || g_lcd == NULL) { return; }

    uint32_t now = Bsp_Get_Time_Ms();
    uint32_t elapsed = now - g_start_ms;
    uint32_t target_frame = (uint32_t)(((uint64_t)elapsed * g_fps_num) /
                                       (1000u * (uint64_t)g_fps_den));

    /* Check if fallen too far behind */
    if (target_frame > g_current_frame + BADAPPLE_CATCHUP_MAX) {
        /* Reset — we cannot safely skip delta frames */
        Badapple_Video_Init(g_lcd, g_source);
        return;
    }

    uint8_t decoded_this_update = 0;
    while (g_active && g_current_frame <= target_frame && g_current_frame < g_frame_count) {
        if (!badapple_decode_frame(g_current_frame)) {
            g_active = 0;
            return;
        }
        g_current_frame++;
        decoded_this_update++;

        if (decoded_this_update >= BADAPPLE_MAX_FRAMES_PER_UPDATE) {
            break;
        }
    }

    /* End of video */
    if (g_active && g_current_frame >= g_frame_count) {
        if (g_flags & BARD_FLAG_LOOP) {
            /* Loop: re-init (clears screen, resets from frame 0) */
            Badapple_Video_Init(g_lcd, g_source);
        } else {
            g_active = 0;  /* Stop, keep last frame on screen */
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/app/video/badapple_video.c
git commit -m "feat: add frame decoding and time-driven update for badapple_video"
```

---

### Task 10: Create badapple_video.c (Part 3 — raw flash install)

**Files:**
- Modify: `src/app/video/badapple_video.c`

- [ ] **Step 1: Implement raw flash install function**

Append to the file:

```c
/* ── Development: Install builtin to raw flash ── */
#if BADAPPLE_VIDEO_ENABLE_RAW_INSTALL
uint8_t Badapple_Video_Install_Builtin_To_Raw(void) {
#if BADAPPLE_VIDEO_USE_BUILTIN
    if (!Storage_Is_Available()) { return 0; }

    uint32_t size = g_badapple_bard_builtin_size;
    if (size == 0 || size > BADAPPLE_VIDEO_RAW_CAPACITY) { return 0; }

    /* Erase — align to 4K sector */
    uint32_t erase_size = (size + 4095u) & ~4095u;
    if (!Storage_Raw_Erase(BADAPPLE_VIDEO_RAW_ADDRESS, erase_size)) { return 0; }

    /* Write in 256-byte pages */
    const uint8_t* src = g_badapple_bard_builtin;
    uint32_t offset = 0;
    while (offset < size) {
        uint32_t chunk = 256u;
        if (offset + chunk > size) { chunk = size - offset; }
        if (!Storage_Raw_Write(BADAPPLE_VIDEO_RAW_ADDRESS + offset, &src[offset], chunk)) {
            return 0;
        }
        offset += chunk;
    }

    /* Verify header magic and total_size */
    uint8_t verify_buf[BARD_HEADER_SIZE];
    if (!Storage_Raw_Read(BADAPPLE_VIDEO_RAW_ADDRESS, verify_buf, BARD_HEADER_SIZE)) {
        return 0;
    }
    if (verify_buf[0] != 'B' || verify_buf[1] != 'A' ||
        verify_buf[2] != 'R' || verify_buf[3] != 'D') {
        return 0;
    }

    return 1;
#else
    (void)0;
    return 0;
#endif
}
#endif /* BADAPPLE_VIDEO_ENABLE_RAW_INSTALL */
```

- [ ] **Step 2: Commit**

```bash
git add src/app/video/badapple_video.c
git commit -m "feat: add builtin-to-raw-flash install function"
```

---

### Task 11: Create screensaver_select menu (SCREENSAVER entry)

**Files:**
- Create: `src/app/games/screensaver_select/screensaver_select.h`
- Create: `src/app/games/screensaver_select/screensaver_select.c`

- [ ] **Step 1: Write screensaver_select.h**

```c
#pragma once

#include <stdint.h>
#include "game_runtime.h"

void         Screensaver_Select_Init(const Game_hardware* hardware);
Game_result  Screensaver_Select_Update(const Game_input* input);
uint32_t     Screensaver_Select_Get_Score(void);
uint8_t      Screensaver_Select_Is_Finished(void);
```

- [ ] **Step 2: Write screensaver_select.c**

```c
#include "screensaver_select.h"

#include <stddef.h>
#include <string.h>

#include "bsp_time.h"
#include "buzzer.h"
#include "game_graphics.h"
#include "screensaver.h"
#include "st7789.h"

#if BADAPPLE_VIDEO_USE_BUILTIN || BADAPPLE_VIDEO_USE_RAW_FLASH
#include "badapple_video.h"
#endif

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

#define COLOR_BLACK   0x0000u
#define COLOR_WHITE   0xFFFFu
#define COLOR_CYAN    0x07FFu
#define COLOR_GRAY    0x8410u
#define COLOR_DARK    0xA514u

typedef enum {
    screensaver_select_list,
    screensaver_select_running,
} ScreensaverSelectState;

typedef enum {
    screensaver_pipes,
    screensaver_bad_apple,
    screensaver_count,
} ScreensaverChoice;

static St7789* g_lcd = NULL;
static Buzzer* g_buzzer = NULL;
static Vib_motor_gpio* g_vib_motor = NULL;
static ScreensaverSelectState g_state = screensaver_select_list;
static ScreensaverChoice g_choice = screensaver_pipes;
static uint8_t g_finished = 0;

void Screensaver_Select_Init(const Game_hardware* hardware) {
    g_lcd = hardware->lcd;
    g_buzzer = hardware->buzzer;
    g_vib_motor = hardware->vib_motor;
    g_state = screensaver_select_list;
    g_choice = screensaver_pipes;
    g_finished = 0;

    /* Draw selection menu */
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_lcd, 58, 40, "SCREENSAVER", 2, COLOR_CYAN);
    Game_Graphics_Fill_Rect(g_lcd, 20, 70, SCREEN_WIDTH - 40, 1, COLOR_DARK);

    /* Pipes option */
    uint16_t pipes_color = (g_choice == screensaver_pipes) ? COLOR_WHITE : COLOR_GRAY;
    Game_Graphics_Draw_Text(g_lcd, 80, 100, "> PIPES", 2, pipes_color);

    /* Bad Apple option */
    uint16_t ba_color = (g_choice == screensaver_bad_apple) ? COLOR_WHITE : COLOR_GRAY;
    Game_Graphics_Draw_Text(g_lcd, 60, 140, "> BAD APPLE", 2, ba_color);

    Game_Graphics_Fill_Rect(g_lcd, 20, 200, SCREEN_WIDTH - 40, 1, COLOR_DARK);
    Game_Graphics_Draw_Text(g_lcd, 40, 220, "A: SELECT", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 140, 220, "B: BACK", 1, COLOR_WHITE);
    Game_Graphics_Draw_Text(g_lcd, 28, 245, "JOY UP/DOWN: CHANGE", 1, COLOR_CYAN);
}

static void start_pipes(void) {
    g_state = screensaver_select_running;
    Screensaver_Init(g_lcd);
}

#if BADAPPLE_VIDEO_USE_BUILTIN || BADAPPLE_VIDEO_USE_RAW_FLASH
static void start_bad_apple(void) {
    g_state = screensaver_select_running;

    BadappleVideoSource source;
#if BADAPPLE_VIDEO_USE_RAW_FLASH
    /* Try raw flash first */
    source = badapple_video_source_raw_flash;
    if (!Badapple_Video_Init(g_lcd, source)) {
        /* Fallback to builtin */
        source = badapple_video_source_builtin;
        Badapple_Video_Init(g_lcd, source);
    }
#else
    source = badapple_video_source_builtin;
    Badapple_Video_Init(g_lcd, source);
#endif
}
#else
static void start_bad_apple(void) {
    /* No video available — show message and go back */
    Game_Graphics_Fill_Rect(g_lcd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    Game_Graphics_Draw_Text(g_lcd, 30, 130, "NO BAD APPLE VIDEO", 2, COLOR_CYAN);
    /* Stay briefly then exit */
}
#endif

Game_result Screensaver_Select_Update(const Game_input* input) {
    if (g_state == screensaver_select_list) {
        if (input->direction == game_direction_up || input->direction == game_direction_down) {
            g_choice = (g_choice == screensaver_pipes) ? screensaver_bad_apple : screensaver_pipes;
            /* Redraw menu with updated selection */
            Screensaver_Select_Init(&(Game_hardware){
                .lcd = g_lcd, .buzzer = g_buzzer, .vib_motor = g_vib_motor});
            return game_result_running;
        }

        if (input->confirm_pressed) {
            if (g_choice == screensaver_pipes) {
                start_pipes();
            } else {
                start_bad_apple();
            }
            return game_result_running;
        }

        if (input->back_requested) {
            g_finished = 1;
            return game_result_exit;
        }

        return game_result_running;
    }

    /* ── Running state ── */
    /* Any input exits the screensaver and returns to the selector */
    if (input->direction_pressed || input->a_pressed || input->b_pressed ||
        input->x_pressed || input->y_pressed) {
#if BADAPPLE_VIDEO_USE_BUILTIN || BADAPPLE_VIDEO_USE_RAW_FLASH
        if (g_choice == screensaver_bad_apple) {
            Badapple_Video_Stop();
        }
#endif
        if (g_choice == screensaver_pipes) {
            Screensaver_Exit();
        }
        /* Return to selection list, don't exit the app */
        Screensaver_Select_Init(&(Game_hardware){
            .lcd = g_lcd, .buzzer = g_buzzer, .vib_motor = g_vib_motor});
        return game_result_running;
    }

    /* Run active screensaver */
    if (g_choice == screensaver_pipes) {
        Screensaver_Run_Frame();
    }
#if BADAPPLE_VIDEO_USE_BUILTIN || BADAPPLE_VIDEO_USE_RAW_FLASH
    else {
        Badapple_Video_Update();
    }
#endif

    return game_result_running;
}

uint32_t Screensaver_Select_Get_Score(void) { return 0; }

uint8_t Screensaver_Select_Is_Finished(void) { return g_finished; }
```

- [ ] **Step 3: Commit**

```bash
git add src/app/games/screensaver_select/
git commit -m "feat: add screensaver select menu (PIPES / BAD APPLE)"
```

---

### Task 12: Register SCREENSAVER in game_registry

**Files:**
- Modify: `src/app/game_console/game_registry.h`
- Modify: `src/app/game_console/game_registry.c`

- [ ] **Step 1: Add icon and ID to registry header**

In `game_registry.h`, append to both enums:

```c
// In Game_icon enum, append:
    game_icon_screensaver,

// In Game_id enum, append (before game_id_count):
    game_id_screensaver,
```

- [ ] **Step 2: Add include and register entry**

In `game_registry.c`:

Add include:
```c
#include "screensaver_select.h"
```

Append to `g_games[]` array (before the closing `};`):
```c
    {
        .name = "SCREENSAVER",
        .icon = game_icon_screensaver,
        .id = game_id_screensaver,
        .control_hint = NULL,
        .info_text = "DESCRIPTION\nChoose a screensaver mode.\nPIPES or BAD APPLE video.\n\n"
                     "CONTROLS\nJOY SELECT\nA OK  B BACK",
        .is_game = 0,
        .init = Screensaver_Select_Init,
        .update = Screensaver_Select_Update,
        .get_score = Screensaver_Select_Get_Score,
        .is_finished = Screensaver_Select_Is_Finished,
    },
```

- [ ] **Step 3: Add icon drawing to game_console.c**

In `game_console.c`, in `draw_grid_cell()`, add an else-if for the new icon. Use a simple TV/monitor icon:

```c
    } else if (game->icon == game_icon_screensaver) {
        /* Simple monitor icon */
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 16, icon_cy - 12, 32, 24, COLOR_DARK);
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 12, icon_cy - 8, 24, 16, COLOR_CYAN);
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 3, icon_cy + 11, 6, 3, COLOR_DARK);
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 7, icon_cy + 14, 14, 2, COLOR_DARK);
        /* Play triangle */
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 4, icon_cy - 5, 3, 10, COLOR_WHITE);
        Game_Graphics_Fill_Rect(g_lcd, icon_cx - 1, icon_cy - 3, 3, 6, COLOR_WHITE);
        Game_Graphics_Fill_Rect(g_lcd, icon_cx + 2, icon_cy - 1, 3, 2, COLOR_WHITE);
```

- [ ] **Step 4: Commit**

```bash
git add src/app/game_console/game_registry.h src/app/game_console/game_registry.c src/app/game_console/game_console.c
git commit -m "feat: register SCREENSAVER entry with PIPES and BAD APPLE modes"
```

---

### Task 13: Build and verify

**Files:** (all files created/modified above)

- [ ] **Step 1: Verify compilation**

```bash
# Build for VM target (faster iteration)
cd build/vm && cmake ../.. -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc)
```

Expected: Compiles without errors. All new `.c` files are auto-discovered by `GLOB_RECURSE`.

- [ ] **Step 2: Test encoding script independently**

```bash
python tools/encode_video_rect_delta.py \
    --input assets/badapple.mp4 \
    --out-bard /tmp/test.bard \
    --out-c /tmp/test_builtin.h \
    --symbol g_test_bard \
    --width 240 --height 180 --display-x 0 --display-y 70 \
    --fps 24 --max-seconds 5 \
    --preview-dir /tmp/badapple_preview
```

Expected: 
- Statistics printed showing frame count, sizes, rect counts
- "Verification: ALL FRAMES PASS"
- `/tmp/test.bard` created
- `/tmp/test_builtin.h` created  
- PNG preview frames in `/tmp/badapple_preview/`

- [ ] **Step 3: Run on hardware (if available)**

Flash the firmware, navigate to SCREENSAVER menu, test both PIPES and BAD APPLE modes.

Expected behavior:
- SCREENSAVER appears in game menu grid
- Selecting PIPES runs existing pipe animation
- Selecting BAD APPLE plays video from builtin (or raw flash if installed)
- Any input exits back to selection menu
- Existing 30s idle screensaver still works

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat: complete BadApple rect delta screensaver implementation"
```

---

### Task 14: Cleanup and documentation

**Files:**
- Modify: (optional) README or project docs

- [ ] **Step 1: Verify all flags work correctly**

Test combinations:
- `BADAPPLE_VIDEO_USE_BUILTIN=1`, `BADAPPLE_VIDEO_USE_RAW_FLASH=0` → builtin only
- `BADAPPLE_VIDEO_USE_BUILTIN=1`, `BADAPPLE_VIDEO_USE_RAW_FLASH=1` → raw flash with builtin fallback
- `BADAPPLE_VIDEO_USE_BUILTIN=0`, `BADAPPLE_VIDEO_USE_RAW_FLASH=0` → "NO BAD APPLE VIDEO" message

- [ ] **Step 2: Ensure no regressions**

- Main menu navigation works
- All existing games still launch and play
- 30s idle screensaver (PIPES) still activates automatically
- Any input exits idle screensaver and restores underlying screen

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "docs: add BadApple screensaver usage instructions"
```
