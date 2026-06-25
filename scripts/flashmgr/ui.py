"""
Flash Manager Workbench — interactive menu UI.

Provides the interactive-mode loop with coloured banner, sub-menus for
connect / upload / download / list / info / delete / format / config,
and safe confirmation for destructive operations.

All protocol work is delegated to :class:`flashmgr.client.FlashManager`;
this module only handles user interaction.
"""

import os
import sys
import time
from typing import Optional

import serial

from flashmgr.client import (
    FlashManager,
    get_serial_ports,
    print_serial_ports,
    pack_image_asset,
)
from flashmgr.utils import (
    clear_screen,
    color_text,
    print_banner,
    print_error,
    print_info,
    print_success,
    print_warning,
    progress_bar,
    COLOR_RESET,
    COLOR_BOLD,
    COLOR_CYAN,
    COLOR_DIM,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_RED,
    COLOR_MAGENTA,
)

# ── Runtime configuration (session-only, not persisted) ────────────────

config = {
    "port": None,
    "baudrate": 2000000,
    "timeout": 5.0,
    "local_cwd": ".",
    "remote_cwd": "/",
    "auto_connect": False,
    "verify_after_upload": True,
    "chunk_size": 512,
    "show_debug": False,
}


# ── Helpers ────────────────────────────────────────────────────────────

def _read_port() -> Optional[str]:
    """Return the current port from config."""
    return config.get("port")


def _ensure_client() -> Optional[FlashManager]:
    """Return a connected FlashManager, or prompt to connect.

    Returns None if the user declined to connect.
    """
    if config.get("port") and config.get("_client"):
        return config["_client"]

    if config.get("auto_connect"):
        return _do_connect()

    print_warning("Device is not connected.")
    ans = input("  Connect now? [Y/n] ").strip().lower()
    if ans in ("", "y", "yes"):
        return _do_connect()
    return None


def _do_connect() -> Optional[FlashManager]:
    """Open a serial connection using the configured port/baudrate.

    Stores the client in config["_client"] on success.
    Returns the FlashManager or None on failure.
    """
    port = config.get("port")
    if not port:
        print_error("No port configured. Use [O] Connect to select one.")
        return None

    # Close any existing client
    old = config.get("_client")
    if old:
        try:
            old.close()
        except Exception:
            pass
        config["_client"] = None

    try:
        fm = FlashManager(
            port=port,
            baudrate=config["baudrate"],
            timeout=config["timeout"],
        )
    except RuntimeError as exc:
        print_error(f"Cannot create client: {exc}")
        return None
    except (OSError, serial.SerialException) as exc:
        print_error(f"Cannot open port {port}: {exc}")
        _handle_serial_error(exc)
        return None

    # Probe the device
    if fm.reset():
        config["_client"] = fm
        print_success(f"Connected to {port} @ {config['baudrate']}")
        return fm
    else:
        print_error("Device did not respond to Flash Manager protocol probe.")
        fm.close()
        return None


def _handle_serial_error(exc: Exception) -> None:
    """Print helpful diagnostics for common serial errors."""
    error_text = str(exc).lower()
    if (
        getattr(exc, "errno", None) == 13
        or "permissionerror" in error_text
        or "access is denied" in error_text
        or "拒绝访问" in error_text
    ):
        print_info(
            "Port is in use. Close serial monitors, VS Code serial "
            "terminal, or other flash_manager.py processes and try again."
        )
    elif "filenotfounderror" in error_text or "找不到" in error_text:
        print_info("Port does not exist or device was disconnected.")


def _close_client() -> None:
    """Close the active client and clear it from config."""
    fm = config.get("_client")
    if fm:
        try:
            fm.close()
        except Exception:
            pass
        config["_client"] = None


