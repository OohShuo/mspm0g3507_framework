#!/usr/bin/env python3
"""
PC-side peer for the com_uart_test app.

Wire format (matches hal/com_uart):
    [len (1)] [data (len)] [crc16_lo (1)] [crc16_hi (1)]
CRC16: modbus, poly 0x1021, init 0xFFFF, reflected (table-based).

Usage:
    python3 com_uart_test.py [--port /dev/ttyUSB0] [--baud 115200]
"""

import argparse
import sys
import threading
import time

import serial

# ===== CRC16 modbus (poly 0x1021, init 0xFFFF, reflected) =====
def _crc16_modbus_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001  # reflected 0x1021
            else:
                crc >>= 1
        table.append(crc)
    return table

_CRC16_TABLE = _crc16_modbus_table()

def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ b) & 0xFF]
    return crc

def make_frame(payload: bytes) -> bytes:
    """Wrap payload bytes as a com_uart frame."""
    if len(payload) > 253:
        raise ValueError(f"payload too long ({len(payload)} > 253)")
    crc = crc16_modbus(bytes([len(payload)]) + payload)
    return bytes([len(payload)]) + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])

def try_parse_frame(buf: bytearray):
    """If buf holds a complete + CRC-valid frame, return (payload, consumed).
    Otherwise return None. consumed is the number of bytes to drop from buf
    (either the full frame, or everything up to and including a bad header)."""
    if len(buf) < 1:
        return None
    payload_len = buf[0]
    total = 1 + payload_len + 2
    if len(buf) < total:
        return None
    frame = bytes(buf[:total])
    payload = frame[1:1 + payload_len]
    crc_rx = frame[-2] | (frame[-1] << 8)
    crc_chk = crc16_modbus(frame[:-2])
    if crc_rx != crc_chk:
        # Bad frame — drop one byte and let the caller resync. This trades
        # latency for robustness against the PC receiving mid-frame bytes.
        return ("bad", 1)
    return ("ok", payload, total)


def reader_thread(ser: serial.Serial, stop: threading.Event) -> None:
    """Background reader: read bytes, slice into frames, print payloads."""
    buf = bytearray()
    while not stop.is_set():
        try:
            chunk = ser.read(64)
        except serial.SerialException as e:
            print(f"[reader] serial error: {e}", flush=True)
            break
        if not chunk:
            continue
        buf.extend(chunk)
        while True:
            res = try_parse_frame(buf)
            if res is None:
                break
            if res[0] == "bad":
                # Drop the bad leading byte and try again
                del buf[:1]
                continue
            _, payload, consumed = res
            del buf[:consumed]
            try:
                text = payload.decode("utf-8", errors="replace")
            except Exception:
                text = payload.hex()
            ts = time.strftime("%H:%M:%S")
            print(f"[{ts}] RX ({len(payload):>3} B): {text}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="com_uart_test PC peer")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="baud rate (default: 115200)")
    args = parser.parse_args()

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.5,
        )
    except serial.SerialException as e:
        print(f"failed to open {args.port}: {e}", file=sys.stderr)
        return 1

    print(f"opened {args.port} @ {args.baud} 8N1", flush=True)
    print("reader running; sending 'from pc <ts>' every 1s. Ctrl+C to exit.", flush=True)

    stop = threading.Event()
    t = threading.Thread(target=reader_thread, args=(ser, stop), daemon=True)
    t.start()

    last_send = 0.0
    try:
        while not stop.is_set():
            now = time.time()
            if now - last_send >= 1.0:
                last_send = now
                ts = time.strftime("%H:%M:%S")
                payload = f"from pc {ts}".encode("utf-8")
                try:
                    ser.write(make_frame(payload))
                except serial.SerialException as e:
                    print(f"[writer] serial error: {e}", flush=True)
                    break
                print(f"[{ts}] TX ({len(payload):>3} B): {payload.decode('utf-8')}", flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\nexiting", flush=True)
    finally:
        stop.set()
        t.join(timeout=1.0)
        if ser.is_open:
            ser.close()
        print("closed.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
