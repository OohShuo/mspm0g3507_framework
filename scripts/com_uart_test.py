#!/usr/bin/env python3
"""
PC-side peer / regression suite for the com_uart_test app.

Wire format (matches hal/com_uart):
    [len (1)] [data (len)] [crc16_lo (1)] [crc16_hi (1)]
CRC16: modbus, poly 0x1021, init 0xFFFF, reflected (table-based).

Test plan (per run):
  - single-frame variable length: 1, 8, 13, 15, 16, 17, 32, 33, 64, 100, 128, 200, 252, 253
  - edge payloads: all 0x00, all 0xFF, alternating, random
  - deliberately bad CRC: must be silently dropped (no echo)
  - back-to-back burst: 10 small frames in one idle event
  - DMA-buffer-boundary sizes (16, 32, 48) to exercise the bsp's re-arm path

For each good frame, the script writes the frame, waits for the echo,
and verifies the bytes match exactly. Per-test latency is reported.
"""

import argparse
import os
import random
import sys
import threading
import time

import serial


# ===== CRC16 modbus (poly 0x1021, init 0xFFFF, reflected) =====
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


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ b) & 0xFF]
    return crc


def make_frame(payload: bytes, corrupt_crc: bool = False) -> bytes:
    if len(payload) > 253:
        raise ValueError(f"payload too long ({len(payload)} > 253)")
    crc = crc16_modbus(bytes([len(payload)]) + payload)
    if corrupt_crc:
        crc ^= 0xA5A5  # flip every other bit — guaranteed mismatch
    return bytes([len(payload)]) + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def parse_frame_at(buf: bytearray, start: int):
    """Try to parse a single frame at buf[start]. Returns:
       ('ok',    payload, total) on success
       ('bad',   None,    None)  on a CRC / length error (caller resyncs)
       ('need',  None,    None)  on incomplete frame (need more bytes)"""
    if start >= len(buf):
        return ("need", None, None)
    payload_len = buf[start]
    total = 1 + payload_len + 2
    if len(buf) - start < total:
        return ("need", None, None)
    frame = bytes(buf[start:start + total])
    payload = frame[1:1 + payload_len]
    crc_rx = frame[-2] | (frame[-1] << 8)
    crc_chk = crc16_modbus(frame[:-2])
    if crc_rx != crc_chk:
        return ("bad", None, None)
    return ("ok", payload, total)


class EchoWaiter:
    """Reader thread that parses frames from the serial port and surfaces
    them as (payload,) tuples via a thread-safe queue-like API."""

    def __init__(self, ser: serial.Serial):
        self.ser = ser
        self.stop = threading.Event()
        self.cv = threading.Condition()
        self.buf = bytearray()
        self.echoes = []  # list of payload bytes
        self.t = threading.Thread(target=self._reader, daemon=True)
        self.t.start()

    def _reader(self):
        while not self.stop.is_set():
            try:
                chunk = self.ser.read(64)
            except (serial.SerialException, OSError):
                break
            if not chunk:
                continue
            with self.cv:
                self.buf.extend(chunk)
                pos = 0
                while pos < len(self.buf):
                    res = parse_frame_at(self.buf, pos)
                    if res[0] == "ok":
                        _, payload, total = res
                        self.echoes.append(bytes(payload))
                        pos += total
                    elif res[0] == "bad":
                        pos += 1
                    else:  # 'need'
                        break
                if pos > 0:
                    del self.buf[:pos]
                self.cv.notify_all()

    def wait_for_echo(self, expected: bytes, timeout: float = 2.0):
        with self.cv:
            deadline = time.time() + timeout
            while time.time() < deadline:
                for i, payload in enumerate(self.echoes):
                    if payload == expected:
                        del self.echoes[: i + 1]
                        return True
                remaining = deadline - time.time()
                if remaining <= 0:
                    break
                self.cv.wait(timeout=remaining)
            return False

    def drain(self):
        with self.cv:
            self.echoes.clear()
            self.buf.clear()

    def close(self):
        self.stop.set()
        self.t.join(timeout=1.0)


# ===== Test cases =====
def make_test_cases() -> list:
    cases = []
    # Variable length, including the DMA-buf size (16) and other boundaries
    for n in [1, 8, 13, 15, 16, 17, 32, 33, 64, 100, 128, 200, 252, 253]:
        cases.append((f"{n:>3} B payload", bytes(range(n))))
    # Edge patterns
    cases.append(("16 B all 0x00",      b"\x00" * 16))
    cases.append(("16 B all 0xFF",      b"\xff" * 16))
    cases.append(("64 B alternating",  bytes(((i & 1) * 0xFF) for i in range(64))))
    cases.append(("200 B random",      random.randbytes(200)))
    cases.append(("253 B random (max)", random.randbytes(253)))
    return cases


