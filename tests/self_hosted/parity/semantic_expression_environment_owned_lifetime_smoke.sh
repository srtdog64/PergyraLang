#!/usr/bin/env bash
# Owns the compiler-semantic temporary String-array lifetime boundary.
# Expression environments borrow semantic facts; persistent function tables
# retain the explicit owned String-array pair.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"
OWNER_FIELDS="$ROOT_DIR/src/self_hosted/semantic/ast_expression_owner_field_environment_owner.pgy"
MATCH_BINDINGS="$ROOT_DIR/src/self_hosted/semantic/ast_match_binding_environment_owner.pgy"
ITERATION_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy"
ASSIGNMENT_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
CALL_TARGETS="$ROOT_DIR/src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy"
BODY_ENV="$ROOT_DIR/src/self_hosted/semantic/ast_body_expression_environment_owner.pgy"
BODY_TYPES="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
PLACE_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_expression_place_fact_owner.pgy"
GENERIC_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy"
INITIALIZER_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
INITIALIZER_REFINEMENT="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_iteration_refinement_owner.pgy"
INITIALIZER_TABLE_BRIDGE="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_function_table_bridge_owner.pgy"
INITIALIZER_CURSOR="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_environment_cursor_owner.pgy"
STATEMENT_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
BUILTINS="$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"
MIR_FACTS="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
DRIVER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
PIPELINE="$ROOT_DIR/src/self_hosted/compiler/driver_pipeline_owner.pgy"
VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_artifact_verdict_owner.pgy"
BODY_ADMISSION="$ROOT_DIR/src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy"
ENTRY="$ROOT_DIR/src/self_hosted/codegen/emission/program_entry_owner.pgy"
ADMITTED_ENTRY="$ROOT_DIR/src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy"
TYPECHECK="$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_array.c"
TRANS_POLICY="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
TRANS_EMIT="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
LLVM_EMIT="$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
LLVM_RUNTIME="$ROOT_DIR/src/codegen/llvm_runtime.c"
INLINE_RUNTIME="$ROOT_DIR/src/runtime/pgy_runtime_builtin_storage_inline.h"
EXPORT_RUNTIME="$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h"
CODEGEN_RUNTIME_CALLS="$ROOT_DIR/src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy"
CODEGEN_COLLECTION_RUNTIME="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy"
CODEGEN_CALL_EMITTER="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"
CODEGEN_PROGRAM_EMITTER="$ROOT_DIR/src/self_hosted/codegen/emission/program_emit.pgy"
CODEGEN_FUNCTION_EMITTER="$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy"
CODEGEN_RECEIVER_FACTS="$ROOT_DIR/src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy"
CODEGEN_RUNTIME_HEADER="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/runtime_header_owner.pgy"

for path in "$OWNER" "$OWNER_FIELDS" "$MATCH_BINDINGS" "$ITERATION_FACTS" \
    "$ASSIGNMENT_FACTS" "$CALL_TARGETS" "$BODY_ENV" "$BODY_TYPES" \
    "$PLACE_FACTS" "$GENERIC_FACTS" "$INITIALIZER_FACTS" \
    "$INITIALIZER_REFINEMENT" "$INITIALIZER_TABLE_BRIDGE" \
    "$INITIALIZER_CURSOR" "$STATEMENT_FACTS" \
    "$MIR_FACTS" "$DRIVER" "$PIPELINE" "$VERDICT" "$BODY_ADMISSION" "$ENTRY" \
    "$ADMITTED_ENTRY" \
    "$BUILTINS" "$TYPECHECK" "$TRANS_POLICY" \
    "$TRANS_EMIT" "$LLVM_EMIT" "$LLVM_RUNTIME" "$INLINE_RUNTIME" \
    "$EXPORT_RUNTIME" "$CODEGEN_RUNTIME_CALLS" "$CODEGEN_COLLECTION_RUNTIME" \
    "$CODEGEN_CALL_EMITTER" "$CODEGEN_PROGRAM_EMITTER" \
    "$CODEGEN_FUNCTION_EMITTER" "$CODEGEN_RECEIVER_FACTS" \
    "$CODEGEN_RUNTIME_HEADER"; do
    [[ -f "$path" ]] || {
        echo "[self-host-parity:semantic-environment-lifetime] missing $path" >&2
        exit 1
    }
done

function_body() {
    local path="$1"
    local function_name="$2"
    sed -n "/func ${function_name}(/,/^}/p" "$path"
}

assert_exact_call_files() {
    local call_term="$1"
    shift
    local actual
    local expected
    # No-match is collected as an empty set and rejected by the exact
    # comparison below; it is not a successful fallback.
    actual="$({ grep -R -l -F --include='*.pgy' "$call_term" \
        "$ROOT_DIR/src/self_hosted" || true; } | \
        sed "s#^$ROOT_DIR/##" | LC_ALL=C sort)"
    expected="$(printf '%s\n' "$@" | LC_ALL=C sort)"
    if [[ "$actual" != "$expected" ]]; then
        echo "[self-host-parity:semantic-environment-lifetime] unexpected caller set for $call_term" >&2
        echo "expected:" >&2
        printf '%s\n' "$expected" >&2
        echo "actual:" >&2
        printf '%s\n' "$actual" >&2
        exit 1
    fi
}

assert_admitted_core_has_no_reconstruction() {
    local path="$1"
    local function_name="$2"
    local body
    body="$(function_body "$path" "$function_name")"
    [[ -n "$body" ]] || {
        echo "[self-host-parity:semantic-environment-lifetime] missing admitted core $function_name" >&2
        exit 1
    }
    for forbidden in \
        'SemanticAstArtifactAnalysisMatches(' \
        'FactsMatchArtifact(' \
        'FactsFromArtifact(' \
        'SemanticAstExpressionSurfaceBorrowReady(' \
        'SemanticExpressionGraphFactsReady('; do
        if grep -Fq "$forbidden" <<<"$body"; then
            echo "[self-host-parity:semantic-environment-lifetime] admitted core $function_name reopened whole-artifact reconstruction: $forbidden" >&2
            exit 1
        fi
    done
}

