#!/usr/bin/env python3
"""
wav2buzzer.py — Convert WAV audio to buzzer C array (8-bit unsigned PCM).

Usage:
    python tools/wav2buzzer.py <input.wav> [sample_rate]

    sample_rate: Target sample rate in Hz (default: auto-select based on duration)

Auto sample rate selection (128 KB Flash, ~43 KB firmware):
    < 2s  → 16000 Hz (best quality)
    < 4s  → 10000 Hz
    < 8s  →  6000 Hz
    < 16s →  4000 Hz

Output: src/hal/buzzer/audio_ss.c
"""

import subprocess
import sys
import os
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
OUTPUT_C = os.path.join(PROJECT_ROOT, "src", "hal", "buzzer", "audio_ss.c")
OUTPUT_H = os.path.join(PROJECT_ROOT, "src", "hal", "buzzer", "audio_demo.h")

FLASH_SIZE = 128 * 1024       # 128 KB
FIRMWARE_SIZE = 50 * 1024     # ~50 KB estimate (with margin)
MAX_AUDIO_SIZE = FLASH_SIZE - FIRMWARE_SIZE  # ~78 KB


def get_duration(wav_path: str) -> float:
    """Get WAV duration in seconds using ffprobe."""
    result = subprocess.run(
        ["ffprobe", "-v", "quiet", "-print_format", "json", "-show_streams", wav_path],
        capture_output=True, text=True,
    )
    import json
    info = json.loads(result.stdout)
    return float(info["streams"][0]["duration"])


def auto_sample_rate(duration: float) -> int:
    """Pick the highest sample rate that fits in Flash."""
    for sr in [16000, 12000, 10000, 8000, 6000, 5000, 4000, 3000]:
        if int(duration * sr) <= MAX_AUDIO_SIZE:
            return sr
    return 3000


def convert_wav(wav_path: str, raw_path: str, sample_rate: int) -> int:
    """Convert WAV to 8-bit unsigned PCM using ffmpeg. Returns sample count."""
    subprocess.run(
        ["ffmpeg", "-y", "-i", wav_path, "-f", "u8", "-ar", str(sample_rate), "-ac", "1", raw_path],
        check=True, capture_output=True,
    )
    return os.path.getsize(raw_path)


def process_audio(data: bytes, sample_rate: int) -> bytes:
    """
    Process 8-bit unsigned PCM for buzzer playback.

    Buzzer-aware processing chain:
    1. DC offset removal
    2. Bandpass filter centered on buzzer resonant range (1-4 kHz)
    3. Dynamic range compression (make quiet parts louder)
    4. Soft-clip + normalize to maximize perceived loudness
    """
    import math

    samples = [float(b) for b in data]
    n = len(samples)

    # Step 1: Remove DC offset
    avg = sum(samples) / n
    centered = [s - avg for s in samples]

    # ── Step 2: Bandpass filter ─────────────────────────────────────
    # Passive buzzers resonate around 2-4 kHz. We boost that range
    # and attenuate frequencies below 500 Hz (which the buzzer can't
    # reproduce and just wastes energy / causes distortion).
    #
    # Two-pole Butterworth bandpass: center ~2500 Hz, Q = 1.5
    # Design via bilinear transform.

    def butterworth_bandpass(sig, sr, f_low, f_high, order=2):
        """Zero-phase (filtfilt-style) Butterworth bandpass via biquads."""
        import math as m

        def biquad_bp(sig, fc, bw):
            """Single biquad section — direct-form I."""
            w0 = 2.0 * m.pi * fc / sr
            alpha = m.sin(w0) * m.sinh(0.5 * bw * w0 / m.sin(w0))
            b0 = alpha
            b1 = 0.0
            b2 = -alpha
            a0 = 1.0 + alpha
            a1 = -2.0 * m.cos(w0)
            a2 = 1.0 - alpha
            # Normalize
            b0 /= a0; b1 /= a0; b2 /= a0
            a1 /= a0; a2 /= a0
            # Direct-form I
            out = [0.0] * len(sig)
            x1 = x2 = y1 = y2 = 0.0
            for i, x in enumerate(sig):
                y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
                out[i] = y
                x2 = x1; x1 = x
                y2 = y1; y1 = y
            return out

        # Two sections for 2nd-order Butterworth
        fc = m.sqrt(f_low * f_high)  # geometric center
        bw = (f_high - f_low) / fc   # normalised bandwidth
        tmp = biquad_bp(sig, fc, bw)
        return biquad_bp(tmp, fc, bw)  # zero-phase by double-pass = 4th order

    f_low = 800.0    # buzzer can't reproduce much below 800 Hz
    f_high = 4500.0  # buzzer resonance drops off above ~4.5 kHz
    filtered = butterworth_bandpass(centered, sample_rate, f_low, f_high)

    # Step 3: Dynamic range compression
    # For buzzer playback we want quiet parts louder and peaks tamed.
    # Simple RMS-based compressor:
    #   gain = min(threshold / rms_level, max_gain)
    #   compressed = input * gain
    #   soft-clip afterward.
    def compute_rms_env(sig, sr, window_ms=20):
        window = int(sr * window_ms / 1000)
        if window < 1:
            window = 1
        rms = [0.0] * len(sig)
        # Pad with first/last value
        for i in range(len(sig)):
            half = window // 2
            start = max(0, i - half)
            end = min(len(sig), i + half)
            seg = sig[start:end]
            r = math.sqrt(sum(v * v for v in seg) / len(seg))
            rms[i] = max(r, 1e-9)
        return rms

    rms_env = compute_rms_env(filtered, sample_rate, window_ms=30)

    # Compression parameters: boost quiet sections, cap loud peaks
    threshold = 0.08   # RMS below this gets boosted
    max_boost = 4.0    # max gain applied to quiet parts
    compression_ratio = 0.6  # mild compression on loud parts (>threshold)

    compressed = [0.0] * n
    for i in range(n):
        r = rms_env[i]
        if r < threshold:
            gain = min(threshold / r, max_boost)
        else:
            # Gentle slope: make loud parts a bit quieter
            excess = r / threshold
            gain = 1.0 / (excess ** (1.0 - compression_ratio))
        compressed[i] = filtered[i] * gain

    # Step 4: Normalize + soft-clip
    peak = max(abs(min(compressed)), abs(max(compressed)), 1e-9)

    # Gentle soft-clip (arctan) — maximises perceived loudness
    # Scale so that 99% of samples are < 1.0, then arctan clamps extremes
    scale = 0.9 / peak
    clipped = [math.atan(v * scale * 3.0) / math.atan(3.0) for v in compressed]

    # Final normalise to full 8-bit range
    final_peak = max(abs(min(clipped)), abs(max(clipped)), 1e-9)
    final_scale = 120.0 / final_peak

    processed = []
    for s in clipped:
        val = int(128 + s * final_scale)
        processed.append(max(0, min(255, val)))

    return bytes(processed)


