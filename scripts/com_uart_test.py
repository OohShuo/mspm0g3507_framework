#!/usr/bin/env python3
"""
Simple com_uart chat: send a string on a fixed interval, print whatever
comes back. Mirrors the C-side test (src/app/com_uart_test/com_uart_test.c)
which sends the same payload list on rotation every second and echoes RX
in its on_rx callback.

Wire format: raw bytes — no [len] header, no CRC. The board's idle timer
demarcates frames on the RX side, so we just need a short gap between
sends for the board to deliver each one as its own callback.
"""

import argparse
import os
import sys
import time

import serial


# Mirrors src/app/com_uart_test/com_uart_test.c's payloads[].
# Each is an ASCII string the C side sends in rotation.
PAYLOADS = [
    "Hello, world!",
    "The quick brown fox jumps over the lazy dog.",
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
    "1234567890",
    "!@#$^&*()_+-=[]{}|;':\",./<>?",
    "Short",
]


def drain(ser: serial.Serial) -> None:
    """Print any bytes that have arrived since the last read.

    RX comes in as raw chunks — they may split mid-frame, so we just
    print whatever pyserial hands back and let the user eyeball
    alignment. With the 115200 baud and 1 s send cadence in the C test,
    you should see one or two echo lines per outbound send.
    """
    while ser.in_waiting:
        chunk = ser.read(ser.in_waiting)
        try:
            text = chunk.decode("utf-8", errors="replace")
        except Exception:
            text = repr(chunk)
        ts = time.strftime("%H:%M:%S")
        print(f"[{ts}] RX {len(chunk):>3} B: {text}", flush=True)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", default=os.environ.get("COM_UART_PORT", "/dev/ttyUSB0"))
    p.add_argument("--baud", type=int, default=2000000)
    p.add_argument("--interval", type=float, default=1.0,
        help="seconds between sends (default: 1.0, like the C test; 0 to send as fast as possible)")
    p.add_argument("--no-rx", action="store_true",
        help="skip reading responses (one-way only)")
    args = p.parse_args()

    try:
        ser = serial.Serial(
            port=args.port, baudrate=args.baud,
            bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE, timeout=0.0)
    except serial.SerialException as e:
        print(f"open {args.port} failed: {e}", file=sys.stderr)
        return 1

    print(f"opened {args.port} @ {args.baud} 8N1 — Ctrl+C to stop", flush=True)
    if args.no_rx:
        print("RX suppressed (--no-rx)", flush=True)
    print(flush=True)

    i = 0
    try:
        while True:
            payload = PAYLOADS[i % len(PAYLOADS)].encode("ascii")
            ser.write(payload)
            ts = time.strftime("%H:%M:%S")
            print(f"[{ts}] [{i:06d}] TX {len(payload):>3} B: {payload.decode('ascii')!r}", flush=True)
            i += 1

            if not args.no_rx:
                # Let the bytes come back. The C side's on_rx fires
                # ~idle_timeout_ms after the last byte, so sleeping
                # that long plus a small margin catches the echo.
                time.sleep(0.05)
                drain(ser)

            if args.interval > 0:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nstopped", flush=True)
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