def _default_remote(local_path: str, remote_dir: str = "/") -> str:
    """Derive a default remote path from a local filename.

    >>> _default_remote("build/res.bin")
    '/res.bin'
    >>> _default_remote("./assets/bg.r565", "/assets")
    '/assets/bg.r565'
    """
    name = os.path.basename(local_path)
    if remote_dir.endswith("/"):
        return f"{remote_dir}{name}"
    return f"{remote_dir}/{name}"


# ── Top-level interactive loop ─────────────────────────────────────────

def interactive_loop() -> None:
    """Enter the Flash Manager Workbench interactive mode."""
    # On first launch, try auto-connect or let user pick a port
    clear_screen()
    while True:
        _refresh_and_show()
        choice = input(color_text("Select an option » ", COLOR_BOLD)).strip().lower()

        if choice in ("o",):
            _menu_connect()
        elif choice in ("i",):
            _action_info()
        elif choice in ("l",):
            _action_list()
        elif choice in ("u",):
            _action_upload()
        elif choice in ("d",):
            _action_download()
        elif choice in ("r",):
            _action_remove()
        elif choice in ("f",):
            _action_format()
        elif choice in ("p",):
            _action_ports()
        elif choice in ("c",):
            _menu_config()
        elif choice in ("h",):
            _action_help()
            input(color_text("\nPress Enter to return...", COLOR_DIM))
        elif choice in ("q", "quit", "exit"):
            _close_client()
            print(color_text("\n👋 Exiting Flash Manager Workbench.\n", COLOR_CYAN, bold=True))
            break
        else:
            print_error("Invalid choice. Press H for help.")


def _refresh_and_show() -> None:
    """Clear screen and print the banner + main menu."""
    clear_screen()
    connected = config.get("_client") is not None
    print_banner(
        port=config.get("port") or "(not set)",
        baudrate=config.get("baudrate", 2000000),
        local_cwd=config.get("local_cwd", "./"),
        remote_cwd=config.get("remote_cwd", "/"),
        connected=connected,
    )

    print("  " + color_text("[O] Connect", COLOR_GREEN, bold=True)
          + "       " + color_text("[I] Info", COLOR_CYAN)
          + "          " + color_text("[L] List", COLOR_YELLOW))
    print("  " + color_text("[U] Upload", COLOR_GREEN, bold=True)
          + "        " + color_text("[D] Download", COLOR_CYAN)
          + "      " + color_text("[R] Remove", COLOR_YELLOW))
    print("  " + color_text("[F] Format FS", COLOR_GREEN, bold=True)
          + "     " + color_text("[P] Ports", COLOR_CYAN)
          + "         " + color_text("[C] Config", COLOR_YELLOW))
    print("  " + color_text("[H] Help", COLOR_GREEN, bold=True)
          + "         " + color_text("[Q] Quit", COLOR_CYAN))
    print()


# ── Connect menu ───────────────────────────────────────────────────────

def _menu_connect() -> None:
    """Interactive port selection and connect."""
    clear_screen()
    print(color_text("=== Serial Ports ===", COLOR_CYAN, bold=True))
    print()

    try:
        ports = get_serial_ports()
    except RuntimeError as exc:
        print_error(str(exc))
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    if not ports:
        print_info("No serial ports detected.")
        print()
    else:
        for i, (dev, desc, hwid) in enumerate(ports, 1):
            marker = ""
            if dev == config.get("port"):
                marker = color_text(" ← current", COLOR_GREEN)
            print(f"  [{color_text(str(i), COLOR_BOLD)}] {dev:<16} {desc}{marker}")

    print(f"  [{color_text('M', COLOR_BOLD)}] Manual input")
    print(f"  [{color_text('Q', COLOR_BOLD)}] Return")
    print()

    choice = input(color_text("Select port or M/Q » ", COLOR_BOLD)).strip().lower()

    if choice in ("q",):
        return

    if choice == "m":
        port = input("  Enter port path: ").strip()
    else:
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                port = ports[idx][0]
            else:
                print_error("Invalid port number.")
                input(color_text("\nPress Enter to return...", COLOR_DIM))
                return
        except ValueError:
            print_error("Invalid choice.")
            input(color_text("\nPress Enter to return...", COLOR_DIM))
            return

    if port:
        config["port"] = port
        print_info(f"Port set to {port}. Connecting...")
        _do_connect()
        input(color_text("\nPress Enter to continue...", COLOR_DIM))


