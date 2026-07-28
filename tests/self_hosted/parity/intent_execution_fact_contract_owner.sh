#!/usr/bin/env bash
set -euo pipefail

# Focused MIR fact-family gate.  Full typed intent runtime/parity remains owned
# by intent_typed_outcome_compensation_owner.sh.todo until the frontend, DIR,
# JSON carrier, and production consumer all reach this plan.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-execution-facts] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-execution-facts" "$PGY" \
    || fail "PGY_BIN is not runnable"

OWNER="$ROOT_DIR/src/self_hosted/mir/intent_execution_fact_owner.pgy"
PROBE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_execution_fact_contract_probe.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_EXECUTION_FACT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_execution_facts}"
BINARY="$BUILD_DIR/intent_execution_fact_contract_probe.exe"
mkdir -p "$BUILD_DIR"

for term in \
    'struct MirIntentStepTransitionFacts' \
    'struct MirIntentTerminalTransitionFacts' \
    'struct MirIntentExecutionPlan' \
    'MirIntentStepTransitionCycleFreeFrom' \
    'MirIntentTerminalSourceMatchesStep' \
    'MirIntentExecutionPlanDigest' \
    'MirIntentExecutionPlanReady'; do
    grep -Fq -- "$term" "$OWNER" \
        || fail "missing typed intent execution owner term: $term"
done

for forbidden in \
    'source_order' 'source order' 'step_index - 1' 'variant == "Ok"' \
    'variant == "Err"' 'success_variant_names[row] == "Success"' \
    'failure_variant_names[row] == "Failure"'; do
    if grep -Fq -- "$forbidden" "$OWNER"; then
        fail "intent execution owner reopened positional/spelling fallback: $forbidden"
    fi
done

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$PROBE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$BINARY")" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { tail -c 65536 "$BUILD_DIR/compile.log" >&2; fail "probe build failed"; }

(cd "$ROOT_DIR" && "$BINARY" \
    >"$BUILD_DIR/run.out" 2>"$BUILD_DIR/run.err") \
    || { cat "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" >&2; fail "probe failed"; }
grep -Fq 'intent execution fact contract: PASS' "$BUILD_DIR/run.out" \
    || fail "probe PASS marker is missing"

echo "[self-host-intent-execution-facts] typed step + terminal fact negatives: PASS"
