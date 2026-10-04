#!/usr/bin/env bash
# Focused C/LLVM proof for exact and composite generic return substitution.

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

LABEL="self-host-parity:generic-return"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/generic_return}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/expected.txt"
CALLABLE_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/callable_resolution_expected.txt"
MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/mismatch_expected.txt"
NESTED_MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/nested_mismatch_expected.txt"
EXPLICIT_MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_mismatch_expected.txt"
EXPLICIT_OK="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_ok.pgy"
EXPLICIT_MISMATCH="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_mismatch.pgy"
POSTFIX_OWNER="$ROOT_DIR/src/self_hosted/parser/expr_postfix_owner.pgy"
CALL_VIEW_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy"
KIND_OWNER="$ROOT_DIR/src/self_hosted/hir/ast_node_kind_owner.pgy"
INVENTORY_OWNER="$ROOT_DIR/src/self_hosted/hir/ast_text_inventory_owner.pgy"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_signature_fact_owner.pgy"
GENERIC_ROW_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy"
NORMALIZATION_OWNER="$ROOT_DIR/src/self_hosted/semantic/expression_normalization_owner.pgy"
TYPE_CANONICAL_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_type_name_canonical_owner.pgy"
TYPE_EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy"
TYPE_BINDING_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_signature_type_binding_owner.pgy"
GENERIC_CALL_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy"
EXPRESSION_VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$CALLABLE_EXPECTED" "$MISMATCH_EXPECTED" \
    "$NESTED_MISMATCH_EXPECTED" "$KIND_OWNER" \
    "$EXPLICIT_MISMATCH_EXPECTED" "$EXPLICIT_OK" "$EXPLICIT_MISMATCH" \
    "$POSTFIX_OWNER" "$CALL_VIEW_OWNER" \
    "$INVENTORY_OWNER" "$SIGNATURE_OWNER" "$GENERIC_ROW_OWNER" \
    "$NORMALIZATION_OWNER" "$TYPE_CANONICAL_OWNER" "$TYPE_EXPRESSION_OWNER" \
    "$TYPE_BINDING_OWNER" \
    "$GENERIC_CALL_OWNER" \
    "$EXPRESSION_VERDICT"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

grep -Fq 'import "expression_normalization_owner.pgy";' "$GENERIC_ROW_OWNER" ||
    { echo "[$LABEL] generic parameter owner bypasses normalization SoT" >&2; exit 1; }
grep -Fq 'func SemanticTrimSourceRangeReuse(' "$NORMALIZATION_OWNER" ||
    { echo "[$LABEL] range trim owner is missing" >&2; exit 1; }
if grep -Fq 'StringTrim(Substring(' "$GENERIC_ROW_OWNER"; then
    echo "[$LABEL] generic default rows retain an unconditional trim copy" >&2
    exit 1
fi
grep -Fq 'import "expression_normalization_owner.pgy";' "$TYPE_CANONICAL_OWNER" ||
    { echo "[$LABEL] canonical type owner bypasses normalization SoT" >&2; exit 1; }
if grep -Fq 'Trim(' "$TYPE_CANONICAL_OWNER"; then
    echo "[$LABEL] canonical type owner retains allocation-returning trim" >&2
    exit 1
fi
grep -Fq 'import "expression_normalization_owner.pgy";' "$TYPE_EXPRESSION_OWNER" ||
    { echo "[$LABEL] signature type-expression owner bypasses normalization SoT" >&2; exit 1; }
if grep -Fq 'StringTrim(' "$TYPE_EXPRESSION_OWNER"; then
    echo "[$LABEL] signature type-expression owner retains allocation-returning trim" >&2
    exit 1
fi

grep -Fq 'TypedAstKindGenericParamsTag()' "$KIND_OWNER" "$INVENTORY_OWNER" ||
    { echo "[$LABEL] generic parameter row is not typed HIR" >&2; exit 1; }
grep -Fq 'generic_starts: Array<Int>' "$SIGNATURE_OWNER" ||
    { echo "[$LABEL] signature owner lacks formal generic rows" >&2; exit 1; }
grep -Fq 'SemanticAstGenericParameterFactRowsFromOwnerNode(' "$SIGNATURE_OWNER" ||
    { echo "[$LABEL] signature owner ignores atomic typed generic rows" >&2; exit 1; }
! grep -Fq 'SemanticAstGenericParameterRowsFromNode(' "$SIGNATURE_OWNER" ||
    { echo "[$LABEL] signature owner revived the retired split generic row path" >&2; exit 1; }
grep -Fq 'type_expressions: SemanticAstSignatureTypeExpressionFacts' \
    "$SIGNATURE_OWNER" ||
    { echo "[$LABEL] signature owner lacks unified type-expression facts" >&2; exit 1; }