assignment_type_body="$(function_body \
    "$ASSIGNMENT_FACTS" 'SemanticAstAssignmentTypeFactsFromArtifact')"
borrow_ready_count="$(grep -Fc \
    'SemanticAstExpressionSurfaceBorrowReady(' \
    <<<"$assignment_type_body" || true)"
if [[ "$borrow_ready_count" -ne 1 ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] assignment owner must prove the expression surface exactly once" >&2
    exit 1
fi
grep -Fq 'SemanticAstAssignmentTypeFactsFromAdmittedArtifact(' \
    <<<"$assignment_type_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked assignment wrapper bypasses its admitted core" >&2
    exit 1
}
assignment_admitted_body="$(function_body \
    "$ASSIGNMENT_FACTS" 'SemanticAstAssignmentTypeFactsFromAdmittedArtifact')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$assignment_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted assignment core rebuilds match-binding inputs" >&2
    exit 1
}
assert_admitted_core_has_no_reconstruction "$ASSIGNMENT_FACTS" \
    'SemanticAstAssignmentTypeFactsFromAdmittedArtifact'

require_borrowed_environment_push() {
    local path="$1"
    local function_name="$2"
    local body
    body="$(function_body "$path" "$function_name")"
    if [[ -z "$body" ]]; then
        echo "[self-host-parity:semantic-environment-lifetime] missing producer $function_name" >&2
        exit 1
    fi
    if grep -Eq 'ArrayPushOwnedString[[:space:]]*\([[:space:]]*(names|types|modes)[[:space:]]*,' <<<"$body"; then
        echo "[self-host-parity:semantic-environment-lifetime] $function_name copied a borrowed environment row" >&2
        exit 1
    fi
    if ! grep -Eq 'ArrayPush[[:space:]]*\([[:space:]]*(names|types|modes)[[:space:]]*,' <<<"$body"; then
        echo "[self-host-parity:semantic-environment-lifetime] $function_name lost its borrowed environment row" >&2
        exit 1
    fi
}

reset_body="$(sed -n '/func SemanticAstExpressionEnvironmentReset(/,/^}/p' "$OWNER")"
for term in \
    'while ArrayLength(names) > 0 { ArrayPop(names); }' \
    'while ArrayLength(types) > 0 { ArrayPop(types); }' \
    'while ArrayLength(modes) > 0 { ArrayPop(modes); }'; do
    grep -Fq "$term" <<<"$reset_body" || {
        echo "[self-host-parity:semantic-environment-lifetime] borrowed reset missing: $term" >&2
        exit 1
    }
done
if grep -Fq 'ArrayDropOwnedStrings' <<<"$reset_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] borrowed reset frees owner facts" >&2
    exit 1
fi

clear_body="$(sed -n '/func SemanticAstExpressionEnvironmentClear(/,/^}/p' "$OWNER")"
grep -Fq 'SemanticAstExpressionEnvironmentReset(names, types, modes);' <<<"$clear_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] last-consumer reset missing" >&2
    exit 1
}
grep -Fq 'ArrayDropOwnedStrings(names);' <<<"$clear_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] empty names backing cleanup missing" >&2
    exit 1
}
grep -Fq 'ArrayDropOwnedStrings(types);' <<<"$clear_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] types cleanup owner missing" >&2
    exit 1
}
grep -Fq 'ArrayDropOwnedStrings(modes);' <<<"$clear_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] modes cleanup owner missing" >&2
    exit 1
}
if grep -Eq 'Array(Pop|Push)\((names|types|modes)' <<<"$clear_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] clear bypassed borrowed reset owner" >&2
    exit 1
fi

for producer in \
    'SemanticAstExpressionSeedEnumValues' \
    'SemanticAstExpressionSeedParameters' \
    'SemanticAstExpressionSeedParameterModes' \
    'SemanticAstExpressionSeedVisibleLocals' \
    'SemanticAstExpressionSeedVisibleLocalModes' \
    'SemanticAstExpressionSeedVisibleIterationRows'; do
    require_borrowed_environment_push "$OWNER" "$producer"
done
require_borrowed_environment_push "$OWNER_FIELDS" \
    'SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors'
checked_owner_fields_body="$(function_body "$OWNER_FIELDS" \
    'SemanticAstExpressionSeedOwnerFields')"
grep -Fq 'SemanticAstNominalConstructorRowsReady(constructors)' \
    <<<"$checked_owner_fields_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked owner-field seed lost constructor row proof" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors(' \
    <<<"$checked_owner_fields_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked owner-field seed bypasses its admitted core" >&2
    exit 1
}
require_borrowed_environment_push "$MATCH_BINDINGS" 'SemanticAstExpressionSeedMatchCaseBindings'
require_borrowed_environment_push "$ITERATION_FACTS" 'SemanticAstIterationSeedVisibleRows'

ready_match_body="$(function_body "$MATCH_BINDINGS" 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact')"
[[ -n "$ready_match_body" ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] missing ready-artifact match environment owner" >&2
    exit 1
}
for forbidden in 'AstTreeArtifactReady(' 'AstExpressionGraphRowsReady('; do
    if grep -Fq "$forbidden" <<<"$ready_match_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] ready-artifact match environment repeats whole-graph readiness: $forbidden" >&2
        exit 1
    fi
done

admitted_match_body="$(function_body "$MATCH_BINDINGS" \
    'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts')"
[[ -n "$admitted_match_body" ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] missing admitted-facts match environment owner" >&2
    exit 1
}
for carried in \
    'enums: SemanticAstEnumFacts' \
    'function_scopes: SemanticAstFunctionScopeFacts' \
    'SemanticAstExpressionSeedMatchCaseBindings('; do
    grep -Fq "$carried" <<<"$admitted_match_body" || {
        echo "[self-host-parity:semantic-environment-lifetime] admitted match environment lost carried fact: $carried" >&2
        exit 1
    }
