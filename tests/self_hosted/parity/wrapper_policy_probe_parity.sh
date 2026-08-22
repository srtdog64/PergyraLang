#!/usr/bin/env bash
# Focused C-oracle/C/LLVM proof for graph-owned Option/Result builtin policy.

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

LABEL="self-host-parity:wrapper-policy"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/wrapper_policy}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/expected.txt"
BAD_OPTION_EXPECTED="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/bad_option_expected.txt"
BAD_UNWRAP_EXPECTED="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/bad_unwrap_expected.txt"
VALID_OPTION="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/valid.pgy"
VALID_RESULT="$VALID_OPTION"
BAD_OPTION="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/bad_option.pgy"
BAD_UNWRAP="$ROOT_DIR/src/self_hosted/tools/wrapper_policy_probe/bad_unwrap.pgy"
WRAPPER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_wrapper_value_owner.pgy"
TYPE_OWNER="$ROOT_DIR/src/self_hosted/semantic/wrapper_type_owner.pgy"
VERDICT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$BAD_OPTION_EXPECTED" \
    "$BAD_UNWRAP_EXPECTED" "$VALID_OPTION" "$VALID_RESULT" \
    "$BAD_OPTION" "$BAD_UNWRAP" "$WRAPPER_OWNER" "$TYPE_OWNER" \
    "$VERDICT_OWNER"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

grep -Fq 'SemanticAstArtifactAnalyzeTyped(' "$SOURCE" ||
    { echo "[$LABEL] probe bypasses typed artifact analysis" >&2; exit 1; }
if grep -Fq 'AnalyzeCompactBridge' "$SOURCE"; then
    echo "[$LABEL] probe reopens the compact expression bridge" >&2
    exit 1
fi
grep -Fq 'SemanticExpressionGraphWrapperValueFactFromGraph(' "$VERDICT_OWNER" ||
    { echo "[$LABEL] final verdict ignores wrapper facts" >&2; exit 1; }
grep -Fq 'wrapper_call_target' "$WRAPPER_OWNER" ||
    { echo "[$LABEL] wrapper owner accepts missing target identity" >&2; exit 1; }
if grep -Eq 'ExprType\(|CheckCall\(' "$WRAPPER_OWNER"; then
    echo "[$LABEL] wrapper owner reconstructs policy from source text" >&2
    exit 1
fi
grep -Fq 'func OptionPayloadTypeOpt(' "$TYPE_OWNER" ||
    { echo "[$LABEL] Option payload policy has no canonical owner" >&2; exit 1; }
grep -Fq 'func ResultPayloadTypeOpt(' "$TYPE_OWNER" ||
    { echo "[$LABEL] Result payload policy has no canonical owner" >&2; exit 1; }

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

oracle_expect "$VALID_OPTION" ok "" valid_option
oracle_expect "$VALID_RESULT" ok "" valid_result
oracle_expect "$BAD_OPTION" error PGY_C_TYPE_UNSUPPORTED bad_option
oracle_expect "$BAD_UNWRAP" error PGY_SEM_BUILTIN_ARGS_INVALID bad_unwrap

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

run_negative() {
    local backend="$1"
    local mode="$2"
    local expected="$3"
    local label="$4"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}_${label}.out"
    local rc=0
    (cd "$ROOT_DIR" && "$bin" "$mode") >"$out" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend $label exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$expected" "$out" "run_output"
}

run_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}.out"
    (cd "$ROOT_DIR" && "$bin") >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
    run_negative "$backend" --bad-option "$BAD_OPTION_EXPECTED" bad_option
    run_negative "$backend" --bad-unwrap "$BAD_UNWRAP_EXPECTED" bad_unwrap
}

BACKENDS="${PGY_SELFHOST_WRAPPER_POLICY_BACKENDS:-c llvm}"
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
echo "[$LABEL] C oracle and graph-owned Option/Result policy parity ok"
