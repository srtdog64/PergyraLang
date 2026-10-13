#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/post_selfhost_validation_bug_classes.json"
DOCUMENT="$ROOT_DIR/docs/post_selfhost_validation_milestone.md"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN=python
    else
        echo "[post-selfhost-validation] python is required" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_post_selfhost_validation_manifest.py" \
    "$MANIFEST" "$DOCUMENT" --check

echo "[post-selfhost-validation] generated document matches the manifest"
