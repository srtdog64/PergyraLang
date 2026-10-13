#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

RIR_TEST="${RIR_TEST_BIN:-$ROOT_DIR/bin/test_rir}"
if [[ "$RIR_TEST" != *.exe && -x "${RIR_TEST}.exe" ]]; then
    RIR_TEST="${RIR_TEST}.exe"
fi
if [[ ! -x "$RIR_TEST" ]]; then
    echo "[dir-domain-identity] missing RIR test binary: $RIR_TEST" >&2
    exit 1
fi
pgy_require_runnable_binary_here "dir-domain-identity" "$RIR_TEST"

output="$($RIR_TEST 2>&1)"
printf '%s\n' "$output"
if ! grep -Eq '=== Results: [0-9]+ passed, 0 failed ===' <<<"$output"; then
    echo "[dir-domain-identity] RIR identity negative gate failed" >&2
    exit 1
fi

echo "[dir-domain-identity] DIR source/owner identity is required for RIR validation"
