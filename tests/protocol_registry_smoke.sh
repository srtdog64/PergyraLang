#!/usr/bin/env bash
# Derived Protocol/ABI/API crosswalk gate. The canonical SoT owner registry
# remains the authority; this gate only rejects stale or incomplete references.

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
        echo "[protocol-registry] Python 3.9+ is required" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" "$ROOT_DIR/scripts/protocol_registry_gate.py" "$ROOT_DIR"