grep -Fq 'SemanticAstSignatureReturnTypeResolveAt(' "$GENERIC_CALL_OWNER" ||
    { echo "[$LABEL] composite return ignores typed return facts" >&2; exit 1; }
grep -Fq 'SemanticAstSignatureParameterTypesBind(' "$GENERIC_CALL_OWNER" ||
    { echo "[$LABEL] batched nested parameter binding ignores typed parameter facts" >&2; exit 1; }
binding_body="$(sed -n '/^func SemanticAstSignatureParameterTypesBind(/,/^}/p' \
    "$TYPE_BINDING_OWNER")"
binding_signature="$(sed -n '1,/) -> Array<String> {/p' <<<"$binding_body")"
grep -Eq '^\) -> Array<String> \{[[:space:]]*$' <<<"$binding_signature" ||
    { echo "[$LABEL] named binding signature lacks an Array<String> result" >&2; exit 1; }
grep -Eq '^[[:space:]]*own parameter_indices: Array<Int>,[[:space:]]*$' \
    <<<"$binding_signature" ||
    { echo "[$LABEL] generic binding does not consume its index row" >&2; exit 1; }
for view in generic_names parameter_actual_types bindings; do
    grep -Eq "^[[:space:]]*$view: Slice<String>,?[[:space:]]*$" \
        <<<"$binding_signature" || {
        echo "[$LABEL] named binding signature lacks borrowed view: $view" >&2
        exit 1
    }
done
grep -Fq 'func SemanticAstSignatureTypeExpressionFormalIndex(' \
    "$TYPE_EXPRESSION_OWNER" ||
    { echo "[$LABEL] generic-name view owner is missing" >&2; exit 1; }
if grep -Eq 'own (generic_names|parameter_actual_types|bindings):' \
    <<<"$binding_signature"; then
    echo "[$LABEL] generic binding revived read-then-consume input authority" >&2
    exit 1
fi
# Structural inventory only; the C/LLVM oracle below owns executable parity.
binding_spine="$(tr -d '[:space:]' <<<"$binding_body")"
for term in \
    'letnext_bindings:Array<String>=[];' \
    'ArrayPushOwnedString(next_bindings,existing_binding);' \
    'ArrayPushOwnedString(next_bindings,update_type_names[update_row]);' \
    'ArrayDrop(parameter_indices);' \
    'returnnext_bindings;'; do
    grep -Fq "$term" <<<"$binding_spine" || {
        echo "[$LABEL] named binding owner lacks fresh-result materialization: $term" >&2
        exit 1
    }
done
if grep -Eq 'Array(Drop(OwnedStrings)?|Set|Push(OwnedString)?)\((generic_names|parameter_actual_types|bindings)[,)]' \
    <<<"$binding_spine"; then
    echo "[$LABEL] named binding owner retires or mutates a borrowed input" >&2
    exit 1
fi
if grep -Fq 'SemanticAstSignatureParameterTypeBindAt(' \
    "$TYPE_BINDING_OWNER" "$GENERIC_CALL_OWNER" "$SOURCE"; then
    echo "[$LABEL] live generic binding code revived the retired per-parameter wrapper" >&2
    exit 1
fi
while IFS='|' read -r caller name actual prior result; do
    caller_body="$(sed -n "/^func $name(/,/^}/p" "$caller")"
    call_spine="$(sed -n \
        '/^[[:space:]]*SemanticAstSignatureParameterTypesBind(/,/^[[:space:]]*);[[:space:]]*$/p' \
        <<<"$caller_body" | tr -d '[:space:]')"
    for view in 'signatures.generic_names' "$actual" "$prior"; do
        grep -Fq "$view.Slice(0,ArrayLength($view))" <<<"$call_spine" || {
            echo "[$LABEL] $name does not pass a call-bounded input view: $view" >&2
            exit 1
        }
    done
    awk -v actual="ArrayDropOwnedStrings($actual);" \
        -v prior="ArrayDropOwnedStrings($prior);" \
        -v result="ArrayDropOwnedStrings($result);" '
        index($0, "SemanticAstSignatureParameterTypesBind(") { in_call = 1 }
        in_call && /^[[:space:]]*\);[[:space:]]*$/ { returned = 1; in_call = 0 }
        index($0, actual) { if (!returned) bad = 1; actual_count++ }
        index($0, prior) { if (!returned) bad = 1; prior_count++ }
        index($0, result) {
            if (!returned || actual_count != 1 || prior_count != 1) bad = 1
            result_count++
        }
        END { exit (!bad && returned && actual_count == 1 &&
            prior_count == 1 && result_count == 1) ? 0 : 1 }
    ' <<<"$caller_body" || {
        echo "[$LABEL] $name lacks ordered caller-owned input/result retirement" >&2
        exit 1
    }
