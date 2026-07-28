#!/usr/bin/env bash
set -euo pipefail

# Focused prerequisite for typed intent execution. The self-host DIR result
# contract must reach the MIR routine signature before any execution-plan JSON
# row can honestly cite that routine.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-result-signature] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-result-signature" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"

BUILD_DIR="${PGY_SELFHOST_INTENT_RESULT_SIGNATURE_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_result_signature}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-result-signature" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

TYPED_FIXTURE="tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
LEGACY_FIXTURE="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
TYPED_MIR="$BUILD_DIR/typed.mir.json"
LEGACY_MIR="$BUILD_DIR/legacy.mir.json"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$TYPED_FIXTURE" \
    >"$TYPED_MIR" 2>"$BUILD_DIR/typed.err") \
    || { cat "$TYPED_MIR" "$BUILD_DIR/typed.err" >&2; fail "typed MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$LEGACY_FIXTURE" \
    >"$LEGACY_MIR" 2>"$BUILD_DIR/legacy.err") \
    || { cat "$LEGACY_MIR" "$BUILD_DIR/legacy.err" >&2; fail "legacy MIR production failed"; }

"$PYTHON_BIN" - "$TYPED_MIR" "$LEGACY_MIR" <<'PY'
import json
from pathlib import Path
import sys

typed = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
legacy = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))

def one_intent(document, name):
    rows = [row for row in document["routines"]
            if row["kind"] == "intent" and row["name"] == name]
    assert len(rows) == 1, (name, rows)
    return rows[0]

typed_intent = one_intent(typed, "RunWorkflow")
legacy_intent = one_intent(legacy, "RunIntent")
assert typed_intent["return"] == "WorkflowOutcome", typed_intent["return"]
assert typed_intent["return"] not in ("Bool", "Void", "Unknown", "")
assert legacy_intent["return"] == "Bool", legacy_intent["return"]
PY

for invalid_type in Bool Void Unknown; do
    invalid_source="$BUILD_DIR/typed-return-${invalid_type}.pgy"
    sed "s/-> WorkflowOutcome/-> ${invalid_type}/" \
        "$ROOT_DIR/$TYPED_FIXTURE" >"$invalid_source"
    invalid_out="$BUILD_DIR/typed-return-${invalid_type}.out"
    invalid_err="$BUILD_DIR/typed-return-${invalid_type}.err"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "${invalid_source#"$ROOT_DIR/"}" >"$invalid_out" 2>"$invalid_err"); then
        fail "typed intent return ${invalid_type} was accepted"
    fi
    if grep -Fq '"schema":"pgy.mir.v1"' "$invalid_out"; then
        fail "typed intent return ${invalid_type} emitted a partial MIR artifact"
    fi
done

DIR_OWNER="$ROOT_DIR/src/self_hosted/dir/intent_result_contract_owner.pgy"
MIR_OWNER="$ROOT_DIR/src/self_hosted/mir/intent_routine_owner.pgy"
for term in \
    'result_modes: Array<String>' \
    'return_type_names: Array<String>' \
    'SelfDirIntentResultContractReady' \
    'typed && return_type == "Bool"' \
    '!typed && return_type != "Bool"'; do
    grep -Fq -- "$term" "$DIR_OWNER" \
        || fail "missing DIR result-carriage ratchet: $term"
done
grep -Fq 'ArrayPush(return_types, return_type_name)' "$MIR_OWNER" \
    || fail "MIR intent routine does not consume the DIR return type"
if grep -Fq 'ArrayPush(return_types, "Void")' "$MIR_OWNER"; then
    fail "MIR intent return-type Void fallback returned"
fi

echo "[self-host-intent-result-signature] typed enum + explicit legacy Bool carriage: PASS"
