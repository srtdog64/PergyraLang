#!/usr/bin/env bash
set -euo pipefail

# Builds the production-like self admission executable, proves legacy MIR keeps
# a semantic-absent carrier, then delegates all present/multi/mutation protocol
# evidence to the one intent_execution protocol corpus owner.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-plan-admission] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-plan-admission" "$PGY" \
    || fail "PGY_BIN is not runnable"

BUILD_DIR="${PGY_SELFHOST_INTENT_PLAN_ADMISSION_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_execution_json_admission}"
PROBE_SOURCE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_execution_plan_json_admission_probe.pgy"
PROBE_BINARY="$BUILD_DIR/intent_execution_plan_json_admission_probe.exe"
LEGACY_SOURCE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
LEGACY_JSON="$BUILD_DIR/legacy.mir.json"
PROTOCOL_GATE="$ROOT_DIR/tests/self_hosted/parity/intent_execution_protocol_mutation_owner.sh"
mkdir -p "$BUILD_DIR"

for owner in \
    intent_execution_json_decode_owner.pgy \
    intent_execution_json_rows_owner.pgy \
    intent_execution_identity_index_owner.pgy \
    intent_execution_plan_fact_owner.pgy; do
    lines="$(wc -l < "$ROOT_DIR/src/self_hosted/mir_lower/$owner")"
    [[ "$lines" -le 600 ]] \
        || fail "$owner exceeded the 600-line component boundary: $lines"
done

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$PROBE_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PROBE_BINARY")" \
    >"$BUILD_DIR/probe.compile.log" 2>&1) \
    || { tail -c 65536 "$BUILD_DIR/probe.compile.log" >&2; fail "probe build failed"; }

(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$LEGACY_SOURCE")" \
    2>"$BUILD_DIR/legacy.native.err" | tr -d '\r' >"$LEGACY_JSON") \
    || { cat "$BUILD_DIR/legacy.native.err" >&2; fail "legacy MIR JSON failed"; }

(cd "$ROOT_DIR" && "$PROBE_BINARY" --verify-input \
    "${LEGACY_JSON#"$ROOT_DIR/"}" \
    >"$BUILD_DIR/legacy.admission.out" 2>"$BUILD_DIR/legacy.admission.err") \
    || { cat "$BUILD_DIR/legacy.admission.out" \
             "$BUILD_DIR/legacy.admission.err" >&2; \
         fail "legacy semantic-absent admission failed"; }
grep -Fq 'pgy.mir.v1 input verified' "$BUILD_DIR/legacy.admission.out" \
    || fail "legacy admission success marker is missing"
grep -Fq 'intent execution admission: absent' "$BUILD_DIR/legacy.admission.out" \
    || fail "legacy intent plan was not semantic absent"

PGY_BIN="$PGY" \
PGY_SELFHOST_INTENT_EXECUTION_ADMISSION_BIN="$PROBE_BINARY" \
    bash "$PROTOCOL_GATE"

echo "[self-host-intent-plan-admission] probe + semantic absent + unified protocol corpus: PASS"
