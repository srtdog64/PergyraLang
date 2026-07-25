#!/usr/bin/env bash
# CLOSED initializer verdict fallbacks:
# semantic_normalization_char_at
# semantic_normalization_trim_copy
# semantic_validation_trim_copy
# Focused C/LLVM proof for semantic initializer type projection into MIR.
# Registry ratchets: source_initializer_type_rescan and
# backend_initializer_type_guess are forbidden in every last consumer.

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

LABEL="self-host-parity:initializer-projection"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/initializer_projection}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/expected.txt"
DIRECT_CALL_EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/direct_call_expected.txt"
NOMINAL_CALL_EXPECTED="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/nominal_call_expected.txt"
ARTIFACT_LOWER="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
ROUTINE_LOWER="$ROOT_DIR/src/self_hosted/mir/routine_lower_owner.pgy"
ROUTINE_LET="$ROOT_DIR/src/self_hosted/mir/routine_let_owner.pgy"
ROUTINE_INPUT="$ROOT_DIR/src/self_hosted/mir/routine_input_owner.pgy"
BODY_TYPE_BUNDLE="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
EXPRESSION_VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
CONCRETE_SCALAR_VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy"
RESOLVED_CALL_TYPE="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy"
CALL_TARGET_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy"
CALL_TARGET_CAPTURE="$ROOT_DIR/src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy"
MEMBER_VIEW_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_member_view_owner.pgy"
RECEIVER_TYPE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_receiver_type_owner.pgy"
SCALAR_TYPE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_scalar_type_owner.pgy"
RETIRED_DIRECT_CALL_TYPE="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_direct_call_type_owner.pgy"
RETIRED_STATIC_CALL_TYPE="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_static_call_type_owner.pgy"
EXPRESSION_SURFACE="$ROOT_DIR/src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy"
EXPRESSION_GRAPH_BUILD="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_build_owner.pgy"
EXPRESSION_ENV="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"
EXPR_TYPE_OWNER="$ROOT_DIR/src/self_hosted/semantic/expr_type_owner.pgy"
SEMANTIC_CALL_EMIT="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"
INITIALIZER_CONSUMERS=(
    "$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
)
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$DIRECT_CALL_EXPECTED" \
    "$NOMINAL_CALL_EXPECTED" \
    "$ARTIFACT_LOWER" "$ROUTINE_LOWER" \
    "$ROUTINE_LET" "$ROUTINE_INPUT" "$BODY_TYPE_BUNDLE" \
    "$EXPRESSION_VERDICT" "$CONCRETE_SCALAR_VERDICT" \
    "$RESOLVED_CALL_TYPE" "$CALL_TARGET_OWNER" "$CALL_TARGET_CAPTURE" \
    "$MEMBER_VIEW_OWNER" \
    "$RECEIVER_TYPE_OWNER" \
    "$SCALAR_TYPE_OWNER" "$EXPRESSION_SURFACE" "$EXPRESSION_GRAPH_BUILD" \
    "$EXPRESSION_ENV" \
    "$EXPR_TYPE_OWNER" \
    "$SEMANTIC_CALL_EMIT"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done
grep -Fq 'import "expression_normalization_owner.pgy";' "$EXPR_TYPE_OWNER" ||
    { echo "[$LABEL] expression type owner bypasses normalization SoT" >&2; exit 1; }
if grep -Fq 'Trim(' "$EXPR_TYPE_OWNER"; then
    echo "[$LABEL] expression type owner retains allocation-returning trim" >&2
    exit 1
fi
[[ ! -e "$RETIRED_DIRECT_CALL_TYPE" ]] ||
    { echo "[$LABEL] retired direct-call type owner returned" >&2; exit 1; }
[[ ! -e "$RETIRED_STATIC_CALL_TYPE" ]] ||
    { echo "[$LABEL] retired static-call type owner returned" >&2; exit 1; }
for consumer in "${INITIALIZER_CONSUMERS[@]}"; do
    if grep -Fq 'SemanticAstInitializerTypeFactsMatchArtifact(' "$consumer"; then
        echo "[$LABEL] consumer rebuilt initializer type facts: $consumer" >&2
        exit 1
    fi
done
if grep -Fq 'bounded MIR producer requires a local type fact' "$ROUTINE_LOWER"; then
    echo "[$LABEL] declared-type-only MIR fallback returned" >&2
    exit 1
