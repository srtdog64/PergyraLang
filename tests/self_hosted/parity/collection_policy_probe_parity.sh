#!/usr/bin/env bash
# Focused C-oracle/C/LLVM proof for owner-directed collection mutation policy.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:collection-policy"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/collection_policy}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/collection_policy_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/collection_policy_probe/expected.txt"
BAD_EXPECTED="$ROOT_DIR/src/self_hosted/tools/collection_policy_probe/bad_value_param_expected.txt"
VALID="$ROOT_DIR/src/self_hosted/tools/collection_policy_probe/valid.pgy"
BAD="$ROOT_DIR/src/self_hosted/tools/collection_policy_probe/bad_value_param.pgy"
POLICY_OWNER="$ROOT_DIR/src/self_hosted/semantic/collection_mutation_policy_owner.pgy"
GRAPH_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_collection_mutation_owner.pgy"
STATEMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
CALL_OWNER="$ROOT_DIR/src/self_hosted/semantic/call_check_owner.pgy"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$BAD_EXPECTED" "$VALID" "$BAD" \
    "$POLICY_OWNER" "$GRAPH_OWNER" "$STATEMENT_OWNER" "$CALL_OWNER"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

grep -Fq 'SemanticAstArtifactAnalyzeTyped(' "$SOURCE" ||
    { echo "[$LABEL] probe bypasses typed artifact analysis" >&2; exit 1; }
grep -Fq 'SemanticCollectionMutationError(' "$STATEMENT_OWNER" ||
    { echo "[$LABEL] specialized statement ignores the policy owner" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphCollectionMutationFactFromGraph(' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy" ||
    { echo "[$LABEL] graph verdict ignores collection policy facts" >&2; exit 1; }
grep -Fq 'collection_call_target' "$GRAPH_OWNER" ||
    { echo "[$LABEL] graph owner accepts missing target identity" >&2; exit 1; }
if grep -Eq 'ExprType\(|CheckCall\(|FirstArg' "$GRAPH_OWNER"; then
    echo "[$LABEL] graph owner reconstructs collection policy from source" >&2
    exit 1
fi
grep -Fq 'false, false' "$CALL_OWNER" ||
    { echo "[$LABEL] graph call checker still replays source collection policy" >&2; exit 1; }

oracle_path() {
    pgy_path_for_compiler "$PGY" "$1"
}

oracle_expect() {
    local source="$1"
    local status="$2"
    local code="$3"
    local stem="$4"
    local out="$BUILD_DIR/oracle_${stem}.out"
    local exe="$BUILD_DIR/oracle_${stem}.exe"
    local rc=0
    (cd "$ROOT_DIR" && "$PGY" "$(oracle_path "$source")" --native-pipeline --backend=c \
        --error-format=json -o "$(oracle_path "$exe")") >"$out" 2>&1 || rc=$?
    if [[ "$status" == "ok" ]]; then
        [[ "$rc" -eq 0 ]] || {
            echo "[$LABEL] C oracle rejected $stem" >&2
            cat "$out" >&2
            exit 1
        }
        return
    fi
    [[ "$rc" -ne 0 ]] || { echo "[$LABEL] C oracle accepted $stem" >&2; exit 1; }
    grep -Fq "$code" "$out" || {
        echo "[$LABEL] C oracle diagnostic drift for $stem" >&2
        cat "$out" >&2
        exit 1
    }
}

oracle_expect "$VALID" ok "" valid
oracle_expect "$BAD" error PGY_SEM_BUILTIN_ARGS_INVALID bad_value_param

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    rm -f "$bin" "$BUILD_DIR/probe_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(oracle_path "$SOURCE")" \
        --backend="$backend" -o "$(oracle_path "$bin")" >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}.out"
    local bad_out="$BUILD_DIR/probe_${backend}_bad.out"
    local rc=0
    (cd "$ROOT_DIR" && "$bin") >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
    (cd "$ROOT_DIR" && "$bin" --bad-value-param) >"$bad_out" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend bad-value-param exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$BAD_EXPECTED" "$bad_out" "run_output"
}

BACKENDS="${PGY_SELFHOST_COLLECTION_POLICY_BACKENDS:-c llvm}"
ran=0
for backend in $BACKENDS; do
    if compile_probe "$backend"; then
        run_probe "$backend"
        ran=$((ran + 1))
    else
        echo "[$LABEL] backend=$backend skipped (LLVM unavailable)"
    fi
done
[[ "$ran" -gt 0 ]] || { echo "[$LABEL] no backend ran" >&2; exit 1; }
echo "[$LABEL] C oracle and owner-directed collection policy parity ok"
