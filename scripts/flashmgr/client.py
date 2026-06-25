"""
Flash Manager Client — PC-side protocol implementation for Flash UART passthrough.

Manages the MPU's LittleFS filesystem on W25Q32 external Flash over a
serial UART link.  Supports chunked upload/download with CRC verification,
automatic retry on timeout/CRC errors, and directory listing.

Usage:
    from flashmgr.client import FlashManager, get_serial_ports, print_serial_ports

    fm = FlashManager("/dev/ttyUSB0")
    fm.upload_file("firmware.bin", "/fw.bin")
    fm.list_dir("/")
    fm.download_file("/fw.bin", "fw_backup.bin")
    fm.close()
"""

import os
import struct
import tempfile
import time
import binascii
from typing import Callable, List, Optional, Tuple

try:
    import serial
except ImportError:
    serial = None

IMAGE_MAGIC = b"R565"
IMAGE_VERSION = 1
IMAGE_FLAG_MASK = 0x01
IMAGE_HEADER_SIZE = 16


def get_serial_ports() -> List[Tuple[str, str, str]]:
    """Return available serial ports as (device, description, hardware id)."""
    if serial is None:
        raise RuntimeError("缺少 pyserial，请先执行：pip install pyserial")

    from serial.tools import list_ports

    return [
        (port.device, port.description, port.hwid)
        for port in list_ports.comports()
    ]


def print_serial_ports(file=None) -> None:
    """Print serial ports in a human-readable form."""
    import sys

    output = file or sys.stdout
    ports = get_serial_ports()
    if not ports:
        print("未检测到串口设备。", file=output)
        return

    print("当前检测到的串口：", file=output)
    for device, description, hardware_id in ports:
        print(f"  {device:<8} {description}", file=output)
        print(f"           {hardware_id}", file=output)