done
for forbidden in \
    'SemanticAstEnumFactsFromArtifact(' \
    'SemanticAstFunctionScopeFactsFromArtifact('; do
    if grep -Fq "$forbidden" <<<"$admitted_match_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] admitted match environment rebuilds an analysis-owned fact: $forbidden" >&2
        exit 1
    fi
done

checked_match_body="$(function_body "$MATCH_BINDINGS" 'SemanticAstExpressionSeedVisibleMatchBindings')"
grep -Fq 'AstTreeArtifactReady(artifact)' <<<"$checked_match_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked match environment lost artifact readiness" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$checked_match_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked match environment bypasses ready-artifact owner" >&2
    exit 1
}

initializer_body="$(function_body "$INITIALIZER_FACTS" 'SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObservedWithFunctionTables')"
grep -Fq 'SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables(' <<<"$initializer_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked initializer wrapper bypasses its admitted core" >&2
    exit 1
}
initializer_admitted_body="$(function_body "$INITIALIZER_FACTS" \
    'SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$initializer_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted initializer core rebuilds match-binding inputs" >&2
    exit 1
}
initializer_wrapper_body="$(function_body "$INITIALIZER_FACTS" 'SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObserved')"
grep -Fq 'SemanticAstExpressionFunctionTableFactsRelease(function_tables);' \
    <<<"$initializer_wrapper_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] standalone initializer wrapper leaked its owned callable table" >&2
    exit 1
}

call_target_body="$(function_body "$CALL_TARGETS" 'SemanticAstAnalysisResolveCallTargetsFromBody')"
grep -Fq 'SemanticAstAnalysisResolveCallTargetsFromAdmittedBody(' \
    <<<"$call_target_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked call-target resolver bypasses its admitted core" >&2
    exit 1
}
call_target_admitted_body="$(function_body "$CALL_TARGETS" \
    'SemanticAstAnalysisResolveCallTargetsFromAdmittedBody')"
grep -Fq 'SemanticAstBodyExpressionEnvironmentSeed(' \
    <<<"$call_target_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted call-target resolver bypasses the body-environment owner" >&2
    exit 1
}
if grep -Fq 'SemanticAstNominalConstructorRowsReady(' \
    <<<"$call_target_admitted_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted call-target resolver repeats constructor row proof" >&2
    exit 1
fi
grep -Fq 'SemanticAstNominalConstructorRowsReady(analysis.constructors)' \
    <<<"$call_target_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked call-target resolver lost constructor row proof" >&2
    exit 1
}
body_environment_body="$(function_body "$BODY_ENV" \
    'SemanticAstBodyExpressionEnvironmentSeed')"
grep -Fq 'SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors(' \
    <<<"$body_environment_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted body reopens checked constructor rows: environment repeats checked owner-field seeding" >&2
    exit 1
}
if grep -Fq 'SemanticAstExpressionSeedOwnerFields(' \
    <<<"$body_environment_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted body environment retained checked owner-field seeding" >&2
    exit 1
fi
for admitted_owner_field_consumer in \
    "$PLACE_FACTS|SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody" \
    "$ASSIGNMENT_FACTS|SemanticAstAssignmentTypeFactsFromAdmittedArtifact" \
    "$ITERATION_FACTS|SemanticAstIterationTypeFactsFromAdmittedArtifactWithFunctionTables" \
    "$STATEMENT_FACTS|SemanticAstStatementTypeFactsFromAdmittedArtifact" \
    "$GENERIC_FACTS|SemanticAstGenericSpecializationFactsFromAdmittedBody" \
    "$INITIALIZER_CURSOR|SemanticAstInitializerEnvironmentCursorAdvance"; do
    admitted_owner_field_path="${admitted_owner_field_consumer%%|*}"
    admitted_owner_field_function="${admitted_owner_field_consumer#*|}"
    admitted_owner_field_body="$(function_body \
        "$admitted_owner_field_path" "$admitted_owner_field_function")"
    grep -Fq \
        'SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors(' \
        <<<"$admitted_owner_field_body" || {
        echo "[self-host-parity:semantic-environment-lifetime] admitted body core bypasses admitted owner-field seeding: $admitted_owner_field_function" >&2
        exit 1
    }
    if grep -Fq 'SemanticAstExpressionSeedOwnerFields(' \
        <<<"$admitted_owner_field_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] admitted body core retained checked owner-field seeding: $admitted_owner_field_function" >&2
        exit 1
    fi
done
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$body_environment_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] body-environment owner rebuilds admitted match bindings" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' \
    <<<"$body_environment_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] body-environment owner repeats checked match bindings" >&2
    exit 1
fi

place_body="$(function_body "$PLACE_FACTS" 'SemanticAstAnalysisResolveExpressionPlacesFromBody')"
grep -Fq 'SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody(' \
    <<<"$place_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked expression-place resolver bypasses its admitted core" >&2
    exit 1
}
place_admitted_body="$(function_body "$PLACE_FACTS" \
    'SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$place_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted expression-place resolver rebuilds match bindings" >&2
    exit 1
}
statement_body="$(function_body "$STATEMENT_FACTS" 'SemanticAstStatementTypeFactsFromArtifact')"
grep -Fq 'SemanticAstExpressionSurfaceBorrowReady(' <<<"$statement_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] statement resolver lost expression-surface readiness proof" >&2
    exit 1
}
grep -Fq 'SemanticAstStatementTypeFactsFromAdmittedArtifact(' <<<"$statement_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked statement wrapper bypasses its admitted core" >&2
    exit 1
}
statement_admitted_body="$(function_body \
    "$STATEMENT_FACTS" 'SemanticAstStatementTypeFactsFromAdmittedArtifact')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$statement_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted statement core rebuilds match-binding inputs" >&2
    exit 1
}
assert_admitted_core_has_no_reconstruction "$STATEMENT_FACTS" \
    'SemanticAstStatementTypeFactsFromAdmittedArtifact'