def generate_c_array(raw_path: str, output_c: str, sample_rate: int, sample_count: int, wav_name: str):
    """Generate C source file from raw PCM data with audio processing."""
    with open(raw_path, "rb") as f:
        data = f.read()

    # Process audio for buzzer
    processed = process_audio(data, sample_rate)

    lines = [
        '#include <stdint.h>',
        '',
        f'/* {wav_name}: {sample_rate} Hz, 8-bit unsigned PCM, mono, DC-removed + normalized */',
        f'const uint32_t audio_ss_len = {len(processed)};',
        f'const uint8_t audio_ss_data[] = {{',
    ]

    for i in range(0, len(processed), 16):
        chunk = processed[i:i + 16]
        line = ', '.join(str(b) for b in chunk)
        lines.append(f'    {line},')

    lines.append('};')
    lines.append('')

    with open(output_c, 'w') as f:
        f.write('\n'.join(lines))

    # Generate header with sample rate macro
    header_lines = [
        '#pragma once',
        '',
        '#include <stdint.h>',
        '',
        f'/* Auto-generated by wav2buzzer.py — do not edit manually */',
        f'#define AUDIO_SS_SAMPLE_RATE  {sample_rate}',
        f'#define AUDIO_SS_LEN          {len(processed)}',
        '',
        'extern const uint32_t audio_ss_len;',
        'extern const uint8_t audio_ss_data[];',
        '',
    ]
    with open(OUTPUT_H, 'w') as f:
        f.write('\n'.join(header_lines))

    return len(processed)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    wav_path = sys.argv[1]
    if not os.path.isfile(wav_path):
        print(f"Error: File not found: {wav_path}")
        sys.exit(1)

    # Get duration
    duration = get_duration(wav_path)
    print(f"Input:    {os.path.basename(wav_path)} ({duration:.1f}s)")

    # Determine sample rate
    if len(sys.argv) >= 3:
        sample_rate = int(sys.argv[2])
    else:
        sample_rate = auto_sample_rate(duration)
    print(f"Sample rate: {sample_rate} Hz")

    estimated_size = int(duration * sample_rate)
    print(f"Est. size:   {estimated_size} bytes ({estimated_size / 1024:.1f} KB)")

    if estimated_size > MAX_AUDIO_SIZE:
        print(f"WARNING: May exceed Flash capacity ({MAX_AUDIO_SIZE / 1024:.0f} KB available)")
        print(f"         Try a lower sample rate: {auto_sample_rate(duration)} Hz")

    # Convert
    with tempfile.NamedTemporaryFile(suffix=".raw", delete=False) as tmp:
        raw_path = tmp.name

    try:
        sample_count = convert_wav(wav_path, raw_path, sample_rate)
        actual_size = generate_c_array(raw_path, OUTPUT_C, sample_rate, sample_count,
                                       os.path.basename(wav_path))
        print(f"Output:  {OUTPUT_C}")
        print(f"Actual:  {actual_size} bytes ({actual_size / 1024:.1f} KB)")
        print(f"Total Flash: ~{(FIRMWARE_SIZE + actual_size) / 1024:.0f} KB / {FLASH_SIZE / 1024:.0f} KB")
        print(f"Done! Rebuild and flash.")
    finally:
        os.unlink(raw_path)


if __name__ == "__main__":
    main()