done <<EOF
$GENERIC_CALL_OWNER|SemanticExpressionGraphGenericCallFactCaptureFromGraph|binding_actual_type_names|bindings|resolved_bindings
$SOURCE|GenericReturnProbeNestedMismatchRejected|actual_types|bindings|after_binding
EOF
grep -Fq 'SemanticAstSignatureTypeBindingContractReady()' "$SOURCE" ||
    { echo "[$LABEL] binding contract is not executable" >&2; exit 1; }
if grep -Fq 'ArraySet(' "$TYPE_BINDING_OWNER"; then
    echo "[$LABEL] generic binding revived element replacement" >&2
    exit 1
fi
if grep -Fq 'inout bindings:' "$TYPE_BINDING_OWNER"; then
    echo "[$LABEL] generic binding revived borrowed mutation" >&2
    exit 1
fi
grep -Fq 'ParserExpressionGenericCalleeActual(' "$POSTFIX_OWNER" ||
    { echo "[$LABEL] parser drops explicit generic actuals" >&2; exit 1; }
grep -Fq 'generic_actual_type_names: Array<String>' "$CALL_VIEW_OWNER" ||
    { echo "[$LABEL] call view drops explicit generic actuals" >&2; exit 1; }
for term in \
    'ArraySet(reversed, i, reversed[last]);' \
    'ArraySet(reversed_actuals, i, reversed_actuals[last]);' \
    'true, node, base_callee, reversed, reversed_actuals'; do
    grep -Fq "$term" "$CALL_VIEW_OWNER" || {
        echo "[$LABEL] call view does not reuse its ordered spine backing: $term" >&2
        exit 1
    }
done
for retired_copy in \
    'let arguments: Array<Int> = [];' \
    'let actuals: Array<String> = [];'; do
    if grep -Fq "$retired_copy" "$CALL_VIEW_OWNER"; then
        echo "[$LABEL] call view rebuilt an ordered spine array: $retired_copy" >&2
        exit 1
    fi
done
grep -Fq 'call.generic_actual_type_names' "$GENERIC_CALL_OWNER" ||
    { echo "[$LABEL] generic consumer ignores explicit actual facts" >&2; exit 1; }
if grep -Eq 'String(IndexOf|Contains).*<[[:space:]]*' "$GENERIC_CALL_OWNER"; then
    echo "[$LABEL] generic consumer reconstructs actuals from call text" >&2
    exit 1
fi
grep -Fq 'SemanticExpressionGraphScalarTypeName(' "$GENERIC_CALL_OWNER" ||
    { echo "[$LABEL] generic binding ignores graph argument types" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphGenericCallFactFromGraph(' "$EXPRESSION_VERDICT" ||
    { echo "[$LABEL] expression verdict ignores generic call facts" >&2; exit 1; }
if grep -Fq 'ExprType(' "$GENERIC_CALL_OWNER"; then
    echo "[$LABEL] generic return was reconstructed from source text" >&2
    exit 1
fi

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    # This executable is parity-harness infrastructure. The public direct-MIR
    # LLVM route is a bounded replacement classifier and is covered by its own
    # replacement gates; it must not become the compiler for this source-level
    # C/LLVM semantic oracle pair.
    local native_subject=""
    [[ "$backend" == "llvm" ]] && native_subject="--native-pipeline"
    rm -f "$bin" "$BUILD_DIR/probe_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend="$backend" \
        $native_subject \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
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
    local mismatch="$BUILD_DIR/probe_${backend}.mismatch.out"
    local nested_mismatch="$BUILD_DIR/probe_${backend}.nested_mismatch.out"
    local explicit_mismatch="$BUILD_DIR/probe_${backend}.explicit_mismatch.out"
    local callable="$BUILD_DIR/probe_${backend}.callable.out"
    (cd "$ROOT_DIR" && "$bin") >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
    (cd "$ROOT_DIR" && "$bin" --callable-resolution) >"$callable"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$CALLABLE_EXPECTED" "$callable" \
        "run_output"
    local rc=0
    (cd "$ROOT_DIR" && "$bin" --target-mismatch) >"$mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$MISMATCH_EXPECTED" "$mismatch" \
        "run_output"
    rc=0
    (cd "$ROOT_DIR" && "$bin" --nested-mismatch) >"$nested_mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend nested mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$NESTED_MISMATCH_EXPECTED" \
        "$nested_mismatch" "run_output"
    rc=0
    (cd "$ROOT_DIR" && "$bin" --explicit-mismatch) >"$explicit_mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend explicit mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPLICIT_MISMATCH_EXPECTED" \
        "$explicit_mismatch" "run_output"
}

BACKENDS="${PGY_SELFHOST_GENERIC_RETURN_BACKENDS:-c llvm}"
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
echo "[$LABEL] inferred and explicit generic parameter/return parity ok"
