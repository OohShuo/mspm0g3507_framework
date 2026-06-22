#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_NAME="$(basename "$PROJECT_ROOT")"

cd "$PROJECT_ROOT"

if [[ "${OSTYPE:-}" == "msys" || "${OSTYPE:-}" == "win32" ]]; then
    SUFFIX=".bat"
else
    SUFFIX=".sh"
fi

SYSCFG_CLI="./tools/sysconfig/sysconfig_cli${SUFFIX}"
PRODUCT_JSON="./tools/product.json"
SYSCFG_FILE="./config/${PROJECT_NAME}.syscfg"

TEMP_DIR="./build/syscfg_temp"
FINAL_DIR="./config/syscfg"

WHITELIST=("ti_msp_dl_config.c" "ti_msp_dl_config.h" "device.opt")

for required in "$SYSCFG_CLI" "$PRODUCT_JSON" "$SYSCFG_FILE"; do
    if [ ! -f "$required" ]; then
        echo "Error: $required not found."
        exit 1
    fi
done

rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR" "$FINAL_DIR"

if ! "$SYSCFG_CLI" --compiler gcc \
    --product "$PRODUCT_JSON" \
    --output "$TEMP_DIR" \
    --quiet \
    "$SYSCFG_FILE"; then
    echo "SysConfig CLI execution failed. Please check the output above."
    exit 1
fi

for FILE in "${WHITELIST[@]}"; do
    if [ -f "$TEMP_DIR/$FILE" ]; then
        cp "$TEMP_DIR/$FILE" "$FINAL_DIR/"
        echo "$FILE generated and copied to $FINAL_DIR/"
    else
        echo "Warning: $FILE not found in SysConfig output."
    fi
done
