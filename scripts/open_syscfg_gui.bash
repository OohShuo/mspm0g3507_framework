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

SYSCFG_GUI="./tools/sysconfig/sysconfig_gui${SUFFIX}"
PRODUCT_JSON="./tools/product.json"
SYSCFG_FILE="./config/${PROJECT_NAME}.syscfg"

for required in "$SYSCFG_GUI" "$PRODUCT_JSON" "$SYSCFG_FILE"; do
    if [ ! -f "$required" ]; then
        echo "Error: $required not found."
        exit 1
    fi
done

"$SYSCFG_GUI" -p "$PRODUCT_JSON" "$SYSCFG_FILE"
