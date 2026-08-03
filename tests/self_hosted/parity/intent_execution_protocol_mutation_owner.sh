#!/usr/bin/env bash
set -euo pipefail

# Orchestration only.  Static once-validation policy, canonical wire/corpus
# construction, and executable admission each have one component owner below;
# their line budgets keep those responsibilities from folding back together.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-parity:intent-execution-protocol] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "intent-execution-protocol" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_EXECUTION_PROTOCOL_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_execution_protocol}"
VALID="$BUILD_DIR/canonical.mir.json"
MULTI_FIXTURE="$BUILD_DIR/multi_routine_intent.pgy"
MULTI_VALID="$BUILD_DIR/multi_routine.mir.json"
MULTI_INTERLEAVED="$BUILD_DIR/multi_routine_interleaved.mir.json"
JOIN_MAP="$BUILD_DIR/intent_execution_join_map.tsv"
MANIFEST="$BUILD_DIR/mutation_manifest.tsv"
STATIC_OWNER="$ROOT_DIR/tests/self_hosted/parity/intent_execution_protocol_static_owner.py"
CORPUS_OWNER="$ROOT_DIR/tests/self_hosted/parity/intent_execution_protocol_corpus_owner.py"
ADMISSION_RUNNER="$ROOT_DIR/tests/self_hosted/parity/intent_execution_protocol_admission_runner.sh"

component_line_budget() {
    local path="$1"
    local maximum="$2"
    local lines
    lines="$(wc -l <"$path")"
    [[ "$lines" -le "$maximum" ]] \
        || fail "component exceeds $maximum-line owner budget: $path ($lines)"
}
component_line_budget "${BASH_SOURCE[0]}" 120
component_line_budget "$STATIC_OWNER" 180
component_line_budget "$CORPUS_OWNER" 900
component_line_budget "$ADMISSION_RUNNER" 100

mkdir -p "$BUILD_DIR"
"$PYTHON_BIN" "$STATIC_OWNER" "$ROOT_DIR"
"$PYTHON_BIN" "$CORPUS_OWNER" generate-multi-source \
    "$FIXTURE" "$MULTI_FIXTURE"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
    2>"$BUILD_DIR/native.err" | tr -d '\r' >"$VALID") \
    || { cat "$BUILD_DIR/native.err" >&2; \
         fail "native canonical MIR production failed"; }
[[ -s "$VALID" ]] || fail "native canonical MIR JSON is empty"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$MULTI_FIXTURE")" \
    2>"$BUILD_DIR/multi_routine.native.err" | tr -d '\r' >"$MULTI_VALID") \
    || { cat "$BUILD_DIR/multi_routine.native.err" >&2; \
         fail "native multi-routine MIR production failed"; }
[[ -s "$MULTI_VALID" ]] || fail "native multi-routine MIR JSON is empty"

"$PYTHON_BIN" "$CORPUS_OWNER" build-corpus \
    "$VALID" "$MULTI_VALID" "$MULTI_INTERLEAVED" \
    "$BUILD_DIR" "$JOIN_MAP" "$MANIFEST"

MUTATION_COUNT="$(($(wc -l <"$MANIFEST") - 1))"
[[ "$MUTATION_COUNT" -ge 28 ]] \
    || fail "mutation corpus is incomplete: $MUTATION_COUNT"
[[ -s "$JOIN_MAP" ]] || fail "intent execution join map was not generated"

bash "$ADMISSION_RUNNER" "$ROOT_DIR" "$BUILD_DIR" "$VALID" \
    "$MULTI_VALID" "$MULTI_INTERLEAVED" "$MUTATION_COUNT"
