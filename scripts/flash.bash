#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

ELF="build/arm/framework.elf"
PACK="tools/packs/TexasInstruments.MSPM0G_DFP.1.1.0.pack"
CONFIG="pyocd.yaml"

command -v pyocd >/dev/null 2>&1 || {
    echo "pyocd not found. Install it and add it to PATH."
    exit 1
}

if [ ! -f "$ELF" ]; then
    echo "$ELF not found. Build the ARM target first: python3 scripts/cc.py --target arm"
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    echo "$CONFIG not found. Create pyocd.yaml in the project root."
    exit 1
fi

if [ ! -f "$PACK" ]; then
    echo "$PACK not found. Place the TI MSPM0G DFP pack under tools/packs/."
    exit 1
fi

echo "Flashing $ELF ..."
pyocd flash "$ELF" --config "$CONFIG"

echo "Reset and run ..."
pyocd reset --config "$CONFIG"

echo "Done."
