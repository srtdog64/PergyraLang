#!/usr/bin/env bash
# Exercise the installed emitters, actual frees, and independent leak controls.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
export PGY_TEST_ROOT="$ROOT_DIR"
exec "${PYTHON_BIN:-python3}" "$ROOT_DIR/tests/self_hosted/parity/ownership/collection_replacement_cleanup_owner.py"