def pack_image_asset(
    source_path: str,
    output_path: str,
    width: int,
    height: int,
    fit: str = "cover",
    with_mask: bool = False,
) -> None:
    """Convert a JPG/PNG into the MCU's streaming RGB565 image format."""
    try:
        from PIL import Image, ImageOps
    except ImportError as exc:
        raise RuntimeError("Pillow is required: pip install pillow") from exc

    # Pillow >= 9.1 uses Image.Resampling.LANCZOS; older versions use Image.LANCZOS
    try:
        _LANCZOS = Image.Resampling.LANCZOS
    except AttributeError:
        _LANCZOS = Image.LANCZOS  # type: ignore[attr-defined]

    if width <= 0 or height <= 0 or width > 0xFFFF or height > 0xFFFF:
        raise ValueError("Image dimensions must be in the range 1..65535")

    image = ImageOps.exif_transpose(Image.open(source_path)).convert("RGBA")
    target_size = (width, height)
    if fit == "cover":
        image = ImageOps.fit(image, target_size, method=_LANCZOS)
    elif fit == "contain":
        contained = ImageOps.contain(image, target_size, method=_LANCZOS)
        image = Image.new("RGBA", target_size, (0, 0, 0, 0))
        image.alpha_composite(
            contained, ((width - contained.width) // 2, (height - contained.height) // 2)
        )
    elif fit == "stretch":
        image = image.resize(target_size, _LANCZOS)
    else:
        raise ValueError(f"Unknown fit mode: {fit}")

    pixel_data = bytearray(width * height * 2)
    mask_stride = (width + 7) // 8
    mask_data = bytearray(mask_stride * height) if with_mask else bytearray()
    pixels = image.load()

    output_index = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if with_mask:
                blend = alpha / 255.0
                red = round(red * blend)
                green = round(green * blend)
                blue = round(blue * blend)
                if alpha >= 40:
                    mask_data[y * mask_stride + x // 8] |= 1 << (x & 7)
            else:
                red = (red * alpha) // 255
                green = (green * alpha) // 255
                blue = (blue * alpha) // 255

            rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            pixel_data[output_index] = rgb565 & 0xFF
            pixel_data[output_index + 1] = rgb565 >> 8
            output_index += 2

    flags = IMAGE_FLAG_MASK if with_mask else 0
    header = struct.pack(
        "<4sBBHHHI",
        IMAGE_MAGIC,
        IMAGE_VERSION,
        flags,
        width,
        height,
        binascii.crc_hqx(pixel_data, 0xFFFF),
        len(pixel_data),
    )
    if len(header) != IMAGE_HEADER_SIZE:
        raise AssertionError("Unexpected image header size")

    with open(output_path, "wb") as output:
        output.write(header)
        output.write(pixel_data)
        output.write(mask_data)


# ── Protocol constants (must match flash_mgr.h) ──────────────────────

SYNC0 = 0xAA
SYNC1 = 0x55

# Host → Device commands
CMD_READ   = 0x01
CMD_WRITE  = 0x02
CMD_DELETE = 0x03
CMD_LIST   = 0x04
CMD_INFO   = 0x05
CMD_FORMAT = 0x06
CMD_RESET  = 0x07

# Device → Host responses
RESP_ACK       = 0x80
RESP_NAK       = 0x81
RESP_CRC_ERR   = 0x82
RESP_BUSY      = 0x83
RESP_CHUNK     = 0x84
RESP_EOF       = 0x85
RESP_LIST_ITEM = 0x86
RESP_LIST_END  = 0x87
RESP_INFO_RESP = 0x88

# NAK error codes
ERR_UNKNOWN = 0x00
ERR_NOENT   = 0x01
ERR_NOSPC   = 0x02
ERR_INVAL   = 0x03
ERR_EXIST   = 0x04
ERR_IO      = 0x05
ERR_CORRUPT = 0x06

# File type codes
TYPE_UNKNOWN = 0x00
TYPE_REG     = 0x01
TYPE_DIR     = 0x02

_ERROR_NAMES = {
    ERR_UNKNOWN: "unknown error",
    ERR_NOENT:   "file not found",
    ERR_NOSPC:   "no space left on device",
    ERR_INVAL:   "invalid argument",
    ERR_EXIST:   "file already exists",
    ERR_IO:      "I/O error",
    ERR_CORRUPT: "filesystem corruption",
}

_TYPE_NAMES = {
    TYPE_UNKNOWN: "?",
    TYPE_REG:     "file",
    TYPE_DIR:     "dir",
}

# ── CRC16 (matches MCU Soft_Crc16_Calc, poly=0x1021, init=0xFFFF) ──

_CRC16_TABLE = None


def _make_crc16_table() -> List[int]:
    """Build CRC-16/CCITT-FALSE lookup table, bit-reversed like the C code."""
    poly_rev = 0x8408  # bit_reverse_u16(0x1021)
    table = []
    for i in range(256):
        crc = 0
        c = i
        for _ in range(8):
            if (crc ^ c) & 1:
                crc = (crc >> 1) ^ poly_rev
            else:
                crc >>= 1
            c >>= 1
        table.append(crc)
    return table


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE — byte-exact match of Soft_Crc16_Calc."""
    global _CRC16_TABLE
    if _CRC16_TABLE is None:
        _CRC16_TABLE = _make_crc16_table()

    crc = 0xFFFF
    for byte in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ byte) & 0xFF]
    return crc


# ── Frame helpers ────────────────────────────────────────────────────


def build_frame(cmd: int, seq: int, data: bytes = b"") -> bytes:
    """Build a protocol_binary_frame.

    Wire layout:  SYNC0 SYNC1 LEN_H LEN_L [PAYLOAD] CRCH CRCL
    Payload:      CMD SEQ_H SEQ_L [DATA...]

    CRC covers LEN(2) + PAYLOAD = 2 + 3 + len(data) bytes.
    This matches protocol_binary.c:bin_send_pack.
    """
    payload_len = 3 + len(data)          # CMD(1) + SEQ(2) + data
    if payload_len > 515:
        raise ValueError(f"Payload too long: {payload_len} > 515")

    buf = bytearray(4 + payload_len + 2)  # SYNC(2) + LEN(2) + payload + CRC(2)
    buf[0] = SYNC0
    buf[1] = SYNC1
    buf[2] = (payload_len >> 8) & 0xFF   # LEN_H
    buf[3] = payload_len & 0xFF           # LEN_L
    buf[4] = cmd
    buf[5] = (seq >> 8) & 0xFF
    buf[6] = seq & 0xFF
    if data:
        buf[7 : 7 + len(data)] = data

    # CRC over LEN(2) + PAYLOAD = bytes at buf[2 .. 7+len(data))
    crc_span = memoryview(buf)[2 : 7 + len(data)]
    crc = crc16(bytes(crc_span))
    buf[7 + len(data)] = (crc >> 8) & 0xFF
    buf[8 + len(data)] = crc & 0xFF

    return bytes(buf)


# ── FlashManager ─────────────────────────────────────────────────────


class FlashManager:
    """Serial Flash file manager for the MPU LittleFS partition.

    Parameters
    ----------
    port : str
        Serial device path, e.g. ``/dev/ttyUSB0`` or ``COM3``.
    baudrate : int
        Baud rate (default 115200).
    timeout : float
        Per-byte read timeout in seconds (default 0.5).
    max_retries : int
        Max retransmissions per chunk (default 3).
    """

    CHUNK_SIZE = 512
    PATH_MAX   = 255

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        timeout: float = 5.0,
        max_retries: int = 3,
    ):
        if serial is None:
            raise RuntimeError("pyserial is required for UART access: pip install pyserial")
        self._timeout = timeout
        self._max_retries = max_retries
        self._seq = 0
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
        )
        time.sleep(0.05)  # let DTR/RTS settle

    # ── Public API ───────────────────────────────────────────────

    def close(self):
        """Close the serial port."""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def upload_file(
        self,
        local_path: str,
        remote_path: str,
        progress_cb: Optional[Callable[[int, int], None]] = None,
    ) -> bool:
        """Upload a local file to the device.

        The file is split into 512-byte chunks.  Each chunk is sent in a
        self-contained WRITE frame (open → seek → write → close on the MCU
        side), so transmission can be safely resumed from any chunk boundary.

        Parameters
        ----------
        local_path : str
            Path to the local file to read.
        remote_path : str
            Destination path on the device (must start with ``/``).
        progress_cb : callable(int sent, int total) or None
            Called after each successful chunk write.

        Returns
        -------
        bool
            True if the entire file was uploaded successfully.
        """
        if not os.path.isfile(local_path):
            raise FileNotFoundError(local_path)

        file_size = os.path.getsize(local_path)
        offset = 0
        path_bytes = remote_path.encode("utf-8")
        if len(path_bytes) > self.PATH_MAX:
            raise ValueError(f"Remote path too long: {len(path_bytes)} > {self.PATH_MAX}")

        # Max data bytes in a frame: 515 (max payload) - 3 (CMD+SEQ) = 512.
        # WRITE payload overhead: 1 (path_len) + len(path) + 4 (offset).
        max_chunk = 512 - 1 - len(path_bytes) - 4
        if max_chunk <= 0:
            raise ValueError(f"Remote path too long for any data: {len(path_bytes)}")

        with open(local_path, "rb") as f:
            while offset < file_size:
                chunk = f.read(max_chunk)

                # Build WRITE payload:
                #   [path_len:1B][path:N][offset:4B LE][data:M]
                payload = (
                    bytes([len(path_bytes)])
                    + path_bytes
                    + struct.pack("<I", offset)
                    + chunk
                )

                resp = self._transact(CMD_WRITE, payload, {RESP_ACK, RESP_NAK})
                if resp is None:
                    return False

                resp_cmd, _, resp_data = resp
                if resp_cmd == RESP_NAK:
                    err = resp_data[0] if resp_data else ERR_UNKNOWN
                    print(f"WRITE NAK at offset {offset}: {_ERROR_NAMES.get(err, err)}")
                    if err == ERR_CORRUPT:
                        print(
                            "文件系统已损坏。重新烧录最新上传固件后执行："
                            f"python scripts/flash_manager.py {self.ser.port} format --yes"
                        )
                    return False

                offset += len(chunk)
                if progress_cb:
                    progress_cb(offset, file_size)

            # Send a zero-length WRITE at file_size to truncate.
            # This ensures the file has the correct size — essential when
            # overwriting a larger file with smaller data, and for
            # recovering from partial/interrupted writes.
            trunc_payload = (
                bytes([len(path_bytes)])
                + path_bytes
                + struct.pack("<I", file_size)
                # no data → truncate at offset on the device side
            )
            resp = self._transact(CMD_WRITE, trunc_payload, {RESP_ACK, RESP_NAK})
            if resp is None:
                return False
            resp_cmd, _, resp_data = resp
            if resp_cmd == RESP_NAK:
                err = resp_data[0] if resp_data else ERR_UNKNOWN
                print(f"WRITE truncate NAK: {_ERROR_NAMES.get(err, err)}")
                return False

        return True

    def upload_image(
        self,
        local_path: str,
        remote_path: str,
        width: int,
        height: int,
        fit: str = "cover",
        with_mask: bool = False,
        progress_cb: Optional[Callable[[int, int], None]] = None,
    ) -> bool:
        """Convert and upload an image as a streamable RGB565 asset."""
        with tempfile.NamedTemporaryFile(suffix=".r565", delete=False) as temp:
            temp_path = temp.name
        try:
            pack_image_asset(
                local_path,
                temp_path,
                width=width,
                height=height,
                fit=fit,
                with_mask=with_mask,
            )
            return self.upload_file(temp_path, remote_path, progress_cb=progress_cb)
        finally:
            try:
                os.remove(temp_path)
            except FileNotFoundError:
                pass

    def download_file(
        self,
        remote_path: str,
        local_path: str,
        progress_cb: Optional[Callable[[int, Optional[int]], None]] = None,
    ) -> bool:
        """Download a file from the device.

        The file is read in 512-byte chunks using READ requests.
        Each READ is idempotent (carries an absolute offset), so a lost
        chunk is recovered by re-requesting the same offset.

        Parameters
        ----------
        remote_path : str
            Source path on the device.
        local_path : str
            Destination path on the PC.
        progress_cb : callable(int received, int total_or_None) or None
            Called after each chunk.  ``total`` is None until EOF.

        Returns
        -------
        bool
            True on success.
        """
        path_bytes = remote_path.encode("utf-8")
        if len(path_bytes) > self.PATH_MAX:
            raise ValueError(f"Remote path too long: {len(path_bytes)} > {self.PATH_MAX}")

        offset = 0

        with open(local_path, "wb") as f:
            while True:
                # Build READ payload:
                #   [path_len:1B][path:N][offset:4B LE]
                payload = (
                    bytes([len(path_bytes)])
                    + path_bytes
                    + struct.pack("<I", offset)
                )

                resp = self._transact(
                    CMD_READ, payload, {RESP_CHUNK, RESP_EOF, RESP_NAK}
                )
                if resp is None:
                    return False

                resp_cmd, _, resp_data = resp

                if resp_cmd == RESP_NAK:
                    err = resp_data[0] if resp_data else ERR_UNKNOWN
                    print(f"READ NAK at offset {offset}: {_ERROR_NAMES.get(err, err)}")
                    return False

                if resp_cmd == RESP_EOF:
                    file_size = struct.unpack("<I", resp_data[:4])[0]
                    if progress_cb:
                        progress_cb(file_size, file_size)
                    return True

                if resp_cmd == RESP_CHUNK:
                    # CHUNK payload: [offset:4B LE][data:M]
                    if len(resp_data) < 4:
                        print("CHUNK response too short")
                        return False
                    chunk_offset = struct.unpack("<I", resp_data[:4])[0]
                    chunk_data = resp_data[4:]
                    f.seek(chunk_offset)
                    f.write(chunk_data)
                    offset = chunk_offset + len(chunk_data)
                    if progress_cb:
                        progress_cb(offset, None)

        return True  # unreachable, but keeps type checkers happy

    def delete(self, remote_path: str) -> bool:
        """Delete a file or empty directory on the device.

        Returns True on success, False if the file doesn't exist or
        another error occurs.
        """
        path_bytes = remote_path.encode("utf-8")
        if len(path_bytes) > self.PATH_MAX:
            raise ValueError(f"Remote path too long: {len(path_bytes)} > {self.PATH_MAX}")

        payload = bytes([len(path_bytes)]) + path_bytes
        resp = self._transact(CMD_DELETE, payload, {RESP_ACK, RESP_NAK})
        if resp is None:
            return False
        resp_cmd, _, resp_data = resp
        if resp_cmd == RESP_NAK:
            err = resp_data[0] if resp_data else ERR_UNKNOWN
            print(f"DELETE NAK: {_ERROR_NAMES.get(err, err)}")
            return False
        return True

    def list_dir(self, remote_path: str = "/") -> List[dict]:
        """List the contents of a directory.

        Returns a list of dicts::

            {"name": str, "type": "file"|"dir"|"?", "size": int}

        Raises OSError on communication failure.
        """
        path_bytes = remote_path.encode("utf-8")
        if len(path_bytes) > self.PATH_MAX:
            raise ValueError(f"Remote path too long: {len(path_bytes)} > {self.PATH_MAX}")

        payload = bytes([len(path_bytes)]) + path_bytes
        entries: List[dict] = []

        # LIST uses a unique sequence number for the request; all responses
        # share that seq.  We send once, then collect until LIST_END.
        seq = self._next_seq()
        frame = build_frame(CMD_LIST, seq, payload)

        for retry in range(self._max_retries + 1):
            self.ser.write(frame)
            self.ser.flush()

            while True:
                parsed = self._recv_frame()
                if parsed is None:
                    # timeout — abort this retry
                    break

                resp_cmd, resp_seq, resp_data = parsed
                if resp_seq != seq:
                    continue  # stale

                if resp_cmd == "crc_error" or resp_cmd == RESP_CRC_ERR:
                    break  # retry the whole LIST

                if resp_cmd == RESP_LIST_ITEM:
                    if len(resp_data) < 2:
                        continue
                    ftype = resp_data[0]
                    name_len = resp_data[1]
                    name = resp_data[2 : 2 + name_len].decode("utf-8", errors="replace")
                    size_bytes = resp_data[2 + name_len : 2 + name_len + 4]
                    size = struct.unpack("<I", size_bytes)[0] if len(size_bytes) == 4 else 0
                    entries.append({
                        "name": name,
                        "type": _TYPE_NAMES.get(ftype, "?"),
                        "size": size,
                    })

                elif resp_cmd == RESP_LIST_END:
                    # success — count is in resp_data[0:2] (LE), but we
                    # already have the entries list
                    return entries

                elif resp_cmd == RESP_NAK:
                    err = resp_data[0] if resp_data else ERR_UNKNOWN
                    raise OSError(f"LIST failed: {_ERROR_NAMES.get(err, err)}")

        raise OSError("LIST: no response from device")

    def get_info(self, remote_path: str) -> Optional[dict]:
        """Get file/directory info.

        Returns ``{"type": "file"|"dir"|"?", "size": int}`` or None on error.
        """
        path_bytes = remote_path.encode("utf-8")
        if len(path_bytes) > self.PATH_MAX:
            raise ValueError(f"Remote path too long: {len(path_bytes)} > {self.PATH_MAX}")

        payload = bytes([len(path_bytes)]) + path_bytes
        resp = self._transact(CMD_INFO, payload, {RESP_INFO_RESP, RESP_NAK})
        if resp is None:
            return None
        resp_cmd, _, resp_data = resp
        if resp_cmd == RESP_NAK:
            return None
        if len(resp_data) < 5:
            return None
        ftype = resp_data[0]
        size = struct.unpack("<I", resp_data[1:5])[0]
        return {"type": _TYPE_NAMES.get(ftype, "?"), "size": size}

    def format(self) -> bool:
        """Format the LittleFS partition. **Destroys all data.**"""
        resp = self._transact(CMD_FORMAT, b"", {RESP_ACK, RESP_NAK})
        if resp is None:
            return False
        resp_cmd, _, resp_data = resp
        if resp_cmd == RESP_NAK:
            err = resp_data[0] if resp_data else ERR_UNKNOWN
            print(f"FORMAT NAK: {_ERROR_NAMES.get(err, err)}")
            return False
        return True

    def reset(self) -> bool:
        """Send a soft-reset request to the device."""
        resp = self._transact(CMD_RESET, b"", {RESP_ACK})
        return resp is not None

    # ── Protocol internals ───────────────────────────────────────

    def _next_seq(self) -> int:
        seq = self._seq
        self._seq = (self._seq + 1) & 0xFFFF
        return seq

    def _recv_frame(self) -> Optional[Tuple[int, int, bytes]]:
        """Read and parse one frame from the serial port.

        Wire format: SYNC0 SYNC1 LEN_H LEN_L [CMD SEQ_H SEQ_L DATA] CRCH CRCL

        Returns
        -------
        (cmd, seq, data) on success.
        ("crc_error", seq, None) if frame has bad CRC.
        None on timeout or framing error.
        """
        ser = self.ser

        # ── Hunt for SYNC0 SYNC1 ────────────────────────────────
        while True:
            b = ser.read(1)
            if not b:
                return None
            if b[0] == SYNC0:
                b2 = ser.read(1)
                if b2 and b2[0] == SYNC1:
                    break
                if b2 and b2[0] == SYNC0:
                    continue  # 0xAA 0xAA 0x55 is valid

        # ── Read LEN (2 bytes, big-endian) ──────────────────────
        len_bytes = ser.read(2)
        if len(len_bytes) < 2:
            return None
        payload_len = (len_bytes[0] << 8) | len_bytes[1]

        if payload_len < 3 or payload_len > 520:
            return None

        # ── Read payload: CMD(1) + SEQ(2) + data(N-3) ──────────
        payload = ser.read(payload_len)
        if len(payload) < payload_len:
            return None

        cmd = payload[0]
        seq = (payload[1] << 8) | payload[2]
        data = payload[3:] if payload_len > 3 else b""

        # ── Read CRC ────────────────────────────────────────────
        crc_bytes = ser.read(2)
        if len(crc_bytes) < 2:
            return None

        expected_crc = (crc_bytes[0] << 8) | crc_bytes[1]
        # CRC covers LEN(2) + payload = len_bytes + payload
        computed_crc = crc16(len_bytes + payload)

        if computed_crc != expected_crc:
            return ("crc_error", seq, None)

        return (cmd, seq, data)

    def _transact(
        self,
        cmd: int,
        data: bytes,
        accept_responses: set,
    ) -> Optional[Tuple[int, int, bytes]]:
        """Send a frame and wait for an accepted response.

        Retries on timeout or CRC_ERR up to ``max_retries`` times.
        NAK is returned to the caller (it is always "accepted" as a
        terminal response).

        Returns ``(cmd, seq, data)`` or ``None`` if all retries are
        exhausted.
        """
        for retry in range(self._max_retries + 1):
            seq = self._next_seq()
            frame = build_frame(cmd, seq, data)
            self.ser.write(frame)
            self.ser.flush()

            deadline = time.monotonic() + self._timeout * 3
            while time.monotonic() < deadline:
                parsed = self._recv_frame()
                if parsed is None:
                    # timeout — break out of recv loop, go to retry
                    break

                resp_cmd, resp_seq, resp_data = parsed

                if resp_cmd == "crc_error":
                    # MCU sent a frame with bad CRC — ignore, keep reading
                    continue

                if resp_cmd == RESP_CRC_ERR:
                    # MCU says our frame had bad CRC — retry
                    break

                if resp_seq != seq:
                    # Stale response from a previous transaction
                    continue

                if resp_cmd in accept_responses:
                    return (resp_cmd, resp_seq, resp_data)

                # NAK is always accepted
                if resp_cmd == RESP_NAK:
                    return (resp_cmd, resp_seq, resp_data)

                if resp_cmd == RESP_BUSY:
                    # Device busy — wait a bit, then retry
                    time.sleep(0.1)
                    break

                # Unexpected response — ignore
                continue

            if retry < self._max_retries:
                time.sleep(0.05)

        return None
