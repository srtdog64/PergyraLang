#!/usr/bin/env bash
# Focused C-oracle/C/LLVM proof for graph-owned aggregate field typing.

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

LABEL="self-host-parity:aggregate-field-policy"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/aggregate_field_policy}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/expected.txt"
BAD_EXPECTED="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/bad_expected.txt"
VALID="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/valid.pgy"
BAD="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/bad_field_type.pgy"
BAD_GENERIC="$ROOT_DIR/src/self_hosted/tools/aggregate_field_policy_probe/bad_generic_field_type.pgy"
FIELD_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_field_type_owner.pgy"
STRUCT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_struct_type_verdict_owner.pgy"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$BAD_EXPECTED" "$VALID" "$BAD" \
    "$BAD_GENERIC" \
    "$FIELD_OWNER" "$STRUCT_OWNER"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

grep -Fq 'SemanticAstArtifactAnalyzeTyped(' "$SOURCE" ||
    { echo "[$LABEL] probe bypasses typed artifact analysis" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphFieldValueTypeName(' "$STRUCT_OWNER" ||
    { echo "[$LABEL] struct verdict ignores graph field types" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphFieldValueAssignableTo(' "$STRUCT_OWNER" ||
    { echo "[$LABEL] struct verdict ignores graph assignability" >&2; exit 1; }
if grep -Eq 'ExprType\(|ExpressionAssignableTo\(' "$STRUCT_OWNER"; then
    echo "[$LABEL] struct verdict reconstructs field types from source text" >&2
    exit 1
fi
if grep -Eq 'ExprType\(|ExpressionAssignableTo\(' "$FIELD_OWNER"; then
    echo "[$LABEL] field owner reopens source type policy" >&2
    exit 1
fi
grep -Fq 'child-fact-missing=reject' "$SOURCE" ||
    { echo "[$LABEL] missing child-fact negative" >&2; exit 1; }
grep -Fq 'generic-leaf-type-drift=reject' "$SOURCE" ||
    { echo "[$LABEL] missing generic graph-drift negative" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphGenericCallFactFromGraph(' "$STRUCT_OWNER" ||
    { echo "[$LABEL] struct verdict ignores signature-owned generic facts" >&2; exit 1; }

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
    (cd "$ROOT_DIR" && "$PGY" "$(oracle_path "$source")" --backend=c \
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
oracle_expect "$BAD" error PGY_SEM_CLASS_CONTRACT_INVALID bad_field
oracle_expect "$BAD_GENERIC" error PGY_SEM_CLASS_CONTRACT_INVALID bad_generic_field

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
    local bad_generic_out="$BUILD_DIR/probe_${backend}_bad_generic.out"
    local rc=0
    (cd "$ROOT_DIR" && "$bin") >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
    (cd "$ROOT_DIR" && "$bin" --bad-field) >"$bad_out" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend bad-field exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$BAD_EXPECTED" "$bad_out" "run_output"
    rc=0
    (cd "$ROOT_DIR" && "$bin" --bad-generic-field) >"$bad_generic_out" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend bad-generic-field exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$BAD_EXPECTED" "$bad_generic_out" "run_output"
}

BACKENDS="${PGY_SELFHOST_AGGREGATE_FIELD_POLICY_BACKENDS:-c llvm}"
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
echo "[$LABEL] C oracle and graph-owned aggregate field policy parity ok"
