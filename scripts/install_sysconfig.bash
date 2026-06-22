#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

INSTALL_SCRIPT="${SYSCONFIG_INSTALLER:-./tools/sysconfig-1.27.1_4634-setup.run}"
INSTALL_DIR="${SYSCONFIG_INSTALL_DIR:-$PROJECT_ROOT/tools/sysconfig}"

if [ ! -f "$INSTALL_SCRIPT" ]; then
    cat <<MSG
Error: SysConfig installer not found: $INSTALL_SCRIPT

The installer is intentionally not stored under scripts/.
Download TI SysConfig and either:
  1. place sysconfig-1.27.1_4634-setup.run at tools/, or
  2. set SYSCONFIG_INSTALLER=/path/to/sysconfig-setup.run

Example:
  SYSCONFIG_INSTALLER=~/Downloads/sysconfig-1.27.1_4634-setup.run bash scripts/install_sysconfig.bash
MSG
    exit 1
fi

chmod +x "$INSTALL_SCRIPT"
"$INSTALL_SCRIPT" --prefix "$INSTALL_DIR" --mode unattended --unattendedmodeui none

rm -rf "$INSTALL_DIR/install_logs" "$INSTALL_DIR/tests"
rm -f \
    "$INSTALL_DIR/TI sysconfig.desktop" \
    "$INSTALL_DIR/uninstall" \
    "$INSTALL_DIR/uninstall.dat" \
    "$INSTALL_DIR/update.ini"

echo "SysConfig installed to $INSTALL_DIR"