generic_body="$(function_body "$GENERIC_FACTS" 'SemanticAstGenericSpecializationFactsFromBody')"
grep -Fq 'SemanticAstExpressionSurfaceBorrowReady(' <<<"$generic_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] generic resolver lost expression-surface readiness proof" >&2
    exit 1
}
grep -Fq 'SemanticAstGenericSpecializationFactsFromAdmittedBody(' \
    <<<"$generic_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked generic resolver bypasses its admitted core" >&2
    exit 1
}
generic_admitted_body="$(function_body "$GENERIC_FACTS" \
    'SemanticAstGenericSpecializationFactsFromAdmittedBody')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromAdmittedFacts(' \
    <<<"$generic_admitted_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted generic resolver rebuilds match bindings" >&2
    exit 1
}

# Every body-stage core below consumes the producer-sealed analysis epoch. A
# new stage may perform bounded row work, but it may not reopen an artifact,
# expression-surface, or full expression-graph proof.
for admitted_stage in \
    "$BODY_TYPES|SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved" \
    "$BODY_TYPES|SemanticAstBodyTypeBundleFromAdmittedAnalysis" \
    "$INITIALIZER_FACTS|SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables" \
    "$INITIALIZER_TABLE_BRIDGE|SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsUsingFunctionTables" \
    "$INITIALIZER_TABLE_BRIDGE|SemanticAstInitializerTypeFactsFromAdmittedArtifactObservedWithFunctionTables" \
    "$ITERATION_FACTS|SemanticAstIterationTypeFactsFromAdmittedArtifactWithFunctionTables" \
    "$CALL_TARGETS|SemanticAstAnalysisResolveCallTargetsFromAdmittedBody" \
    "$INITIALIZER_REFINEMENT|SemanticAstInitializerTypeFactsRefinedByIterationsFromAdmittedFactsWithFunctionTables" \
    "$PLACE_FACTS|SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody" \
    "$ASSIGNMENT_FACTS|SemanticAstAssignmentTypeFactsFromAdmittedArtifact" \
    "$STATEMENT_FACTS|SemanticAstStatementTypeFactsFromAdmittedArtifact" \
    "$GENERIC_FACTS|SemanticAstGenericSpecializationFactsFromAdmittedBody"; do
    stage_path="${admitted_stage%%|*}"
    stage_function="${admitted_stage#*|}"
    assert_admitted_core_has_no_reconstruction \
        "$stage_path" "$stage_function"
done

mir_ready_body="$(function_body "$MIR_FACTS" 'SelfMirProgramFactsFromReadyArtifactObserved')"
for forbidden in \
    'SemanticAstArtifactAnalysisMatches(' \
    'SemanticAstInitializerTypeFactsReadyProjection(' \
    'SemanticAstAssignmentTypeFactsMatchArtifact(' \
    'SemanticAstStatementTypeFactsMatchArtifact(' \
    'SemanticAstGenericSpecializationFactsMatchExpressionGraph('; do
    if grep -Fq "$forbidden" <<<"$mir_ready_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] MIR ready-artifact core repeats whole-semantic proof: $forbidden" >&2
        exit 1
    fi
done

mir_checked_body="$(function_body "$MIR_FACTS" 'SelfMirProgramFactsFromArtifact')"
grep -Fq 'SemanticAstArtifactAnalysisMatches(' <<<"$mir_checked_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked MIR entrypoint lost artifact proof" >&2
    exit 1
}
grep -Fq 'SelfMirProgramFactsFromReadyArtifact(' <<<"$mir_checked_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] checked MIR entrypoint bypasses ready-artifact owner" >&2
    exit 1
}

driver_mir_body="$(function_body "$DRIVER" 'DriverRung2MirProjectionFromVerifiedFactsObserved')"
grep -Fq 'SelfMirProgramFactsBeforeCanonicalIdsObserved(' <<<"$driver_mir_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] verified driver lost ready-artifact MIR path" >&2
    exit 1
}
if grep -Fq 'SelfMirProgramFactsFromArtifact(' <<<"$driver_mir_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] verified driver repeats checked MIR proof" >&2
    exit 1
fi

admission_body="$(function_body "$VERDICT" \
    'SemanticAstArtifactAdmissionReady')"
for forbidden in \
    'AstTreeArtifactReady(' \
    'TypedAstArenaParallelRowsReady(' \
    'AstExpressionGraphRowsReady(' \
    'StringLength(' \
    'CharCode(' \
    'FactsMatchArtifact(' \
    'FactsFromArtifact(' \
    'RowsFromArtifact('; do
    if grep -Fq "$forbidden" <<<"$admission_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] semantic admission repeats an unbounded artifact proof: $forbidden" >&2
        exit 1
    fi
done
body_admission_body="$(function_body "$BODY_ADMISSION" \
    'SemanticAstBodyAnalysisAdmissionReady')"
grep -Fq 'analysis.function_scopes.ok' <<<"$body_admission_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] semantic admission lost carried function-scope readiness" >&2
    exit 1
}
grep -Fq 'analysis.function_scopes.function_node_ids' \
    <<<"$body_admission_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] semantic admission lost function-scope row shape" >&2
    exit 1
}
analysis_producer_body="$(function_body "$VERDICT" \
    'SemanticAstArtifactAnalyzeFromExpressionSurfaces')"
grep -Fq 'SemanticAstFunctionScopeFactsFromArtifact(artifact)' \
    <<<"$analysis_producer_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] semantic analysis no longer owns function-scope production" >&2
    exit 1
}
grep -Fq 'kind_surfaces, function_tables, function_scopes' \
    <<<"$analysis_producer_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] semantic analysis drops function scopes before sealing" >&2
    exit 1
}

c_ready_body="$(function_body "$CODEGEN_PROGRAM_EMITTER" \
    'GenerateCUnitFromReadySemanticFacts')"
if grep -Fq 'func GenerateCUnitFromSemanticFacts(' \
    "$CODEGEN_PROGRAM_EMITTER"; then
    echo "[self-host-parity:semantic-environment-lifetime] retired checked C-facts fallback returned" >&2
    exit 1
