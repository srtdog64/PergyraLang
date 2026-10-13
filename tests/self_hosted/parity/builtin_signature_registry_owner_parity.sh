#!/usr/bin/env bash
# The builtin row registry owns its population. Runtime readiness may validate
# the projected rows, but it may not mirror the row count as another fact.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PROBE="$ROOT_DIR/tests/self_hosted/fixtures/builtin_signature_contract_ready.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/builtin_signature_registry}"

[[ -x "$PGY" ]] || { echo "[self-host-parity:builtin-signature] missing compiler: $PGY" >&2; exit 1; }
[[ -f "$PROBE" ]] || { echo "[self-host-parity:builtin-signature] probe is missing" >&2; exit 1; }

mkdir -p "$BUILD_DIR"
PROBE_ARG="$(pgy_path_for_compiler "$PGY" "$PROBE")"
assert_llvm_leg "self-host-parity:builtin-signature" "$PROBE_ARG" "$BUILD_DIR"
grep -Fxq 'builtin-signature-ready' "$BUILD_DIR/main_c_leg.out" || {
    echo "[self-host-parity:builtin-signature] readiness probe returned an unexpected artifact" >&2
    cat "$BUILD_DIR/main_c_leg.out" >&2
    exit 1
}

echo "[self-host-parity:builtin-signature] C/LLVM readiness parity ok"