# ── Action: Ports ──────────────────────────────────────────────────────

def _action_ports() -> None:
    """List available serial ports."""
    clear_screen()
    print(color_text("=== Serial Ports ===", COLOR_CYAN, bold=True))
    print()
    try:
        print_serial_ports()
    except RuntimeError as exc:
        print_error(str(exc))
    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: Info ───────────────────────────────────────────────────────

def _action_info() -> None:
    """Show device / filesystem info and root directory summary."""
    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    clear_screen()
    print(color_text("=== Device / Flash Info ===", COLOR_CYAN, bold=True))
    print()

    # Get filesystem root info
    try:
        info = fm.get_info("/")
        if info:
            print(f"  Flash chip      : {'W25Q32 (assumed)':<20}")
            print(f"  Root type       : {info['type']:<20}")
        else:
            print_warning("Could not read filesystem root info.")
    except OSError as exc:
        print_error(str(exc))

    # List root to show usage
    try:
        entries = fm.list_dir("/")
        total_files = sum(1 for e in entries if e["type"] == "file")
        total_dirs = sum(1 for e in entries if e["type"] == "dir")
        total_size = sum(e["size"] for e in entries if e["type"] == "file")
        print(f"  Files           : {total_files}")
        print(f"  Directories     : {total_dirs}")
        print(f"  Total data size : {total_size:,} B  ({total_size/1024:.1f} KiB)")
        print(f"  Status          : {'OK' if info else 'Unknown'}")
    except OSError:
        print_warning("Could not list root directory (filesystem may be corrupted).")

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: List ───────────────────────────────────────────────────────

def _action_list() -> None:
    """List the current remote directory."""
    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    remote_cwd = config.get("remote_cwd", "/")
    clear_screen()
    print(color_text(f"=== Remote: {remote_cwd} ===", COLOR_CYAN, bold=True))
    print()

    try:
        entries = fm.list_dir(remote_cwd)
    except OSError as exc:
        print_error(str(exc))
        if "corruption" in str(exc).lower():
            print_info(
                "Filesystem may be corrupted. "
                "Re-flash firmware and run: format --yes"
            )
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    if not entries:
        print_info("(empty directory)")
    else:
        # Header
        print(f"  {'Name':<30} {'Type':<10} {'Size':>12}")
        print("  " + "-" * 54)
        for e in entries:
            size_str = f"{e['size']:,}" if e["size"] else "-"
            print(f"  {e['name']:<30} {e['type']:<10} {size_str:>12}")
        print()
        print_info(f"{len(entries)} entries")

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: Upload ─────────────────────────────────────────────────────

