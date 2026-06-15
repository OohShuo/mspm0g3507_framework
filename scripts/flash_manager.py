#!/usr/bin/env python3
"""
FlashManager — PC-side client for the Flash UART passthrough protocol.

Manages the MPU's LittleFS filesystem on W25Q32 external Flash over a
serial UART link.  Supports chunked upload/download with CRC verification,
automatic retry on timeout/CRC errors, and directory listing.

Usage:
    from flash_manager import FlashManager

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
from typing import Callable, List, Optional, Tuple, Union

try:
    import serial
except ImportError:
    serial = None

IMAGE_MAGIC = b"R565"
IMAGE_VERSION = 1
IMAGE_FLAG_MASK = 0x01
IMAGE_HEADER_SIZE = 16
VIDEO_MAGIC = b"V565"
VIDEO_VERSION = 1
VIDEO_HEADER_SIZE = 24
VIDEO_FLAGS_NONE = 0
VIDEO_FLAG_RLE = 0x01
VIDEO_FLAG_INDEX8 = 0x02
VIDEO_FLAG_INDEX1 = 0x04
VIDEO_FRAME_EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".gif")


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

    if width <= 0 or height <= 0 or width > 0xFFFF or height > 0xFFFF:
        raise ValueError("Image dimensions must be in the range 1..65535")

    image = ImageOps.exif_transpose(Image.open(source_path)).convert("RGBA")
    target_size = (width, height)
    if fit == "cover":
        image = ImageOps.fit(image, target_size, method=Image.Resampling.LANCZOS)
    elif fit == "contain":
        contained = ImageOps.contain(image, target_size, method=Image.Resampling.LANCZOS)
        image = Image.new("RGBA", target_size, (0, 0, 0, 0))
        image.alpha_composite(
            contained, ((width - contained.width) // 2, (height - contained.height) // 2)
        )
    elif fit == "stretch":
        image = image.resize(target_size, Image.Resampling.LANCZOS)
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


def _fit_rgba_image(image, width: int, height: int, fit: str):
    from PIL import Image, ImageOps

    target_size = (width, height)
    if fit == "cover":
        return ImageOps.fit(image, target_size, method=Image.Resampling.LANCZOS)
    if fit == "contain":
        contained = ImageOps.contain(image, target_size, method=Image.Resampling.LANCZOS)
        output = Image.new("RGBA", target_size, (0, 0, 0, 255))
        output.alpha_composite(
            contained, ((width - contained.width) // 2, (height - contained.height) // 2)
        )
        return output
    if fit == "stretch":
        return image.resize(target_size, Image.Resampling.LANCZOS)
    raise ValueError(f"Unknown fit mode: {fit}")


def _rgba_to_rgb565_bytes(image) -> bytes:
    width, height = image.size
    pixel_data = bytearray(width * height * 2)
    pixels = image.load()
    output_index = 0
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            red = (red * alpha) // 255
            green = (green * alpha) // 255
            blue = (blue * alpha) // 255
            rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            pixel_data[output_index] = rgb565 & 0xFF
            pixel_data[output_index + 1] = rgb565 >> 8
            output_index += 2
    return bytes(pixel_data)


def _rgba_to_rgb_image(image):
    from PIL import Image

    output = Image.new("RGBA", image.size, (0, 0, 0, 255))
    output.alpha_composite(image.convert("RGBA"))
    return output.convert("RGB")


def _palette_rgb565_bytes(palette_image) -> bytes:
    palette = (palette_image.getpalette() or [])[: 256 * 3]
    palette += [0] * (256 * 3 - len(palette))
    output = bytearray(512)
    for i in range(256):
        red, green, blue = palette[i * 3 : i * 3 + 3]
        rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        output[i * 2] = rgb565 & 0xFF
        output[i * 2 + 1] = rgb565 >> 8
    return bytes(output)


def _mono1_palette_rgb565_bytes() -> bytes:
    return struct.pack("<HH", 0x0000, 0xFFFF)


def _rgba_to_mono1_bytes(image, threshold: int = 128) -> bytes:
    rgb_image = _rgba_to_rgb_image(image)
    width, height = rgb_image.size
    pixels = rgb_image.load()
    stride = (width + 7) // 8
    output = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            red, green, blue = pixels[x, y]
            luma = (red * 30 + green * 59 + blue * 11) // 100
            if luma >= threshold:
                output[y * stride + x // 8] |= 1 << (7 - (x & 7))
    return bytes(output)


def _encode_rle_row(row_data: bytes, unit_size: int = 2) -> bytes:
    pixel_count = len(row_data) // unit_size
    output = bytearray()
    pixel = 0

    while pixel < pixel_count:
        run_count = 1
        while (
            pixel + run_count < pixel_count
            and run_count < 128
            and row_data[(pixel + run_count) * unit_size : (pixel + run_count + 1) * unit_size]
            == row_data[pixel * unit_size : (pixel + 1) * unit_size]
        ):
            run_count += 1

        if run_count >= 3:
            output.append(0x80 | (run_count - 1))
            output.extend(row_data[pixel * unit_size : (pixel + 1) * unit_size])
            pixel += run_count
            continue

        literal_start = pixel
        pixel += run_count
        while pixel < pixel_count and pixel - literal_start < 128:
            lookahead_run = 1
            while (
                pixel + lookahead_run < pixel_count
                and lookahead_run < 128
                and row_data[
                    (pixel + lookahead_run) * unit_size :
                    (pixel + lookahead_run + 1) * unit_size
                ]
                == row_data[pixel * unit_size : (pixel + 1) * unit_size]
            ):
                lookahead_run += 1
            if lookahead_run >= 3:
                break
            pixel += lookahead_run

        literal_count = pixel - literal_start
        output.append(literal_count - 1)
        output.extend(row_data[literal_start * unit_size : pixel * unit_size])

    return bytes(output)


def _encode_rle_frame(pixel_data: bytes, width: int, height: int, unit_size: int = 2) -> bytes:
    row_size = width * unit_size
    output = bytearray()
    for y in range(height):
        encoded = _encode_rle_row(pixel_data[y * row_size : (y + 1) * row_size], unit_size)
        if len(encoded) > 0xFFFF:
            raise ValueError("Compressed row is too large for V565 RLE")
        output.extend(struct.pack("<H", len(encoded)))
        output.extend(encoded)
    return bytes(output)


def _iter_video_frames(source_path: str):
    try:
        from PIL import Image, ImageOps, ImageSequence
    except ImportError as exc:
        raise RuntimeError("Pillow is required: pip install pillow") from exc

    if os.path.isdir(source_path):
        names = sorted(
            name for name in os.listdir(source_path)
            if name.lower().endswith(VIDEO_FRAME_EXTS)
        )
        for name in names:
            path = os.path.join(source_path, name)
            with Image.open(path) as image:
                yield ImageOps.exif_transpose(image).convert("RGBA")
        return

    try:
        with Image.open(source_path) as image:
            if getattr(image, "is_animated", False):
                for frame in ImageSequence.Iterator(image):
                    yield frame.convert("RGBA")
            else:
                yield ImageOps.exif_transpose(image).convert("RGBA")
        return
    except Exception:
        pass

    try:
        import imageio.v3 as iio
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError(
            "Video files need imageio and imageio-ffmpeg: "
            "pip install imageio imageio-ffmpeg"
        ) from exc

    for frame in iio.imiter(source_path):
        yield Image.fromarray(frame).convert("RGBA")


def pack_video_asset(
    source_path: str,
    output_path: str,
    width: int,
    height: int,
    fps: int,
    fit: str = "cover",
    max_frames: Optional[int] = None,
    compression: str = "mono1-rle",
    start_frame: int = 0,
    end_frame: Optional[int] = None,
) -> int:
    """Convert a video, animated image, or frame directory into V565 frames."""
    if width <= 0 or height <= 0 or width > 0xFFFF or height > 0xFFFF:
        raise ValueError("Video dimensions must be in the range 1..65535")
    if fps <= 0 or fps > 0xFFFF:
        raise ValueError("FPS must be in the range 1..65535")
    if max_frames is not None and max_frames <= 0:
        raise ValueError("--max-frames must be positive")
    if start_frame < 0:
        raise ValueError("--start-frame must be >= 0")
    if end_frame is not None and end_frame < start_frame:
        raise ValueError("--end-frame must be >= --start-frame")
    if compression not in ("none", "rle", "index8", "index8-rle", "mono1", "mono1-rle"):
        raise ValueError(
            "compression must be one of: none, rle, index8, index8-rle, mono1, mono1-rle"
        )

    frame_size = width * height * 2
    frame_count = 0
    frame_offsets: List[int] = []
    uses_index8 = compression in ("index8", "index8-rle")
    uses_index1 = compression in ("mono1", "mono1-rle")
    uses_rle = compression in ("rle", "index8-rle", "mono1-rle")
    palette_bytes = b""
    with tempfile.NamedTemporaryFile(suffix=".v565data", delete=False) as temp:
        temp_path = temp.name

    try:
        data_size = 0
        frames = []
        for source_frame_index, frame in enumerate(_iter_video_frames(source_path)):
            if source_frame_index < start_frame:
                continue
            if end_frame is not None and source_frame_index > end_frame:
                break
            if max_frames is not None and len(frames) >= max_frames:
                break
            frames.append(_fit_rgba_image(frame.convert("RGBA"), width, height, fit))

        frame_count = len(frames)
        if frame_count == 0:
            raise ValueError("No frames were found in the video source")

        if uses_index8:
            from PIL import Image

            palette_source = Image.new("RGB", (width, height * frame_count))
            for i, frame in enumerate(frames):
                palette_source.paste(_rgba_to_rgb_image(frame), (0, i * height))
            palette_image = palette_source.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
            palette_bytes = _palette_rgb565_bytes(palette_image)
        elif uses_index1:
            palette_bytes = _mono1_palette_rgb565_bytes()

        with open(temp_path, "wb") as data_output:
            for frame in frames:
                if uses_index8:
                    encoded_input = _rgba_to_rgb_image(frame).quantize(
                        palette=palette_image,
                        dither=Image.Dither.NONE,
                    ).tobytes()
                    unit_size = 1
                elif uses_index1:
                    encoded_input = _rgba_to_mono1_bytes(frame)
                    unit_size = 1
                    encode_width = (width + 7) // 8
                else:
                    encoded_input = _rgba_to_rgb565_bytes(frame)
                    unit_size = 2
                    encode_width = width
                if not uses_index1:
                    encode_width = width

                if uses_rle:
                    frame_offsets.append(data_size)
                    encoded_frame = _encode_rle_frame(encoded_input, encode_width, height, unit_size)
                    data_output.write(encoded_frame)
                    data_size += len(encoded_frame)
                else:
                    data_output.write(encoded_input)
                    data_size += len(encoded_input)

        if data_size > 0xFFFFFFFF:
            raise ValueError("Packed video is too large for the V565 header")

        flags = VIDEO_FLAGS_NONE
        if uses_rle:
            flags |= VIDEO_FLAG_RLE
        if uses_index8:
            flags |= VIDEO_FLAG_INDEX8
        if uses_index1:
            flags |= VIDEO_FLAG_INDEX1

        header = struct.pack(
            "<4sBBHHHIII",
            VIDEO_MAGIC,
            VIDEO_VERSION,
            flags,
            width,
            height,
            fps,
            frame_count,
            frame_size,
            data_size,
        )
        if len(header) != VIDEO_HEADER_SIZE:
            raise AssertionError("Unexpected video header size")

        with open(output_path, "wb") as output, open(temp_path, "rb") as data_input:
            output.write(header)
            if uses_index8 or uses_index1:
                output.write(palette_bytes)
            if uses_rle:
                for frame_offset in frame_offsets:
                    output.write(struct.pack("<I", frame_offset))
            while True:
                chunk = data_input.read(64 * 1024)
                if not chunk:
                    break
                output.write(chunk)
    finally:
        try:
            os.remove(temp_path)
        except FileNotFoundError:
            pass

    return frame_count


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
        Baud rate (default 921600).
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
        baudrate: int = 921600,
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

        # Replace semantics: free old file blocks before streaming a new file.
        # LittleFS is copy-on-write, so overwriting a large old /demo.v565 without
        # truncating first can temporarily need old+new space in the 2 MiB partition.
        prepare_payload = (
            bytes([len(path_bytes)])
            + path_bytes
            + struct.pack("<I", 0)
        )
        resp = self._transact(CMD_WRITE, prepare_payload, {RESP_ACK, RESP_NAK})
        if resp is None:
            print(f"WRITE prepare timeout for {remote_path}")
            return False
        resp_cmd, _, resp_data = resp
        if resp_cmd == RESP_NAK:
            err = resp_data[0] if resp_data else ERR_UNKNOWN
            print(f"WRITE prepare NAK: {_ERROR_NAMES.get(err, err)}")
            return False

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
                    print(
                        f"WRITE timeout at offset {offset}; "
                        "device did not acknowledge this chunk"
                    )
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
                print(f"WRITE truncate timeout at final size {file_size}")
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

    def upload_video(
        self,
        local_path: str,
        remote_path: str,
        width: int,
        height: int,
        fps: int,
        fit: str = "cover",
        max_frames: Optional[int] = None,
        compression: str = "mono1-rle",
        start_frame: int = 0,
        end_frame: Optional[int] = None,
        progress_cb: Optional[Callable[[int, int], None]] = None,
    ) -> bool:
        """Convert and upload a V565 RGB565 video asset."""
        with tempfile.NamedTemporaryFile(suffix=".v565", delete=False) as temp:
            temp_path = temp.name
        try:
            frame_count = pack_video_asset(
                local_path,
                temp_path,
                width=width,
                height=height,
                fps=fps,
                fit=fit,
                max_frames=max_frames,
                compression=compression,
                start_frame=start_frame,
                end_frame=end_frame,
            )
            print(f"Packed {frame_count} frames, {os.path.getsize(temp_path)} bytes")
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


# ── CLI ──────────────────────────────────────────────────────────────

def _main() -> int:
    """Minimal CLI for quick testing."""
    import argparse
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "pack-video":
        pack_parser = argparse.ArgumentParser(description="Convert video/GIF/frames to a V565 file")
        pack_parser.add_argument("local", help="Local video/GIF path or frame directory")
        pack_parser.add_argument("output", help="Output .v565 file")
        pack_parser.add_argument("--width", type=int, required=True)
        pack_parser.add_argument("--height", type=int, required=True)
        pack_parser.add_argument("--fps", type=int, default=8)
        pack_parser.add_argument(
            "--fit", choices=("cover", "contain", "stretch"), default="cover"
        )
        pack_parser.add_argument("--max-frames", type=int, default=None)
        pack_parser.add_argument("--start-frame", type=int, default=0)
        pack_parser.add_argument("--end-frame", type=int, default=None)
        pack_parser.add_argument(
            "--compression",
            choices=("mono1-rle", "mono1", "index8-rle", "index8", "rle", "none"),
            default="mono1-rle",
        )
        pack_args = pack_parser.parse_args(sys.argv[2:])
        frame_count = pack_video_asset(
            pack_args.local,
            pack_args.output,
            width=pack_args.width,
            height=pack_args.height,
            fps=pack_args.fps,
            fit=pack_args.fit,
            max_frames=pack_args.max_frames,
            compression=pack_args.compression,
            start_frame=pack_args.start_frame,
            end_frame=pack_args.end_frame,
        )
        size = os.path.getsize(pack_args.output)
        print(f"Packed {frame_count} frames, {size} bytes -> {pack_args.output}")
        return 0

    parser = argparse.ArgumentParser(
        description="通过 UART 管理外部 Flash 文件")
    parser.add_argument("port", nargs="?", help="串口，例如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=921600, help="串口波特率")
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="列出当前检测到的串口后退出",
    )
    sub = parser.add_subparsers(dest="action")

    sub.add_parser("probe", help="检测 Flash Manager 协议是否有响应")
    sub.add_parser("list", help="List root directory")

    p_upload = sub.add_parser("upload", help="Upload a file")
    p_upload.add_argument("local", help="Local file path")
    p_upload.add_argument("remote", help="Remote path (e.g. /data.bin)")

    p_image = sub.add_parser("upload-image", help="Convert and upload a RGB565 image asset")
    p_image.add_argument("local", help="Local JPG/PNG path")
    p_image.add_argument("remote", help="Remote path (e.g. /air_bg.r565)")
    p_image.add_argument("--width", type=int, required=True)
    p_image.add_argument("--height", type=int, required=True)
    p_image.add_argument(
        "--fit", choices=("cover", "contain", "stretch"), default="cover"
    )
    p_image.add_argument("--mask", action="store_true", help="Store a 1-bit alpha mask")

    p_video = sub.add_parser("upload-video", help="Convert and upload a RGB565 video asset")
    p_video.add_argument("local", help="Local video/GIF path or frame directory")
    p_video.add_argument("remote", help="Remote path (e.g. /demo.v565)")
    p_video.add_argument("--width", type=int, required=True)
    p_video.add_argument("--height", type=int, required=True)
    p_video.add_argument("--fps", type=int, default=8)
    p_video.add_argument(
        "--fit", choices=("cover", "contain", "stretch"), default="cover"
    )
    p_video.add_argument("--max-frames", type=int, default=None)
    p_video.add_argument("--start-frame", type=int, default=0)
    p_video.add_argument("--end-frame", type=int, default=None)
    p_video.add_argument(
        "--compression",
        choices=("mono1-rle", "mono1", "index8-rle", "index8", "rle", "none"),
        default="mono1-rle",
    )

    p_pack_video = sub.add_parser("pack-video", help="Convert video/GIF/frames to a V565 file")
    p_pack_video.add_argument("local", help="Local video/GIF path or frame directory")
    p_pack_video.add_argument("output", help="Output .v565 file")
    p_pack_video.add_argument("--width", type=int, required=True)
    p_pack_video.add_argument("--height", type=int, required=True)
    p_pack_video.add_argument("--fps", type=int, default=8)
    p_pack_video.add_argument(
        "--fit", choices=("cover", "contain", "stretch"), default="cover"
    )
    p_pack_video.add_argument("--max-frames", type=int, default=None)
    p_pack_video.add_argument("--start-frame", type=int, default=0)
    p_pack_video.add_argument("--end-frame", type=int, default=None)
    p_pack_video.add_argument(
        "--compression",
        choices=("mono1-rle", "mono1", "index8-rle", "index8", "rle", "none"),
        default="mono1-rle",
    )

    p_dl = sub.add_parser("download", help="Download a file")
    p_dl.add_argument("remote", help="Remote path")
    p_dl.add_argument("local", help="Local file path")

    p_del = sub.add_parser("delete", help="Delete a file")
    p_del.add_argument("remote", help="Remote path")

    p_info = sub.add_parser("info", help="Get file info")
    p_info.add_argument("remote", help="Remote path")

    p_format = sub.add_parser("format", help="格式化文件系统（会清空 LittleFS 分区）")
    p_format.add_argument(
        "--yes",
        action="store_true",
        help="确认清空外部 Flash 的 LittleFS 分区",
    )

    args = parser.parse_args()

    if args.list_ports:
        try:
            print_serial_ports()
        except RuntimeError as exc:
            print(f"错误：{exc}", file=sys.stderr)
            return 2
        return 0

    if args.action == "pack-video":
        frame_count = pack_video_asset(
            args.local,
            args.output,
            width=args.width,
            height=args.height,
            fps=args.fps,
            fit=args.fit,
            max_frames=args.max_frames,
            compression=args.compression,
            start_frame=args.start_frame,
            end_frame=args.end_frame,
        )
        size = os.path.getsize(args.output)
        print(f"Packed {frame_count} frames, {size} bytes -> {args.output}")
        return 0

    if not args.port or not args.action:
        parser.error("请指定串口和操作，或使用 --list-ports 查看可用串口")

    def _progress(sent: int, total: Optional[int]) -> None:
        if total:
            pct = sent * 100 // total
            print(f"\r  {sent}/{total} ({pct}%)", end="", flush=True)
        else:
            print(f"\r  {sent} bytes", end="", flush=True)

    try:
        fm = FlashManager(args.port, baudrate=args.baud)
    except RuntimeError as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 2
    except (OSError, serial.SerialException) as exc:
        print(f"无法打开串口 {args.port}：{exc}", file=sys.stderr)
        error_text = str(exc).lower()
        if (
            getattr(exc, "errno", None) == 13
            or "permissionerror(13" in error_text
            or "access is denied" in error_text
            or "拒绝访问" in error_text
        ):
            print(
                "COM 口通常正被其他程序占用。请关闭串口助手、VS Code "
                "串口监视器、CCS/UniFlash 串口终端以及其他 flash_manager.py 进程，"
                "然后重新插拔设备再试。",
                file=sys.stderr,
            )
        elif "filenotfounderror" in error_text or "找不到指定的文件" in error_text:
            print("该串口不存在或设备已经断开。", file=sys.stderr)

        try:
            print_serial_ports(file=sys.stderr)
        except RuntimeError:
            pass
        return 2

    try:
        if args.action == "probe":
            if fm.reset():
                print("Flash Manager 协议连接正常。")
            else:
                raise OSError("设备未返回 Flash Manager 协议响应")

        elif args.action == "list":
            entries = fm.list_dir("/")
            print(f"{'Type':<6} {'Size':>10}  Name")
            print("-" * 50)
            for e in entries:
                print(f"{e['type']:<6} {e['size']:>10}  {e['name']}")
            print(f"\n{len(entries)} entries")

        elif args.action == "upload":
            print(f"Uploading {args.local} → {args.remote}")
            ok = fm.upload_file(args.local, args.remote,
                                progress_cb=_progress)
            print()
            print("OK" if ok else "FAILED")

        elif args.action == "upload-image":
            print(
                f"Converting {args.local} to {args.width}x{args.height} RGB565 "
                f"and uploading to {args.remote}"
            )
            ok = fm.upload_image(
                args.local,
                args.remote,
                width=args.width,
                height=args.height,
                fit=args.fit,
                with_mask=args.mask,
                progress_cb=_progress,
            )
            print()
            print("OK" if ok else "FAILED")

        elif args.action == "upload-video":
            print(
                f"Converting {args.local} to {args.width}x{args.height} "
                f"V565 at {args.fps} fps and uploading to {args.remote}"
            )
            ok = fm.upload_video(
                args.local,
                args.remote,
                width=args.width,
                height=args.height,
                fps=args.fps,
                fit=args.fit,
                max_frames=args.max_frames,
                compression=args.compression,
                start_frame=args.start_frame,
                end_frame=args.end_frame,
                progress_cb=_progress,
            )
            print()
            print("OK" if ok else "FAILED")

        elif args.action == "download":
            print(f"Downloading {args.remote} → {args.local}")
            ok = fm.download_file(args.remote, args.local,
                                  progress_cb=_progress)
            print()
            print("OK" if ok else "FAILED")

        elif args.action == "delete":
            ok = fm.delete(args.remote)
            print("OK" if ok else "FAILED")

        elif args.action == "info":
            info = fm.get_info(args.remote)
            if info:
                print(f"  type: {info['type']}")
                print(f"  size: {info['size']}")
            else:
                print("Not found")
                return 1

        elif args.action == "format":
            if not args.yes:
                print(
                    "格式化会清空外部 Flash 高 2 MiB LittleFS 分区。"
                    "确认后请重新执行并添加 --yes。",
                    file=sys.stderr,
                )
                return 2
            print("正在格式化 LittleFS 分区...")
            ok = fm.format()
            print("OK" if ok else "FAILED")

    except OSError as exc:
        print(f"操作失败：{exc}", file=sys.stderr)
        if "no response" in str(exc).lower() or "未返回" in str(exc):
            print(
                "串口已经打开，但 MCU 没有返回协议帧。请确认：\n"
                "  1. 已烧录 FLASH_MGR_ENABLE=1 的最新固件并复位；\n"
                "  2. 调试器 UART TX 接 PA11（MCU RX）；\n"
                "  3. 调试器 UART RX 接 PA10（MCU TX）；\n"
                "  4. 调试器与 MCU 共地，串口为 921600 8N1；\n"
                "  5. 当前选择的是调试器虚拟串口。",
                file=sys.stderr,
            )
        elif "filesystem corruption" in str(exc).lower():
            print(
                "文件系统已损坏。重新烧录最新上传固件后执行：\n"
                f"  python scripts/flash_manager.py {args.port} format --yes",
                file=sys.stderr,
            )
        return 1
    finally:
        fm.close()

    return 0


if __name__ == "__main__":
    import sys
    sys.exit(_main())
