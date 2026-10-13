#!/usr/bin/env bash
# Canonical SoT registry gate: registry/Coq projection, single producer
# definition, path resolution, fact-owner classification and layer imports.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python >/dev/null 2>&1 \
        && python -c 'import sys; raise SystemExit(sys.version_info < (3, 9))'; then
        PYTHON_BIN="$(command -v python)"
    elif command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    else
        echo "[sot-authority-edge] Python is required" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" "$ROOT_DIR/scripts/sot_registry_gate.py" "$ROOT_DIR"
