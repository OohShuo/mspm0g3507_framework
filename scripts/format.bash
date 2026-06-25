#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

STYLE_FILE="$PROJECT_ROOT/.clang-format"

if ! command -v clang-format &>/dev/null; then
    echo "Error: clang-format not found in PATH" >&2
    exit 1
fi

if [ ! -f "$STYLE_FILE" ]; then
    echo "Error: .clang-format not found at $STYLE_FILE" >&2
    exit 1
fi

find "$PROJECT_ROOT/src" "$PROJECT_ROOT/lib/local_lib" "$PROJECT_ROOT/config" \
    \( \
        \( -path "*/src/*" -o -path "*/local_lib/*" \) \
        ! -name "*_img.c" \
        ! -name "*badapple_builtin_video.h" \
        \( -name "*.c" -o -name "*.h" \) \
        -o \
        -path "*/config/*" \
        \( -name "app_config.h" -o -name "board_config.h" -o -name "test_config.h" -o -name "lfs_config.h" -o -name "global_config.h" \) \
    \) \
    -exec clang-format -i -style="file:$STYLE_FILE" --verbose {} +
