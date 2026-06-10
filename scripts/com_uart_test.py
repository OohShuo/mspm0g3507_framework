#!/usr/bin/env python3
"""
Simple com_uart sender. Loops through the C-side payload list and writes
each one as a frame. No reader, no verification — just send.

Wire format (matches hal/com_uart):
    [len (1)] [data (len)] [crc16_lo (1)] [crc16_hi (1)]
CRC16: poly 0x1021, init 0xFFFF, reflected (matches hal/soft_crc).
"""

import argparse
import os
import sys
import time

import serial


# ===== CRC16 (matches hal/soft_crc Soft_Crc16_Calc) =====
def _build_crc16_table():
    t = []
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0x8408  # reflected 0x1021
            else:
                crc >>= 1
        t.append(crc)
    return t


_CRC16_TABLE = _build_crc16_table()


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ b) & 0xFF]
    return crc


def make_frame(payload: bytes) -> bytes:
    crc = crc16(bytes([len(payload)]) + payload)
    return bytes([len(payload)]) + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


# Mirrors src/app/com_uart_test/com_uart_test.c's payloads[]
PAYLOADS = [
    b"a"*253,
    b"b"*253,
    b"c"*253,
    b"d"*253,
    b"!"*253,
]


def main() -> int:
    parser = argparse.ArgumentParser(description="Simple com_uart payload sender")
    parser.add_argument("--port", default=os.environ.get("COM_UART_PORT", "/dev/ttyUSB0"))
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--interval", type=float, default=1,
        help="seconds between sends (default: 1, send every second; 0 to send as fast as possible)")
    args = parser.parse_args()

    try:
        ser = serial.Serial(
            port=args.port, baudrate=args.baud,
            bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE, timeout=0.2)
    except serial.SerialException as e:
        print(f"open {args.port} failed: {e}", file=sys.stderr)
        return 1

    print(f"opened {args.port} @ {args.baud} 8N1", flush=True)
    print(f"sending {len(PAYLOADS)} payloads in rotation, Ctrl+C to stop\n", flush=True)

    i = 0
    try:
        while True:
            payload = PAYLOADS[i % len(PAYLOADS)]
            frame = make_frame(payload)
            ser.write(frame)
            ts = time.strftime("%H:%M:%S")
            text = payload.decode("utf-8", errors="replace")
            print(f"[{ts}] [{i:06d}] TX {len(payload):>3} B: {text}", flush=True)
            i += 1
            if args.interval > 0:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nstopped", flush=True)
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
