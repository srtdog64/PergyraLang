#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PROBE="${PGY_TARGET_CAPABILITY_PROBE:-$ROOT_DIR/bin/test_target_capability_contract}"
if [[ "$PROBE" != *.exe && -x "${PROBE}.exe" ]]; then
    PROBE="${PROBE}.exe"
fi
if [[ ! -x "$PROBE" ]]; then
    echo "[target-capability] missing probe: $PROBE" >&2
    exit 1
fi
pgy_require_runnable_binary_here "target-capability" "$PROBE"
"$PROBE"

echo "[target-capability] target capability contract probe passed"