fi
for forbidden in \
    'CodegenAstTreeArtifactReady(' \
    'SemanticAstArtifactAnalysisMatches(' \
    'CodegenSemanticStatementTypeFactsReady(' \
    'CodegenSemanticAssignmentTypeFactsReady(' \
    'SemanticAstIntentSignatureFactsReady(' \
    'CodegenCallableReceiverFactsReady(' \
    'CodegenDomainRuntimeFactsReady(' \
    'FactsMatchArtifact(' \
    'FactsFromArtifact(' \
    'RowsFromArtifact(' \
    'AstTreeArtifactIdentityDigest('; do
    if grep -Fq "$forbidden" <<<"$c_ready_body"; then
        echo "[self-host-parity:semantic-environment-lifetime] C ready core repeats admitted semantic work: $forbidden" >&2
        exit 1
    fi
done
ready_receipt_count="$(grep -Fc 'SemanticAstArtifactAdmissionReady(' \
    <<<"$c_ready_body" || true)"
if [[ "$ready_receipt_count" -ne 1 ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] C ready core must consume exactly one semantic admission receipt" >&2
    exit 1
fi
for admitted_check in \
    'SemanticAstIntentSignatureFactsAdmittedReady(' \
    'CodegenCallableReceiverFactsAdmittedReady(' \
    'CodegenDomainRuntimeFactsAdmittedReady('; do
    grep -Fq "$admitted_check" <<<"$c_ready_body" || {
        echo "[self-host-parity:semantic-environment-lifetime] C ready core lost admitted row-shape check: $admitted_check" >&2
        exit 1
    }
done
receiver_admitted_ready_body="$(function_body "$CODEGEN_RECEIVER_FACTS" \
    'CodegenCallableReceiverFactsAdmittedReady')"
grep -Fq 'facts.admitted' <<<"$receiver_admitted_ready_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] callable receiver admitted shape lost its identity receipt" >&2
    exit 1
}
receiver_admitted_carriage_body="$(function_body "$CODEGEN_RECEIVER_FACTS" \
    'CodegenCallableReceiverCarriageForAdmittedSignatureOrDie')"
for admitted_guard in '!facts.ok' '!facts.admitted' 'signature_index < 0'; do
    grep -Fq "$admitted_guard" <<<"$receiver_admitted_carriage_body" || {
        echo "[self-host-parity:semantic-environment-lifetime] admitted callable receiver row lost O(1) fail-closed guard: $admitted_guard" >&2
        exit 1
    }
done
if grep -Fq 'CodegenCallableReceiverFactsReady(' \
    <<<"$receiver_admitted_carriage_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted callable receiver row repeats the whole-table proof" >&2
    exit 1
fi
receiver_checked_carriage_body="$(function_body "$CODEGEN_RECEIVER_FACTS" \
    'CodegenCallableReceiverCarriageForSignatureOrDie')"
grep -Fq 'CodegenCallableReceiverFactsReady(' \
    <<<"$receiver_checked_carriage_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] arbitrary callable receiver lookup lost its full proof" >&2
    exit 1
}
receiver_admitted_producer_body="$(function_body "$CODEGEN_RECEIVER_FACTS" \
    'CodegenCallableReceiverFactsFromAdmittedAnalysis')"
receiver_producer_ready_count="$(grep -Fc \
    'CodegenCallableReceiverFactsReady(' \
    <<<"$receiver_admitted_producer_body" || true)"
if [[ "$receiver_producer_ready_count" -ne 1 ]] ||
    ! grep -Fq 'facts.ok, true' <<<"$receiver_admitted_producer_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted callable receiver producer must prove identity once before sealing" >&2
    exit 1
fi
assert_exact_call_files \
    'CodegenCallableReceiverFactsFromAdmittedAnalysis(' \
    'src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy'
for admitted_row_accessor in \
    'CodegenCallableReceiverCarriageForAdmittedSignatureOrDie(' \
    'CodegenCallableReceiverRoleErasedForAdmittedSignatureOrDie('; do
    grep -Fq "$admitted_row_accessor" "$CODEGEN_FUNCTION_EMITTER" || {
        echo "[self-host-parity:semantic-environment-lifetime] ready function emission bypasses admitted receiver rows: $admitted_row_accessor" >&2
        exit 1
    }
done
for repeated_row_proof in \
    'CodegenCallableReceiverCarriageForSignatureOrDie(' \
    'CodegenCallableReceiverRoleErasedForSignatureOrDie(' \
    'CodegenCallableReceiverFactsReady('; do
    if grep -Fq "$repeated_row_proof" "$CODEGEN_FUNCTION_EMITTER"; then
        echo "[self-host-parity:semantic-environment-lifetime] ready function emission repeats callable receiver proof: $repeated_row_proof" >&2
        exit 1
    fi
done
ready_admission_line="$(grep -nF 'SemanticAstArtifactAdmissionReady(' \
    <<<"$c_ready_body" | head -n 1 | cut -d: -f1)"
ready_first_work_line="$(grep -nF 'RejectUnsupportedCodegenBuiltins(' \
    <<<"$c_ready_body" | head -n 1 | cut -d: -f1)"
if [[ -z "$ready_admission_line" || -z "$ready_first_work_line" ||
    "$ready_admission_line" -ge "$ready_first_work_line" ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] C ready core performs work before semantic admission" >&2
    exit 1
fi

ast_entry_body="$(function_body "$ENTRY" 'GenerateCUnitFromAstArtifact')"
compact_analysis_count="$(grep -Fc \
    'SemanticAstArtifactAnalyzeCompactBridge(' <<<"$ast_entry_body" || true)"
if [[ "$compact_analysis_count" -ne 1 ]] ||
    ! grep -Fq 'GenerateCUnitFromAdmittedSemanticArtifact(' \
        <<<"$ast_entry_body" ||
    grep -Fq 'GenerateCUnitFromSemanticArtifact(' <<<"$ast_entry_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] direct codegen seed lost its one-analysis admitted path" >&2
    exit 1
fi

admitted_entry_body="$(function_body "$ADMITTED_ENTRY" \
    'GenerateCUnitFromAdmittedSemanticArtifactObserved')"
grep -Fq 'GenerateCUnitFromReadySemanticFacts(' <<<"$admitted_entry_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] admitted semantic artifact bypasses the C ready core" >&2
    exit 1
}
if grep -Fq 'SemanticAstArtifactAnalysisMatches(' <<<"$admitted_entry_body" ||
    grep -Fq 'GenerateCUnitFromSemanticFacts(' <<<"$admitted_entry_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted semantic artifact reopened checked reconstruction" >&2
    exit 1
