#!/usr/bin/env python3
"""Pack the Low Knight PICO-8 cartridge resources for external Flash.

The MCU should not keep the full .p8 text cartridge in internal Flash.  This
tool extracts the fixed-size PICO-8 resource sections and writes a compact
binary file that can be uploaded to LittleFS as /low_knight.p8r.
"""

from __future__ import annotations

import argparse
import binascii
import pathlib
import struct


MAGIC = b"LKPR"
VERSION = 1
HEADER_FMT = "<4sBBHIIII"
HEADER_SIZE = struct.calcsize(HEADER_FMT)


def read_sections(path: pathlib.Path) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if raw_line.startswith("__") and raw_line.endswith("__"):
            current = raw_line.strip("_")
            sections[current] = []
            continue
        if current is not None:
            sections[current].append(raw_line.rstrip())
    return sections


def hex_lines_to_bytes(lines: list[str], expected_lines: int, expected_width: int, name: str) -> bytes:
    if len(lines) != expected_lines:
        raise ValueError(f"{name}: expected {expected_lines} lines, got {len(lines)}")

    out = bytearray()
    for row, line in enumerate(lines):
        if len(line) != expected_width:
            raise ValueError(f"{name}: line {row} expected {expected_width} chars, got {len(line)}")
        try:
            out.extend(bytes.fromhex(line))
        except ValueError as exc:
            raise ValueError(f"{name}: line {row} contains non-hex data") from exc
    return bytes(out)


def pack_cartridge(source: pathlib.Path, output: pathlib.Path) -> None:
    sections = read_sections(source)
    missing = [name for name in ("gfx", "gff", "map") if name not in sections]
    if missing:
        raise ValueError(f"missing PICO-8 section(s): {', '.join(missing)}")

    gfx = hex_lines_to_bytes(sections["gfx"], 128, 128, "gfx")
    gff = hex_lines_to_bytes(sections["gff"], 2, 256, "gff")
    game_map = hex_lines_to_bytes(sections["map"], 32, 256, "map")
    payload = gfx + gff + game_map
    crc = binascii.crc_hqx(payload, 0xFFFF)
    header = struct.pack(HEADER_FMT, MAGIC, VERSION, 0, crc, len(gfx), len(gff), len(game_map), 0)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + payload)

    print(f"wrote {output}")
    print(f"  header: {HEADER_SIZE} bytes")
    print(f"  gfx:    {len(gfx)} bytes")
    print(f"  gff:    {len(gff)} bytes")
    print(f"  map:    {len(game_map)} bytes")
    print(f"  crc16:  0x{crc:04x}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack low.p8 resources into a compact MCU asset")
    parser.add_argument("source", nargs="?", default="low.p8", help="PICO-8 .p8 cartridge")
    parser.add_argument("output", nargs="?", default="build/low_knight.p8r", help="Output resource file")
    args = parser.parse_args()

    pack_cartridge(pathlib.Path(args.source), pathlib.Path(args.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
