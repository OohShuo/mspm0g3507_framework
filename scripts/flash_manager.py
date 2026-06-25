#!/usr/bin/env python3
"""
Flash Manager Workbench — PC-side client for the Flash UART passthrough protocol.

Manages the MPU's LittleFS filesystem on W25Q32 external Flash over a
serial UART link.

Usage:
    # Interactive workbench mode (no arguments)
    python scripts/flash_manager.py

    # CLI mode (original compatibility)
    python scripts/flash_manager.py --list-ports
    python scripts/flash_manager.py <port> upload <local> <remote>
    python scripts/flash_manager.py <port> list /
    python scripts/flash_manager.py <port> download <remote> <local>
    python scripts/flash_manager.py <port> delete <remote>
    python scripts/flash_manager.py <port> info <remote>
    python scripts/flash_manager.py <port> format --yes
    python scripts/flash_manager.py <port> probe
    python scripts/flash_manager.py ports
"""

import sys


def main() -> int:
    argv = sys.argv[1:]

    # No arguments → interactive workbench
    if not argv:
        from flashmgr.ui import interactive_loop

        try:
            interactive_loop()
        except KeyboardInterrupt:
            print()
            from flashmgr.utils import color_text, COLOR_CYAN, COLOR_BOLD
            print(color_text("\n👋 Exiting Flash Manager Workbench.\n", COLOR_CYAN, bold=True))
        return 0

    # Standalone "ports" convenience command
    if argv == ["ports"] or argv == ["--list-ports"]:
        from flashmgr.client import print_serial_ports
        try:
            print_serial_ports()
        except RuntimeError as exc:
            print(f"Error: {exc}", file=sys.stderr)
            return 2
        return 0

    # All other arguments → CLI mode (original argparse-based)
    from flashmgr.cli import cli_main
    return cli_main(argv)


if __name__ == "__main__":
    sys.exit(main())
