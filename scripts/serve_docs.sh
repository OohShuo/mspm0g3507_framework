#!/usr/bin/env bash
# Generate and open the mkdocs documentation site.
#
# Usage:
#   bash scripts/serve_docs.sh            # live preview (auto-reload on changes)
#   bash scripts/serve_docs.sh --build    # static build + open in browser
#   bash scripts/serve_docs.sh --help
#
# Requirements: mkdocs (1.6+), mkdocs-material

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

print_help() {
    echo "Usage: bash scripts/serve_docs.sh [OPTION]"
    echo ""
    echo "Options:"
    echo "  (none)      Start live-reload dev server at http://127.0.0.1:8000"
    echo "  --build     Build static site to site/ and open index.html"
    echo "  --help      Show this help"
}

open_browser() {
    local url="$1"
    if command -v xdg-open &>/dev/null; then
        xdg-open "$url" &>/dev/null &
    elif command -v open &>/dev/null; then
        open "$url" &>/dev/null &
    elif command -v start &>/dev/null; then
        start "$url" &>/dev/null &
    else
        echo "⚠  Cannot open browser — open manually: $url"
    fi
}

case "${1:-}" in
    --help|-h)
        print_help
        exit 0
        ;;
    --build)
        echo "==> Building static site to site/ ..."
        mkdocs build --strict
        echo "==> Build done."
        open_browser "$PROJECT_ROOT/site/index.html"
        echo "==> Opened site/index.html in browser."
        ;;
    *)
        echo "==> Starting mkdocs serve at http://127.0.0.1:8000"
        echo "    Press Ctrl+C to stop."
        echo ""
        # Give mkdocs a moment to start, then open browser
        (sleep 1 && open_browser "http://127.0.0.1:8000") &
        mkdocs serve
        ;;
esac
