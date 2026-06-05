#!bin/bash

PROJECT_NAME="$(basename "$(pwd)")"

if (WINDOWS=true) then
    SUFFIX=".bat"
else
    SUFFIX=".sh"
fi

SYSCFG_GUI_SH="./tools/sysconfig/sysconfig_gui${SUFFIX}"
PRODUCT_JSON="./tools/product.json"
SYSCFG_FILE="./config/${PROJECT_NAME}.syscfg"

if [ ! -f "$SYSCFG_GUI_SH" ]; then
    echo "Error: $SYSCFG_GUI_SH not found. Please check the path."
    return 1
fi

if [ ! -f "$PRODUCT_JSON" ]; then
    echo "Error: $PRODUCT_JSON not found. Please check the path."
    return 1
fi

if [ ! -f "$SYSCFG_FILE" ]; then
    echo "Error: $SYSCFG_FILE not found. Please check the path."
    return 1
fi

"$SYSCFG_GUI_SH" -p "$PRODUCT_JSON" "$SYSCFG_FILE"
