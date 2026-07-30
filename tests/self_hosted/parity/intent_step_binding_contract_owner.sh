#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-step-binding] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-step-binding" "$PGY" \
    || fail "PGY_BIN is not runnable"

OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/intent_step_binding_owner.pgy"
CONTRACT="$ROOT_DIR/src/self_hosted/codegen/emission/intent_step_binding_contract_owner.pgy"
PROBE="$ROOT_DIR/src/self_hosted/tools/intent_step_binding_contract/main.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_STEP_BINDING_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_step_binding}"
BINARY="$BUILD_DIR/intent_step_binding_contract.exe"
mkdir -p "$BUILD_DIR"

for term in \
    'where-using-zone-mismatch' \
    'authority-slot-not-declared' \
    'zone-subject-slot-ambiguous' \
    'zone-subject-slot-missing'; do
    grep -Fq -- "$term" "$OWNER" "$CONTRACT" \
        || fail "missing fail-closed binding case: $term"
done

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$PROBE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$BINARY")" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { tail -c 65536 "$BUILD_DIR/compile.log" >&2; fail "probe build failed"; }

(cd "$ROOT_DIR" && "$BINARY" \
    >"$BUILD_DIR/run.out" 2>"$BUILD_DIR/run.err") \
    || { cat "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" >&2; fail "probe failed"; }
grep -Fq 'intent step binding contract: PASS' "$BUILD_DIR/run.out" \
    || fail "probe PASS marker is missing"

echo "[self-host-intent-step-binding] distinct actor/authority plus where/value/inout/ambiguous/missing negatives: PASS"
