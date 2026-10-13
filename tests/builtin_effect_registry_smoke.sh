#!/usr/bin/env bash
# Fixed ambient-call policy only, not whole-language effect completeness.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
PYTHON_BIN="${PYTHON_BIN:-python3}"
"$PYTHON_BIN" scripts/render_builtin_effect_registry.py --check
echo 'fixed builtin effect registry smoke: ok'