fi
grep -Fq 'input.initializers.inferred_type_names' "$ROUTINE_LET" ||
    { echo "[$LABEL] MIR let does not consume initializer rows" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphConcreteScalarValueOwned(' \
    "$EXPRESSION_VERDICT" ||
    { echo "[$LABEL] concrete scalar result type is not graph-owned" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphScalarOperatorError(' \
    "$CONCRETE_SCALAR_VERDICT" ||
    { echo "[$LABEL] scalar operator validation is not graph-owned" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphResolvedCallReturnTypeName(' "$EXPRESSION_VERDICT" ||
    { echo "[$LABEL] resolved call return is not graph-owned" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphResolvedCallConcreteScalarTypeName(' \
    "$CONCRETE_SCALAR_VERDICT" ||
    { echo "[$LABEL] concrete scalar call capability is not explicit" >&2; exit 1; }
if grep -Fq 'SemanticExpressionGraphResolvedCallTypeName' \
    "$EXPRESSION_VERDICT" "$CONCRETE_SCALAR_VERDICT" \
    "$RESOLVED_CALL_TYPE" "$SCALAR_TYPE_OWNER"; then
    echo "[$LABEL] ambiguous resolved-call type owner returned" >&2
    exit 1
fi
grep -Fq 'SemanticExpressionGraphResolvedCallReturnTypeName(' \
    "$SCALAR_TYPE_OWNER" ||
    { echo "[$LABEL] scalar graph rebuilt a call return type" >&2; exit 1; }
if grep -Fq 'function_returns[UnwrapOption(target)]' "$SCALAR_TYPE_OWNER"; then
    echo "[$LABEL] scalar graph bypassed the resolved-call return owner" >&2
    exit 1
fi
grep -Fq 'SemanticExpressionGraphConcreteScalarValueError(' \
    "$EXPRESSION_VERDICT" ||
    { echo "[$LABEL] concrete scalar verdict is not graph-owned" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphConcreteScalarValueOwned(' \
    "$CONCRETE_SCALAR_VERDICT" ||
    { echo "[$LABEL] concrete scalar graph capability is missing" >&2; exit 1; }
if grep -Eq 'DirectCallLeaf|DirectCallScalarArgumentsOwned|DirectCallScalarArgumentError|DirectCallScalarValueOwned' \
    "$EXPRESSION_VERDICT" "$CONCRETE_SCALAR_VERDICT"; then
    echo "[$LABEL] obsolete direct-call-only verdict owner returned" >&2
    exit 1
fi
grep -Fq 'SemanticExpressionGraphResolvedCallTarget(' \
    "$RESOLVED_CALL_TYPE" "$CONCRETE_SCALAR_VERDICT" "$SCALAR_TYPE_OWNER" ||
    { echo "[$LABEL] call consumers ignore the semantic target owner" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphCallTargetName(' "$CALL_TARGET_OWNER" ||
    { echo "[$LABEL] call target owner is missing" >&2; exit 1; }
grep -Fq 'SemanticCallTargetMember()' "$CALL_TARGET_OWNER" ||
    { echo "[$LABEL] member-call target kind is missing" >&2; exit 1; }
grep -Fq 'SemanticCallTargetDirect()' "$CALL_TARGET_OWNER" ||
    { echo "[$LABEL] direct-call target kind is missing" >&2; exit 1; }
grep -Fq 'SemanticCallTargetDirect()' "$CALL_TARGET_CAPTURE" ||
    { echo "[$LABEL] direct-call signature capture is missing" >&2; exit 1; }
grep -Fq 'target_fact.parameter_offset' "$CONCRETE_SCALAR_VERDICT" ||
    { echo "[$LABEL] member-call self parameter offset is ignored" >&2; exit 1; }
grep -Fq 'SemanticCallableCanonicalDeclaredName(' "$EXPRESSION_ENV" ||
    { echo "[$LABEL] callable table discards declaration ownership" >&2; exit 1; }
grep -Fq 'SemanticAstAnalysisResolveCallTargetsFromBody(' \
    "$BODY_TYPE_BUNDLE" ||
    { echo "[$LABEL] body fixpoint does not carry resolved call targets" >&2; exit 1; }
grep -Fq 'inout analysis: SemanticAstArtifactAnalysis' \
    "$BODY_TYPE_BUNDLE" ||
    { echo "[$LABEL] body fixpoint is not an analysis state transformer" >&2; exit 1; }
grep -Fq 'input.analysis.expression_surfaces' "$ROUTINE_LET" ||
    { echo "[$LABEL] MIR does not consume the resolved analysis graph" >&2; exit 1; }
grep -Fq 'SemanticCallTargetMember()' "$SEMANTIC_CALL_EMIT" ||
    { echo "[$LABEL] codegen ignores carried member targets" >&2; exit 1; }
grep -Fq 'analysis, "Box_Get", "box"' "$SOURCE" ||
    { echo "[$LABEL] executable probe does not reach hard codegen" >&2; exit 1; }
grep -Fq 'SemanticCanonicalTypeNameFactFrom(' "$CALL_TARGET_OWNER" ||
    { echo "[$LABEL] generic receiver target ignores canonical type owner" >&2; exit 1; }
grep -Fq 'SemanticAstNominalFieldType(' "$RECEIVER_TYPE_OWNER" ||
    { echo "[$LABEL] chained receiver ignores nominal field facts" >&2; exit 1; }
if grep -Eq 'SemanticProjectionExpressionType|LookupFieldType|ExprMemberFieldType' \
    "$CALL_TARGET_OWNER" "$RECEIVER_TYPE_OWNER"; then
    echo "[$LABEL] chained receiver reopened text/backend type recovery" >&2
    exit 1
fi
if grep -Fq 'CompilerSymbolCQualifiedName(' "$SEMANTIC_CALL_EMIT"; then
    echo "[$LABEL] codegen rebuilt member target identity" >&2
    exit 1
fi
# Registry fallback IDs: callee_text_as_final_identity,
# namespace_name_join_fallback, codegen_receiver_type_method_join.
if grep -Fq 'callee_text' "$SEMANTIC_CALL_EMIT"; then
    echo "[$LABEL] hard codegen recovered direct identity from callee text" >&2
    exit 1
fi
grep -Fq 'ast_expression_graph_member_view_owner.pgy' "$SEMANTIC_CALL_EMIT" ||
    { echo "[$LABEL] codegen does not consume the semantic member view" >&2; exit 1; }
if grep -Fq 'struct SemanticMemberAccessView' "$SEMANTIC_CALL_EMIT"; then
    echo "[$LABEL] codegen-local member view owner returned" >&2
    exit 1
fi
if grep -Eq 'DirectCallTypeName|DirectCallConcreteScalarType' \
    "$EXPRESSION_VERDICT" "$CONCRETE_SCALAR_VERDICT" "$RESOLVED_CALL_TYPE"; then
    echo "[$LABEL] obsolete direct-call-only type owner returned" >&2
    exit 1
fi
if grep -Eq 'SemanticExpressionGraphStaticCall(TypeName|TargetName)' \
    "$EXPRESSION_VERDICT" "$CONCRETE_SCALAR_VERDICT" \
    "$RESOLVED_CALL_TYPE" "$SCALAR_TYPE_OWNER"; then
    echo "[$LABEL] obsolete static-call-only owner returned" >&2
    exit 1
fi
grep -Fq 'SemanticAstExpressionGraphRootSpellingMatches(' \
    "$EXPRESSION_SURFACE" ||
    { echo "[$LABEL] parser-canonical root spelling verifier is missing" >&2; exit 1; }
grep -Fq 'SemanticExpressionGraphBuildCompactBridgeFromText(' \
    "$EXPRESSION_SURFACE" ||
    { echo "[$LABEL] root spelling verifier bypasses the parser owner" >&2; exit 1; }
grep -Fq 'func SemanticExpressionGraphAppendLaneRows(' \
    "$EXPRESSION_GRAPH_BUILD" ||
    { echo "[$LABEL] expression graph builder is not row-carried" >&2; exit 1; }
if grep -Eq 'inout (arena: SemanticExpressionGraphArena|facts: SemanticExpressionGraphFacts)' \
    "$EXPRESSION_GRAPH_BUILD" "$EXPRESSION_SURFACE"; then
    echo "[$LABEL] large expression graph aggregate crossed an inout boundary" >&2
    exit 1
fi

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    rm -f "$bin" "$BUILD_DIR/probe_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend="$backend" \
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
    local raw="$BUILD_DIR/probe_${backend}.raw"
    local out="$BUILD_DIR/probe_${backend}.out"
    (cd "$ROOT_DIR" && "$bin" >"$raw")
    pgy_selfhost_normalize_text_artifact <"$raw" >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"

    local direct_raw="$BUILD_DIR/probe_${backend}.direct_call.raw"
    local direct_out="$BUILD_DIR/probe_${backend}.direct_call.out"
    (cd "$ROOT_DIR" && "$bin" --direct-call-positive >"$direct_raw")
    pgy_selfhost_normalize_text_artifact <"$direct_raw" >"$direct_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$direct_out" \
        "run_output"

    local nested_raw="$BUILD_DIR/probe_${backend}.direct_call_nested.raw"
    local nested_out="$BUILD_DIR/probe_${backend}.direct_call_nested.out"
    (cd "$ROOT_DIR" && "$bin" --direct-call-nested-positive >"$nested_raw")
    pgy_selfhost_normalize_text_artifact <"$nested_raw" >"$nested_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$nested_out" \
        "run_output"

    local nested_call_raw="$BUILD_DIR/probe_${backend}.direct_call_nested_call.raw"
    local nested_call_out="$BUILD_DIR/probe_${backend}.direct_call_nested_call.out"
    (cd "$ROOT_DIR" && "$bin" --direct-call-nested-call-positive \
        >"$nested_call_raw")
    pgy_selfhost_normalize_text_artifact <"$nested_call_raw" \
        >"$nested_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$nested_call_out" \
        "run_output"

    local scalar_call_raw="$BUILD_DIR/probe_${backend}.scalar_call.raw"
    local scalar_call_out="$BUILD_DIR/probe_${backend}.scalar_call.out"
    (cd "$ROOT_DIR" && "$bin" --scalar-call-positive >"$scalar_call_raw")
    pgy_selfhost_normalize_text_artifact <"$scalar_call_raw" \
        >"$scalar_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$scalar_call_out" \
        "run_output"

    local namespace_raw="$BUILD_DIR/probe_${backend}.namespace_call.raw"
    local namespace_out="$BUILD_DIR/probe_${backend}.namespace_call.out"
    (cd "$ROOT_DIR" && "$bin" --namespace-call-positive >"$namespace_raw")
    pgy_selfhost_normalize_text_artifact <"$namespace_raw" \
        >"$namespace_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$namespace_out" \
        "run_output"

    local member_raw="$BUILD_DIR/probe_${backend}.member_call.raw"
    local member_out="$BUILD_DIR/probe_${backend}.member_call.out"
    (cd "$ROOT_DIR" && "$bin" --member-call-positive >"$member_raw")
    pgy_selfhost_normalize_text_artifact <"$member_raw" >"$member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" "$member_out" \
        "run_output"

    local generic_member_raw="$BUILD_DIR/probe_${backend}.generic_member_call.raw"
    local generic_member_out="$BUILD_DIR/probe_${backend}.generic_member_call.out"
    (cd "$ROOT_DIR" && "$bin" --generic-member-call-positive \
        >"$generic_member_raw")
    pgy_selfhost_normalize_text_artifact <"$generic_member_raw" \
        >"$generic_member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" \
        "$generic_member_out" "run_output"

    local chained_member_raw="$BUILD_DIR/probe_${backend}.chained_member_call.raw"
    local chained_member_out="$BUILD_DIR/probe_${backend}.chained_member_call.out"
    (cd "$ROOT_DIR" && "$bin" --chained-member-call-positive \
        >"$chained_member_raw")
    pgy_selfhost_normalize_text_artifact <"$chained_member_raw" \
        >"$chained_member_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$DIRECT_CALL_EXPECTED" \
        "$chained_member_out" "run_output"

    local nominal_call_raw="$BUILD_DIR/probe_${backend}.nominal_call.raw"
    local nominal_call_out="$BUILD_DIR/probe_${backend}.nominal_call.out"
    (cd "$ROOT_DIR" && "$bin" --nominal-return-call-positive \
        >"$nominal_call_raw")
    pgy_selfhost_normalize_text_artifact <"$nominal_call_raw" \
        >"$nominal_call_out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$NOMINAL_CALL_EXPECTED" \
        "$nominal_call_out" "run_output"

    local cursor_case cursor_mode cursor_expected cursor_raw cursor_out
    for cursor_case in \
        'outer_shadow|--cursor-outer-shadow-positive|cursor-outer-shadow=Int' \
        'nested_exit|--cursor-nested-exit-positive|cursor-nested-exit=Int' \
        'destructure|--cursor-destructure-atomic-positive|cursor-destructure=atomic'; do
        cursor_mode="${cursor_case#*|}"
        cursor_mode="${cursor_mode%%|*}"
        cursor_expected="${cursor_case##*|}"
        cursor_raw="$BUILD_DIR/probe_${backend}.cursor_${cursor_case%%|*}.raw"
        cursor_out="$BUILD_DIR/probe_${backend}.cursor_${cursor_case%%|*}.out"
        (cd "$ROOT_DIR" && "$bin" "$cursor_mode" >"$cursor_raw")
        pgy_selfhost_normalize_text_artifact <"$cursor_raw" >"$cursor_out"
        grep -Fxq "$cursor_expected" "$cursor_out" || {
            echo "[$LABEL] backend=$backend $cursor_mode output drifted" >&2
            cat "$cursor_out" >&2
            exit 1
        }
    done

    local mode rc diagnostic
    for mode in missing-initializer-row missing-inferred-type \
        unknown-scalar-operand scalar-type-mismatch \
        direct-call-callee-mismatch direct-call-argument-mismatch \
        direct-call-nested-mismatch direct-call-nested-call-mismatch \
        scalar-call-mismatch namespace-call-target-mismatch \
        member-call-target-mismatch nominal-return-call-target-mismatch \
        missing-carried-member-target \
        missing-carried-direct-target \
        missing-carried-generic-member-target \
        missing-carried-chained-member-target \
        cursor-self-reference cursor-sibling-leak; do
        diagnostic='matching initializer and iteration type facts'
        if [[ "$mode" == "unknown-scalar-operand" ]]; then
            diagnostic='undefined_symbol'
        elif [[ "$mode" == "scalar-type-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "direct-call-callee-mismatch" ]]; then
            diagnostic='let_type_mismatch'
        elif [[ "$mode" == "direct-call-argument-mismatch" ]]; then
            diagnostic='call_arg_type_mismatch'
        elif [[ "$mode" == "direct-call-nested-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "direct-call-nested-call-mismatch" ]]; then
            diagnostic='call_arg_type_mismatch'
        elif [[ "$mode" == "scalar-call-mismatch" ]]; then
            diagnostic='binop_type_mismatch'
        elif [[ "$mode" == "namespace-call-target-mismatch" ]]; then
            diagnostic='call_target_unresolved'
        elif [[ "$mode" == "member-call-target-mismatch" ]]; then
            diagnostic='let_type_mismatch'
        elif [[ "$mode" == "nominal-return-call-target-mismatch" ]]; then
            diagnostic='call_target_unresolved'
        elif [[ "$mode" == "missing-carried-direct-target" || \
                "$mode" == "missing-carried-member-target" || \
                "$mode" == "missing-carried-generic-member-target" || \
                "$mode" == "missing-carried-chained-member-target" ]]; then
            diagnostic='matching initializer and iteration type facts'
        elif [[ "$mode" == "cursor-self-reference" || \
                "$mode" == "cursor-sibling-leak" ]]; then
            diagnostic='undefined_symbol'
        fi
        set +e
        (cd "$ROOT_DIR" && "$bin" "--$mode" \
            >"$BUILD_DIR/$backend.$mode.out" \
            2>"$BUILD_DIR/$backend.$mode.err")
        rc=$?
        set -e
        [[ "$rc" -ne 0 ]] ||
            { echo "[$LABEL] backend=$backend $mode did not fail closed" >&2; exit 1; }
        grep -Fq "$diagnostic" \
            "$BUILD_DIR/$backend.$mode.out" "$BUILD_DIR/$backend.$mode.err" ||
            { echo "[$LABEL] backend=$backend $mode diagnostic drifted" >&2; exit 1; }
    done
}

compile_probe c
run_probe c

BACKENDS="${PGY_SELFHOST_INITIALIZER_PROJECTION_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    set +e
    compile_probe llvm
    llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -eq 0 ]]; then
        run_probe llvm
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.out" "$BUILD_DIR/probe_llvm.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call.out" \
            "$BUILD_DIR/probe_llvm.direct_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call_nested.out" \
            "$BUILD_DIR/probe_llvm.direct_call_nested.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.direct_call_nested_call.out" \
            "$BUILD_DIR/probe_llvm.direct_call_nested_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.scalar_call.out" \
            "$BUILD_DIR/probe_llvm.scalar_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.namespace_call.out" \
            "$BUILD_DIR/probe_llvm.namespace_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.member_call.out" \
            "$BUILD_DIR/probe_llvm.member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.generic_member_call.out" \
            "$BUILD_DIR/probe_llvm.generic_member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.chained_member_call.out" \
            "$BUILD_DIR/probe_llvm.chained_member_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.nominal_call.out" \
            "$BUILD_DIR/probe_llvm.nominal_call.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.cursor_outer_shadow.out" \
            "$BUILD_DIR/probe_llvm.cursor_outer_shadow.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.cursor_nested_exit.out" \
            "$BUILD_DIR/probe_llvm.cursor_nested_exit.out"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.cursor_destructure.out" \
            "$BUILD_DIR/probe_llvm.cursor_destructure.out"
    elif [[ "$llvm_rc" -eq 2 ]]; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        exit "$llvm_rc"
    fi
fi

echo "[$LABEL] semantic initializer projection parity ok"
