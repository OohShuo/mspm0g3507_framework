"""
Flash Manager Workbench — ANSI colour helpers, banner, and output utilities.

All output is via plain print() with embedded ANSI escapes.  No heavy TUI
dependency is required.
"""

import os
import sys
import shutil

# ── ANSI escape sequences ──────────────────────────────────────────────

COLOR_RESET   = "\033[0m"
COLOR_RED     = "\033[31m"
COLOR_GREEN   = "\033[32m"
COLOR_YELLOW  = "\033[33m"
COLOR_BLUE    = "\033[34m"
COLOR_MAGENTA = "\033[35m"
COLOR_CYAN    = "\033[36m"
COLOR_BOLD    = "\033[1m"
COLOR_DIM     = "\033[2m"

_ANSI_ENABLED = None


def _supports_ansi() -> bool:
    """Return True if the terminal likely supports ANSI escape sequences."""
    global _ANSI_ENABLED
    if _ANSI_ENABLED is not None:
        return _ANSI_ENABLED

    # Windows check: prefer ANSI unless we're on old conhost without support
    if sys.platform == "win32":
        try:
            import ctypes
            kernel32 = ctypes.windll.kernel32
            # Try to enable virtual-terminal-processing
            h = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
            mode = ctypes.c_uint32()
            kernel32.GetConsoleMode(h, ctypes.byref(mode))
            ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
            kernel32.SetConsoleMode(
                h, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING
            )
            _ANSI_ENABLED = True
        except Exception:
            _ANSI_ENABLED = False
    else:
        _ANSI_ENABLED = hasattr(sys.stdout, "isatty") and sys.stdout.isatty()

    return _ANSI_ENABLED


def color_text(text: str, color: str = COLOR_RESET, bold: bool = False) -> str:
    """Wrap *text* in ANSI colour escapes (no-op when ANSI is unavailable)."""
    if not _supports_ansi():
        return text
    prefix = color
    if bold:
        prefix += COLOR_BOLD
    return f"{prefix}{text}{COLOR_RESET}"


def clear_screen() -> None:
    """Clear the terminal."""
    if _supports_ansi():
        sys.stdout.write("\033[2J\033[H")
    else:
        os.system("cls" if sys.platform == "win32" else "clear")


def _terminal_width() -> int:
    try:
        return shutil.get_terminal_size().columns
    except Exception:
        return 80


def print_banner(
    port: str = "",
    baudrate: int = 2000000,
    local_cwd: str = "./",
    remote_cwd: str = "/",
    connected: bool = False,
) -> None:
    """Print the Flash Manager Workbench banner with current status."""
    width = _terminal_width()
    width = max(width, 44)

    border = "═" * (width - 2)
    print(color_text(f"╔{border}╗", COLOR_CYAN, bold=True))
    pad = (width - 2 - 26) // 2
    pad_r = width - 2 - 26 - pad
    lhs = " " * pad
    rhs = " " * pad_r
    print(color_text(f"║{lhs}Flash Manager Workbench{rhs}║", COLOR_CYAN, bold=True))
    print(color_text(f"╚{border}╝", COLOR_CYAN, bold=True))

    status_color = COLOR_GREEN if connected else COLOR_YELLOW
    status_text = "connected" if connected else "disconnected"

    lines = [
        ("port", port if port else "(not set)"),
        ("baudrate", str(baudrate)),
        ("local cwd", local_cwd),
        ("remote cwd", remote_cwd),
        ("status", status_text),
    ]
    label_w = max(len(l) for l, _ in lines)
    for label, value in lines:
        print(f"   {color_text(label + ':', COLOR_DIM):<{label_w+8}}{color_text(value, status_color if label == 'status' else COLOR_RESET)}")
    print()


def print_success(msg: str) -> None:
    """Print a success message."""
    print(f"  {color_text('✅', COLOR_GREEN)} {msg}")


def print_warning(msg: str) -> None:
    """Print a warning message."""
    print(f"  {color_text('⚠️', COLOR_YELLOW)} {msg}")


def print_error(msg: str, show_traceback: bool = False) -> None:
    """Print an error message."""
    print(f"  {color_text('❌', COLOR_RED)} {msg}")


def print_info(msg: str) -> None:
    """Print an informational message."""
    print(f"  {color_text('ℹ️', COLOR_BLUE)} {msg}")


def progress_bar(sent: int, total: int, start_time: float) -> None:
    """Print a one-line progress indicator for upload/download.

    Call this repeatedly; it overwrites the previous line with ``\\r``.
    """
    if total > 0:
        pct = sent * 100.0 / total
        elapsed = max(time.time() - start_time, 0.001)
        kbps = (sent / 1024.0) / elapsed
        sys.stdout.write(
            f"\r  {color_text('⏳', COLOR_YELLOW)} "
            f"{pct:5.1f}%  {sent}/{total} bytes  {kbps:.1f} KB/s"
        )
    else:
        sys.stdout.write(f"\r  {color_text('⏳', COLOR_YELLOW)} {sent} bytes")
    sys.stdout.flush()


# Need time for progress_bar default
import time
