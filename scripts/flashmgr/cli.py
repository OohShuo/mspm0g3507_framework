"""
Flash Manager Workbench — CLI mode (command-line argument parsing).

Preserves the original argparse-based CLI from flash_manager.py so that
all existing scripts, Makefiles, and documentation continue to work.

Usage::

    from flashmgr.cli import cli_main
    import sys
    sys.exit(cli_main(sys.argv[1:]))
"""

import argparse
import os
import sys
from typing import Optional

import serial

from flashmgr.client import (
    FlashManager,
    get_serial_ports,
    print_serial_ports,
    pack_image_asset,
)


def cli_main(argv: list) -> int:
    """Entry point for command-line mode.

    Parameters
    ----------
    argv : list
        Argument list (sys.argv[1:]).

    Returns
    -------
    int
        Exit code (0 = success, 1 = operation failed, 2 = usage/config error).
    """
    parser = argparse.ArgumentParser(
        description="通过 UART 管理外部 Flash 文件")
    parser.add_argument("port", nargs="?", help="串口，例如 COM3 或 /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=2000000, help="串口波特率")
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

    args = parser.parse_args(argv)

    if args.list_ports:
        try:
            print_serial_ports()
        except RuntimeError as exc:
            print(f"错误：{exc}", file=sys.stderr)
            return 2
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
                "  1. 已烧录 runtime_mode: flash_mgr 的最新固件并复位；\n"
                "  2. 调试器 UART TX 接 PA11（MCU RX）；\n"
                "  3. 调试器 UART RX 接 PA10（MCU TX）；\n"
                "  4. 调试器与 MCU 共地，串口为 2000000 8N1；\n"
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