fi
admitted_receipt_line="$(grep -nF 'SemanticAstArtifactAdmissionReady(' \
    <<<"$admitted_entry_body" | head -n 1 | cut -d: -f1)"
admitted_first_work_line="$(grep -nF 'SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved(' \
    <<<"$admitted_entry_body" | head -n 1 | cut -d: -f1)"
if [[ -z "$admitted_receipt_line" || -z "$admitted_first_work_line" ||
    "$admitted_receipt_line" -ge "$admitted_first_work_line" ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] admitted adapter performs work before semantic admission" >&2
    exit 1
fi

# The fast path relies on one immutable-after-admission call graph.  Any new
# direct caller must choose the checked public entry or update this owner gate
# with explicit evidence that it receives the same admitted epoch.
assert_exact_call_files 'GenerateCUnitFromReadySemanticFacts(' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy' \
    'src/self_hosted/codegen/emission/program_emit.pgy' \
    'src/self_hosted/codegen/emission/program_entry_owner.pgy'
assert_exact_call_files 'GenerateCUnitFromAdmittedSemanticArtifact(' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy' \
    'src/self_hosted/codegen/emission/program_entry_owner.pgy' \
    'src/self_hosted/compiler/driver_pipeline_owner.pgy'
assert_exact_call_files 'GenerateCUnitFromAdmittedSemanticArtifactObserved(' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy' \
    'src/self_hosted/codegen/run/codegen_run_owner.pgy'
assert_exact_call_files 'GenerateCFromVerifiedSemanticArtifact(' \
    'src/self_hosted/codegen/emission/program_entry_owner.pgy' \
    'src/self_hosted/compiler/driver_rung2_owner.pgy'
assert_exact_call_files 'SemanticAstArtifactAdmissionReady(' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy' \
    'src/self_hosted/codegen/emission/program_emit.pgy' \
    'src/self_hosted/compiler/driver_rung2_owner.pgy' \
    'src/self_hosted/semantic/ast_artifact_verdict_contract_owner.pgy' \
    'src/self_hosted/semantic/ast_artifact_verdict_owner.pgy' \
    'src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy' \
    'src/self_hosted/semantic/ast_body_type_bundle_admission_receipt_owner.pgy'
assert_exact_call_files 'SemanticAstBodyTypeBundleFromAdmittedAnalysis(' \
    'src/self_hosted/semantic/ast_body_type_bundle_owner.pgy'
assert_exact_call_files 'SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved(' \
    'src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy' \
    'src/self_hosted/compiler/driver_rung2_owner.pgy' \
    'src/self_hosted/semantic/ast_body_analysis_admission_contract_owner.pgy' \
    'src/self_hosted/semantic/ast_body_type_bundle_owner.pgy'

for production_body in \
    "$ADMITTED_ENTRY|GenerateCUnitFromAdmittedSemanticArtifactObserved|SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved(" \
    "$DRIVER|VerifyArtifactForDriverRung2FromAdmittedAnalysisObserved|SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved("; do
    production_path="${production_body%%|*}"
    production_rest="${production_body#*|}"
    production_function="${production_rest%%|*}"
    admitted_call="${production_rest#*|}"
    body="$(function_body "$production_path" "$production_function")"
    grep -Fq "$admitted_call" <<<"$body" || {
        echo "[self-host-parity:semantic-environment-lifetime] production body $production_function lost admitted body analysis" >&2
        exit 1
    }
    for checked_call in \
        'SemanticAstBodyTypeBundleFromAnalysis(' \
        'SemanticAstBodyTypeBundleFromAnalysisObserved('; do
        if grep -Fq "$checked_call" <<<"$body"; then
            echo "[self-host-parity:semantic-environment-lifetime] production body $production_function reopened checked body analysis: $checked_call" >&2
            exit 1
        fi
    done
done

verified_entry_body="$(function_body "$ENTRY" \
    'GenerateCFromVerifiedSemanticArtifact')"
grep -Fq 'GenerateCUnitFromReadySemanticFacts(' <<<"$verified_entry_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] verified C entrypoint bypasses the ready core" >&2
    exit 1
}
if grep -Fq 'GenerateCUnitFromSemanticFacts(' <<<"$verified_entry_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] verified C entrypoint reopened checked reconstruction" >&2
    exit 1
fi

pipeline_body="$(function_body "$PIPELINE" 'CompileAstArtifactToC')"
pipeline_analysis_count="$(grep -Fc \
    'SemanticAstArtifactAnalyzeCompactBridge(' <<<"$pipeline_body" || true)"
if [[ "$pipeline_analysis_count" -ne 1 ]] ||
    ! grep -Fq 'GenerateCUnitFromAdmittedSemanticArtifact(' <<<"$pipeline_body" ||
    grep -Fq 'GenerateCUnitFromReadySemanticFacts(' <<<"$pipeline_body" ||
    grep -Fq 'GenerateCUnitFromSemanticFacts(' <<<"$pipeline_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] source-to-C pipeline lost its one-analysis admitted path" >&2
    exit 1
fi

driver_consumer_body="$(function_body "$DRIVER" \
    'CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved')"
driver_analysis_count="$(grep -Fc \
    'SemanticAstArtifactAnalyzeWithExpressionGraph(' \
    <<<"$driver_consumer_body" || true)"