def _action_upload() -> None:
    """Upload a file with progress and optional verification."""
    clear_screen()
    print(color_text("--- Upload File ---", COLOR_CYAN, bold=True))
    print()

    # Local path
    local_path = input("  Local file path  > ").strip()
    if not local_path:
        print_info("Cancelled.")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    if not os.path.isfile(local_path):
        print_error(f"Local file not found: {local_path}")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    file_size = os.path.getsize(local_path)
    print_info(f"File size: {file_size:,} bytes ({file_size/1024:.1f} KiB)")

    # Remote path
    default_r = _default_remote(local_path, config.get("remote_cwd", "/"))
    remote_path = input(f"  Remote file path [{default_r}] > ").strip()
    if not remote_path:
        remote_path = default_r

    # Verify
    verify = config.get("verify_after_upload", True)
    if verify:
        ans = input("  Verify after upload? [Y/n] ").strip().lower()
        if ans in ("n", "no"):
            verify = False

    print()

    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    # Check if remote exists
    try:
        existing = fm.get_info(remote_path)
        if existing:
            print_warning(f"Remote file '{remote_path}' already exists ({existing.get('size', '?')} bytes).")
            ans = input("  Overwrite? [y/N] ").strip().lower()
            if ans not in ("y", "yes"):
                print_info("Upload cancelled.")
                input(color_text("\nPress Enter to return...", COLOR_DIM))
                return
    except Exception:
        pass  # proceed if info check fails

    # Upload
    start_time = time.time()
    last_update = [0]

    def _progress_cb(sent: int, total: int) -> None:
        last_update[0] = sent
        progress_bar(sent, total, start_time)

    try:
        ok = fm.upload_file(local_path, remote_path, progress_cb=_progress_cb)
        elapsed = time.time() - start_time
        print()  # newline after progress
        if ok:
            kbps = (last_update[0] / 1024.0) / max(elapsed, 0.001)
            print_success(
                f"Uploaded {last_update[0]:,} bytes in {elapsed:.1f}s "
                f"({kbps:.1f} KB/s)"
            )
        else:
            print_error("Upload failed.")
    except FileNotFoundError as exc:
        print_error(f"Upload failed: {exc}")
    except (OSError, Exception) as exc:
        print_error(f"Upload failed: {exc}")
        if config.get("show_debug"):
            import traceback
            traceback.print_exc()

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: Download ───────────────────────────────────────────────────

def _action_download() -> None:
    """Download a file with progress."""
    clear_screen()
    print(color_text("--- Download File ---", COLOR_CYAN, bold=True))
    print()

    # Remote path
    default_r = config.get("remote_cwd", "/")
    remote_path = input(f"  Remote file path [{default_r}] > ").strip()
    if not remote_path:
        print_info("Cancelled.")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    # Derive local name
    local_default = os.path.join(
        config.get("local_cwd", "."),
        os.path.basename(remote_path) or "download.bin",
    )
    local_path = input(f"  Local file path  [{local_default}] > ").strip()
    if not local_path:
        local_path = local_default

    # Check local overwrite
    if os.path.exists(local_path):
        print_warning(f"Local file '{local_path}' already exists.")
        ans = input("  Overwrite? [y/N] ").strip().lower()
        if ans not in ("y", "yes"):
            print_info("Download cancelled.")
            input(color_text("\nPress Enter to return...", COLOR_DIM))
            return

    print()

    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    # First check if remote file exists
    try:
        info = fm.get_info(remote_path)
        if not info:
            print_error(f"Remote file not found: {remote_path}")
            input(color_text("\nPress Enter to return...", COLOR_DIM))
            return
        remote_size = info.get("size", 0)
        print_info(f"Remote file size: {remote_size:,} bytes")
    except Exception:
        pass  # proceed anyway

    start_time = time.time()

    def _progress_cb(received: int, total) -> None:
        progress_bar(received, total or 0, start_time)

    try:
        ok = fm.download_file(remote_path, local_path, progress_cb=_progress_cb)
        elapsed = time.time() - start_time
        print()  # newline
        if ok:
            local_size = os.path.getsize(local_path)
            kbps = (local_size / 1024.0) / max(elapsed, 0.001)
            print_success(
                f"Downloaded {local_size:,} bytes in {elapsed:.1f}s "
                f"({kbps:.1f} KB/s)"
            )
        else:
            print_error("Download failed.")
    except (OSError, Exception) as exc:
        print_error(f"Download failed: {exc}")
        if config.get("show_debug"):
            import traceback
            traceback.print_exc()

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: Remove (Delete) ────────────────────────────────────────────

