#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

rm -rf "$PROJECT_ROOT/build"
echo "Removed $PROJECT_ROOT/build"
