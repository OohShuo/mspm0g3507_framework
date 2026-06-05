#!/bin/bash

PROJECT_NAME="$(basename "$(pwd)")"

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    SUFFIX=".bat"
else
    SUFFIX=".sh"
fi

SYSCFG_CLI="./tools/sysconfig/sysconfig_cli${SUFFIX}"
PRODUCT_JSON="./tools/product.json"
SYSCFG_FILE="./config/${PROJECT_NAME}.syscfg"

TEMP_DIR="./build/syscfg_temp"
FINAL_DIR="./src/syscfg"

WHITELIST=("ti_msp_dl_config.c" "ti_msp_dl_config.h" "device.opt")

rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"
mkdir -p "$FINAL_DIR"

"$SYSCFG_CLI" --compiler gcc \
    --product "$PRODUCT_JSON" \
    --output "$TEMP_DIR" \
    --quiet \
    "$SYSCFG_FILE"

if [ $? -ne 0 ]; then
    echo "SysConfig CLI execution failed. Please check the output for details."
    return 1
fi

for FILE in "${WHITELIST[@]}"; do
    if [ -f "$TEMP_DIR/$FILE" ]; then
        cp "$TEMP_DIR/$FILE" "$FINAL_DIR/"
        echo "$FILE generated and copied to $FINAL_DIR/"
    else
        echo "$FILE not found. Please check the SysConfig output."
    fi
done