def _action_remove() -> None:
    """Delete a remote file with confirmation."""
    clear_screen()
    print(color_text("--- Remove File ---", COLOR_CYAN, bold=True))
    print()

    remote_path = input(f"  Remote file path [{config.get('remote_cwd', '/')}] > ").strip()
    if not remote_path:
        print_info("Cancelled.")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    # Confirm
    print()
    print_warning(f"This will DELETE remote file '{remote_path}' permanently.")
    ans = input(color_text("  Type 'yes' to confirm > ", COLOR_RED)).strip()
    if ans != "yes":
        print_info("Delete cancelled.")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    try:
        ok = fm.delete(remote_path)
        if ok:
            print_success(f"Deleted: {remote_path}")
        else:
            print_error(f"Delete failed (file may not exist): {remote_path}")
    except OSError as exc:
        print_error(f"Delete failed: {exc}")

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Action: Format ─────────────────────────────────────────────────────

def _action_format() -> None:
    """Format the LittleFS partition.

    Requires the user to type 'FORMAT' for confirmation (not just 'y').
    """
    clear_screen()
    print(color_text("=== Format File System ===", COLOR_RED, bold=True))
    print()
    print_warning("This will format the external Flash LittleFS partition.")
    print_warning("ALL FILES WILL BE LOST.")
    print()

    ans = input(color_text("  Type 'FORMAT' to continue > ", COLOR_RED, bold=True)).strip()
    if ans != "FORMAT":
        print_info("Format cancelled.")
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    fm = _ensure_client()
    if not fm:
        input(color_text("\nPress Enter to return...", COLOR_DIM))
        return

    print()
    print_info("Formatting LittleFS partition...")
    try:
        ok = fm.format()
        if ok:
            print_success("Format complete.")
        else:
            print_error("Format failed.")
    except OSError as exc:
        print_error(f"Format failed: {exc}")

    print()
    input(color_text("Press Enter to return...", COLOR_DIM))


# ── Config menu ────────────────────────────────────────────────────────

def _menu_config() -> None:
    """Interactive configuration sub-menu."""
    while True:
        clear_screen()
        print(color_text("--- Config Menu ---", COLOR_MAGENTA, bold=True))
        print()
        _show_config()
        print()
        print(f"  [{color_text('S', COLOR_BOLD)}] Show config       "
              f"[{color_text('P', COLOR_BOLD)}] Set port")
        print(f"  [{color_text('B', COLOR_BOLD)}] Set baudrate      "
              f"[{color_text('L', COLOR_BOLD)}] Set local cwd")
        print(f"  [{color_text('R', COLOR_BOLD)}] Set remote cwd     "
              f"[{color_text('V', COLOR_BOLD)}] Toggle verify after upload")
        print(f"  [{color_text('A', COLOR_BOLD)}] Toggle auto-connect "
              f"[{color_text('D', COLOR_BOLD)}] Toggle debug")
        print(f"  [{color_text('Q', COLOR_BOLD)}] Return to main")
        print()

        choice = input(color_text("config » ", COLOR_MAGENTA, bold=True)).strip().lower()

        if choice in ("q",):
            return
        elif choice == "s":
            pass  # config already shown this cycle, will re-show next loop
        elif choice == "p":
            p = input("  Port (e.g. /dev/ttyACM0): ").strip()
            if p:
                config["port"] = p
                # Invalidate existing client so user reconnects
                _close_client()
                print_success(f"Port set to: {p}")
        elif choice == "b":
            b = input("  Baudrate [2000000]: ").strip()
            if b:
                try:
                    config["baudrate"] = int(b)
                    print_success(f"Baudrate set to: {config['baudrate']}")
                except ValueError:
                    print_error("Invalid baudrate (must be an integer).")
        elif choice == "l":
            d = input(f"  Local working dir [{config['local_cwd']}]: ").strip()
            if d:
                config["local_cwd"] = d
                print_success(f"Local cwd set to: {d}")
        elif choice == "r":
            d = input(f"  Remote working dir [{config['remote_cwd']}]: ").strip()
            if d:
                if not d.startswith("/"):
                    d = "/" + d
                config["remote_cwd"] = d
                print_success(f"Remote cwd set to: {d}")
        elif choice == "v":
            config["verify_after_upload"] = not config["verify_after_upload"]
            state = "ON" if config["verify_after_upload"] else "OFF"
            print_success(f"Verify after upload: {state}")
        elif choice == "a":
            config["auto_connect"] = not config["auto_connect"]
            state = "ON" if config["auto_connect"] else "OFF"
            print_success(f"Auto-connect: {state}")
        elif choice == "d":
            config["show_debug"] = not config["show_debug"]
            state = "ON" if config["show_debug"] else "OFF"
            print_success(f"Debug: {state}")
        elif choice == "m":
            # Hidden: manual baudrate entry
            b = input("  Enter new baudrate: ").strip()
            if b:
                try:
                    config["baudrate"] = int(b)
                    print_success(f"Baudrate set to: {config['baudrate']}")
                except ValueError:
                    print_error("Invalid baudrate.")
        else:
            print_error("Invalid choice.")

        if choice != "s":
            input(color_text("\nPress Enter to continue...", COLOR_DIM))