if [[ "$driver_analysis_count" -ne 1 ]] ||
    ! grep -Fq 'GenerateCFromVerifiedSemanticArtifact(' \
        <<<"$driver_consumer_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] integrated driver lost its one-analysis verified C path" >&2
    exit 1
fi

for consumer_contract in \
    "$ASSIGNMENT_FACTS|SemanticAstAssignmentTypeFactsFromAdmittedArtifact" \
    "$CALL_TARGETS|SemanticAstAnalysisResolveCallTargetsFromAdmittedBody" \
    "$PLACE_FACTS|SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody" \
    "$GENERIC_FACTS|SemanticAstGenericSpecializationFactsFromAdmittedBody" \
    "$INITIALIZER_FACTS|SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables" \
    "$ITERATION_FACTS|SemanticAstIterationTypeFactsFromAdmittedArtifactWithFunctionTables" \
    "$STATEMENT_FACTS|SemanticAstStatementTypeFactsFromAdmittedArtifact"; do
    path="${consumer_contract%%|*}"
    function_name="${consumer_contract#*|}"
    function_body "$path" "$function_name" |
        grep -Fq 'SemanticAstExpressionEnvironmentClear(names, types, modes);' || {
        echo "[self-host-parity:semantic-environment-lifetime] missing last-consumer cleanup in $function_name" >&2
        exit 1
    }
done

for reuse_contract in \
    "$CALL_TARGETS|SemanticAstAnalysisResolveCallTargetsFromAdmittedBody" \
    "$GENERIC_FACTS|SemanticAstGenericSpecializationFactsFromAdmittedBody" \
    "$INITIALIZER_CURSOR|SemanticAstInitializerEnvironmentCursorAdvance" \
    "$STATEMENT_FACTS|SemanticAstStatementTypeFactsFromAdmittedArtifact"; do
    path="${reuse_contract%%|*}"
    function_name="${reuse_contract#*|}"
    function_body "$path" "$function_name" |
        grep -Fq 'SemanticAstExpressionEnvironmentReset(names, types, modes);' || {
        echo "[self-host-parity:semantic-environment-lifetime] growing environment backing is not reused in $function_name" >&2
        exit 1
    }
done

generic_surface_epoch="$(sed -n \
    '/while surface_index < SemanticAstExpressionSurfaceCount(/,/SemanticAstExpressionEnvironmentClear(names, types, modes);/p' \
    "$GENERIC_FACTS")"
generic_environment_prefix="$(sed -n \
    '/func SemanticAstGenericSpecializationFactsFromAdmittedBody(/,/let surface_index: Int = 0;/p' \
    "$GENERIC_FACTS")"
for fresh_environment in \
    'let names: Array<String> = [];' \
    'let types: Array<String> = [];' \
    'let modes: Array<String> = [];'; do
    [[ "$(grep -Fc "$fresh_environment" <<<"$generic_environment_prefix" || true)" -eq 1 ]] || {
        echo "[self-host-parity:semantic-environment-lifetime] generic owner lost its one reusable environment: $fresh_environment" >&2
        exit 1
    }
    if grep -Fq "$fresh_environment" <<<"$generic_surface_epoch"; then
        echo "[self-host-parity:semantic-environment-lifetime] generic surface loop recreated environment backing: $fresh_environment" >&2
        exit 1
    fi
done
[[ "$(grep -Fc 'SemanticAstExpressionEnvironmentReset(names, types, modes);' \
    <<<"$generic_surface_epoch" || true)" -eq 1 ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] generic surface loop lost its exact environment reset" >&2
    exit 1
}
[[ "$(grep -Fc 'SemanticAstExpressionEnvironmentClear(names, types, modes);' \
    <<<"$generic_surface_epoch" || true)" -eq 1 ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] generic owner lost its post-surface last-consumer cleanup" >&2
    exit 1
}
generic_last_surface_line="$(grep -n -F 'surface_index = surface_index + 1;' \
    <<<"$generic_surface_epoch" | tail -n 1 | cut -d: -f1)"
generic_clear_line="$(grep -n -F \
    'SemanticAstExpressionEnvironmentClear(names, types, modes);' \
    <<<"$generic_surface_epoch" | cut -d: -f1)"