def run_one(waiter: EchoWaiter, name: str, payload: bytes, expect_echo: bool = True) -> tuple:
    """Returns (ok, latency_ms)."""
    frame = make_frame(payload)
    t0 = time.time()
    waiter.ser.write(frame)

    if not expect_echo:
        time.sleep(0.05)
        return (True, 0.0)

    ok = waiter.wait_for_echo(payload, timeout=1.5)
    dt = (time.time() - t0) * 1000.0
    return (ok, dt)


def main() -> int:
    parser = argparse.ArgumentParser(description="com_uart_test PC peer / regression suite")
    parser.add_argument("--port", default=os.environ.get("COM_UART_PORT", "/dev/ttyUSB0"),
        help="serial port (default: /dev/ttyUSB0 or $COM_UART_PORT)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seed", type=int, default=None, help="random seed (default: time-based)")
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
    else:
        random.seed(int(time.time()))

    try:
        ser = serial.Serial(
            port=args.port, baudrate=args.baud,
            bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE, timeout=0.2)
    except serial.SerialException as e:
        print(f"open {args.port} @ {args.baud} failed: {e}", file=sys.stderr)
        return 1

    print(f"opened {args.port} @ {args.baud} 8N1\n", flush=True)
    waiter = EchoWaiter(ser)
    # Let any stale bytes (e.g. boot-time 'From board X.XXX s' echo) drain.
    time.sleep(0.3)
    waiter.drain()

    cases = make_test_cases()
    pass_count = 0
    fail_count = 0
    latencies = []

    print("[1] single-frame variable length + edge payloads")
    for name, payload in cases:
        ok, dt = run_one(waiter, name, payload)
        latencies.append(dt)
        status = "PASS" if ok else "FAIL"
        print(f"    {status}  {name:<28s}  {dt:6.1f} ms", flush=True)
        if ok:
            pass_count += 1
        else:
            fail_count += 1

    print("\n[2] deliberately bad CRC - must NOT produce an echo")
    bad_payload = b"this payload will be CRC-corrupted on the wire"
    frame = make_frame(bad_payload, corrupt_crc=True)
    t0 = time.time()
    waiter.ser.write(frame)
    bad_echo_seen = waiter.wait_for_echo(bad_payload, timeout=0.4)
    dt = (time.time() - t0) * 1000.0
    if bad_echo_seen:
        print(f"    FAIL  bad-CRC frame echoed back ({dt:.0f} ms) - CRC check is broken")
        fail_count += 1
    else:
        print(f"    PASS  bad-CRC frame silently dropped ({dt:.0f} ms)")
        pass_count += 1
    waiter.drain()

    print("\n[3] back-to-back burst - 10 small frames in one idle event")
    burst_payloads = [f"burst{i:02d}".encode() for i in range(10)]
    burst_frames = b"".join(make_frame(p) for p in burst_payloads)
    waiter.ser.write(burst_frames)

    t0 = time.time()
    burst_ok = True
    for p in burst_payloads:
        if not waiter.wait_for_echo(p, timeout=2.0):
            burst_ok = False
            break
    burst_dt = (time.time() - t0) * 1000.0
    if burst_ok:
        print(f"    PASS  all 10 frames received ({burst_dt:.0f} ms total)")
        pass_count += 1
    else:
        print(f"    FAIL  burst missing some frames ({burst_dt:.0f} ms)")
        fail_count += 1
    waiter.drain()

    print("\n[4] DMA-buffer-boundary sizes (16, 32, 48) - exercise the re-arm path")
    for n in [16, 32, 48]:
        payload = bytes(range(n))
        ok, dt = run_one(waiter, f"{n} B (DMA buf = 16)", payload)
        status = "PASS" if ok else "FAIL"
        print(f"    {status}  {n} B at DMA boundary     {dt:6.1f} ms", flush=True)
        if ok:
            pass_count += 1
        else:
            fail_count += 1

    if latencies:
        latencies_sorted = sorted(latencies)
        p50 = latencies_sorted[len(latencies_sorted) // 2]
        p95 = latencies_sorted[int(len(latencies_sorted) * 0.95)]
        print(f"\n=== Summary: {pass_count} passed, {fail_count} failed ===")
        print(f"=== Round-trip latency (single-frame tests): "
            f"min={min(latencies):.1f}ms  p50={p50:.1f}ms  p95={p95:.1f}ms  "
            f"max={max(latencies):.1f}ms ===")

    waiter.close()
    ser.close()
    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