def _show_config() -> None:
    """Print the current config as a table."""
    items = [
        ("port", config.get("port", "(not set)")),
        ("baudrate", config.get("baudrate", 2000000)),
        ("timeout", f"{config.get('timeout', 5.0):.1f}s"),
        ("local cwd", config.get("local_cwd", "./")),
        ("remote cwd", config.get("remote_cwd", "/")),
        ("auto connect", "ON" if config.get("auto_connect") else "OFF"),
        ("verify after upload", "ON" if config.get("verify_after_upload") else "OFF"),
        ("chunk size", config.get("chunk_size", 512)),
        ("show debug", "ON" if config.get("show_debug") else "OFF"),
    ]
    label_w = max(len(l) for l, _ in items)
    for label, value in items:
        print(f"  {color_text(label + ':', COLOR_DIM):<{label_w+8}} {value}")


# ── Help ───────────────────────────────────────────────────────────────

def _action_help() -> None:
    """Print help information."""
    clear_screen()
    print(color_text("=== Flash Manager Workbench Help ===", COLOR_CYAN, bold=True))
    print()
    print("  INTERACTIVE MODE (no arguments):")
    print(f"    {color_text('python scripts/flash_manager.py', COLOR_BOLD)}")
    print()
    print("  CLI MODE (with arguments):")
    print(f"    {color_text('python scripts/flash_manager.py <port> <command> [...]', COLOR_BOLD)}")
    print()
    print("  Commands:")
    print(f"    {color_text('ports', COLOR_YELLOW)}              List available serial ports")
    print(f"    {color_text('<port> probe', COLOR_YELLOW)}        Test Flash Manager protocol")
    print(f"    {color_text('<port> list', COLOR_YELLOW)}         List root directory")
    print(f"    {color_text('<port> info <remote>', COLOR_YELLOW)}   Get file/dir info")
    print(f"    {color_text('<port> upload <local> <remote>', COLOR_YELLOW)}   Upload a file")
    print(f"    {color_text('<port> upload-image <local> <remote> --width W --height H', COLOR_YELLOW)}")
    print(f"    {color_text('<port> download <remote> <local>', COLOR_YELLOW)}   Download a file")
    print(f"    {color_text('<port> delete <remote>', COLOR_YELLOW)}   Delete a file")
    print(f"    {color_text('<port> format --yes', COLOR_YELLOW)}        Format filesystem")
    print()
    print("  Protocol: UART 2000000 8N1, chunked transfer with CRC16 verification.")
    print("  MCU commands: READ, WRITE, DELETE, LIST, INFO, FORMAT, RESET.")
    print()
    print("  Notes:")
    print("  - mkdir / tree / raw-read / raw-write are not supported by current MCU protocol.")
    print("  - Destructive operations (delete, format) require explicit confirmation.")
    print("  - Ctrl+C quits the workbench gracefully.")