if [[ -z "$generic_last_surface_line" || -z "$generic_clear_line" || \
    "$generic_clear_line" -le "$generic_last_surface_line" ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] generic owner clears its environment before the final surface consumer" >&2
    exit 1
fi

statement_row_epoch="$(sed -n \
    '/while i < SemanticAstStatementCount(statements)/,/SemanticAstExpressionEnvironmentClear(names, types, modes);/p' \
    "$STATEMENT_FACTS")"
statement_environment_prefix="$(sed -n \
    '/func SemanticAstStatementTypeFactsFromAdmittedArtifact(/,/let i: Int = 0;/p' \
    "$STATEMENT_FACTS")"
for fresh_environment in \
    'let names: Array<String> = [];' \
    'let types: Array<String> = [];' \
    'let modes: Array<String> = [];'; do
    [[ "$(grep -Fc "$fresh_environment" <<<"$statement_environment_prefix" || true)" -eq 1 ]] || {
        echo "[self-host-parity:semantic-environment-lifetime] statement owner lost its one reusable environment: $fresh_environment" >&2
        exit 1
    }
    [[ "$(grep -Fc "$fresh_environment" <<<"$statement_row_epoch" || true)" -eq 0 ]] || {
        echo "[self-host-parity:semantic-environment-lifetime] statement row loop recreated environment backing: $fresh_environment" >&2
        exit 1
    }
done
[[ "$(grep -Fc 'SemanticAstExpressionEnvironmentReset(names, types, modes);' \
    <<<"$statement_row_epoch" || true)" -eq 1 ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] statement row loop lost its exact environment reset" >&2
    exit 1
}
[[ "$(grep -Fc 'SemanticAstExpressionEnvironmentClear(names, types, modes);' \
    <<<"$statement_row_epoch" || true)" -eq 1 ]] || {
    echo "[self-host-parity:semantic-environment-lifetime] statement owner lost its post-row last-consumer cleanup" >&2
    exit 1
}
statement_last_row_line="$(grep -n -F 'i = i + 1;' \
    <<<"$statement_row_epoch" | tail -n 1 | cut -d: -f1)"
statement_clear_line="$(grep -n -F \
    'SemanticAstExpressionEnvironmentClear(names, types, modes);' \
    <<<"$statement_row_epoch" | cut -d: -f1)"
if [[ -z "$statement_last_row_line" || -z "$statement_clear_line" || \
    "$statement_clear_line" -le "$statement_last_row_line" ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] statement owner clears its environment before the final row consumer" >&2
    exit 1
fi

for copy_contract in \
    "$ASSIGNMENT_FACTS|Concat(\"\", target_binding_mode)" \
    "$ASSIGNMENT_FACTS|Concat(\"\", target_type)" \
    "$ASSIGNMENT_FACTS|Concat(\"\", index_type)" \
    "$ASSIGNMENT_FACTS|Concat(\"\", expected)" \
    "$ASSIGNMENT_FACTS|Concat(\"\", inferred)" \
    "$ITERATION_FACTS|Concat(\"\", binding_type)" \
    "$ITERATION_FACTS|Concat(\"\", iterable_type)" \
    "$STATEMENT_FACTS|Concat(\"\", expected)" \
    "$STATEMENT_FACTS|Concat(\"\", inferred)" \
    "$GENERIC_FACTS|Concat(\"\", actuals[i])"; do
    path="${copy_contract%%|*}"
    term="${copy_contract#*|}"
    grep -Fq "$term" "$path" || {
        echo "[self-host-parity:semantic-environment-lifetime] result copy contract missing: $term" >&2
        exit 1
    }
done

grep -Fq 'ArrayPushOwnedString^Void^Array<String>|String' "$BUILTINS"
grep -Fq 'ArrayDropOwnedStrings^Void^Array<String>' "$BUILTINS"
grep -Fq 'ArrayPushOwnedString' "$TYPECHECK"
grep -Fq 'ArrayDropOwnedStrings' "$TYPECHECK"
grep -Fq 'STDLIB_COLLECTION_ARRAY_DROP_OWNED_STRINGS' "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_collections.c"
grep -Fq 'TRANSPILER_ARRAY_OP_PUSH_OWNED_STRING' "$TRANS_POLICY"
grep -Fq 'TRANSPILER_ARRAY_OP_DROP_OWNED_STRINGS' "$TRANS_POLICY"
grep -Fq 'pgy_array_push_owned_%s' "$TRANS_EMIT"
grep -Fq 'pgy_array_drop_owned_String' "$TRANS_EMIT"
grep -Fq 'LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING' "$LLVM_EMIT"
grep -Fq 'LLVM_ARRAY_BUILTIN_DROP_OWNED_STRINGS' "$LLVM_EMIT"
grep -Fq 'array_push_owned' "$LLVM_RUNTIME"
grep -Fq 'array_drop_owned' "$LLVM_RUNTIME"
grep -Fq 'pgy_array_push_owned_String' "$INLINE_RUNTIME"
grep -Fq 'pgy_array_drop_owned_String' "$INLINE_RUNTIME"
grep -Fq 'pgy_runtime_strdup(value != NULL ? value : "")' "$INLINE_RUNTIME" || {
    echo "[self-host-parity:semantic-environment-lifetime] inline owned push does not duplicate its input" >&2
    exit 1
}
grep -Fq 'pgy_array_push_owned_String' "$EXPORT_RUNTIME"
grep -Fq 'pgy_array_drop_owned_String' "$EXPORT_RUNTIME"
grep -Fq 'source_name == "ArrayPushOwnedString"' "$CODEGEN_RUNTIME_CALLS"
grep -Fq 'CollectionRuntimeCOwnedStringPushFn()' "$CODEGEN_RUNTIME_CALLS"
grep -Fq 'source_name == "ArrayDropOwnedStrings"' "$CODEGEN_RUNTIME_CALLS"
grep -Fq 'CollectionRuntimeCOwnedStringDropFn()' "$CODEGEN_RUNTIME_CALLS"
grep -Fq 'func CollectionRuntimeCOwnedStringPushFn()' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'func CollectionRuntimeCOwnedStringDropFn()' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'pgy_as_owned_copy' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'pgy_as_drop_owned' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'free((void *)a->data[i])' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'next_data == NULL' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'PGY_RUNTIME_PANIC_CLASS_OOM' "$CODEGEN_COLLECTION_RUNTIME"
grep -Fq 'uses_str || uses_array || uses_io' "$CODEGEN_PROGRAM_EMITTER" || {
    echo "[self-host-parity:semantic-environment-lifetime] array runtime lost its direct string-header dependency" >&2
    exit 1
}
grep -Fq 'usage.uses_box_array, uses_array, usage.uses_spawn' \
    "$CODEGEN_PROGRAM_EMITTER" || {
    echo "[self-host-parity:semantic-environment-lifetime] array runtime dependency was not passed to the header owner" >&2
    exit 1
}
grep -Fq 'else if uses_array {' "$CODEGEN_RUNTIME_HEADER" &&
grep -Fq '#include \"pgy_runtime_panic_contract.h\"' \
    "$CODEGEN_RUNTIME_HEADER" || {
    echo "[self-host-parity:semantic-environment-lifetime] array runtime lost its panic-contract header" >&2
    exit 1
}
grep -Fq 'func RewriteSemanticOwnedStringArrayCall(' "$CODEGEN_CALL_EMITTER"
grep -Fq '"Array<String>", "inout"' "$CODEGEN_CALL_EMITTER"
grep -Fq 'CollectionRuntimeCOwnedStringPushFn()' "$CODEGEN_CALL_EMITTER"
grep -Fq 'CollectionRuntimeCOwnedStringDropFn()' "$CODEGEN_CALL_EMITTER"

echo "[self-host-parity:semantic-environment-lifetime] borrowed environment reset and owned table cleanup are owner-directed"
