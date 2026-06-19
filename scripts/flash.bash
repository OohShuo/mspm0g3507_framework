#!/usr/bin/env bash
set -euo pipefail

ELF="build/arm/framework.elf"
PACK="tools/packs/TexasInstruments.MSPM0G_DFP.1.1.0.pack"
CONFIG="pyocd.yaml"

command -v pyocd > /dev/null 2>&1 || {
    echo "pyocd not found, please install pyocd and add it to your PATH"
    exit 1
}

if [ ! -f "$ELF" ]; then
    echo "$ELF not found, please build the framework first"
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    echo "$CONFIG not found, please create a pyocd.yaml file in the project root"
    exit 1
fi

if [ ! -f "$PACK" ]; then
    echo "$PACK not found, please download it and place it in tools/packs/"
    exit 1
fi

echo "Flashing $ELF ..."
pyocd flash "$ELF"

echo "Reset and run ..."
pyocd reset

echo "Done."