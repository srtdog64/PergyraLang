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
PLACE_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_expression_place_fact_owner.pgy"
GENERIC_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy"
INITIALIZER_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
INITIALIZER_CURSOR="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_environment_cursor_owner.pgy"
STATEMENT_FACTS="$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
BUILTINS="$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"
MIR_FACTS="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
DRIVER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
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

for path in "$OWNER" "$OWNER_FIELDS" "$MATCH_BINDINGS" "$ITERATION_FACTS" \
    "$ASSIGNMENT_FACTS" "$CALL_TARGETS" "$PLACE_FACTS" "$GENERIC_FACTS" \
    "$INITIALIZER_FACTS" "$INITIALIZER_CURSOR" "$STATEMENT_FACTS" \
    "$MIR_FACTS" "$DRIVER" \
    "$BUILTINS" "$TYPECHECK" "$TRANS_POLICY" \
    "$TRANS_EMIT" "$LLVM_EMIT" "$LLVM_RUNTIME" "$INLINE_RUNTIME" \
    "$EXPORT_RUNTIME" "$CODEGEN_RUNTIME_CALLS" "$CODEGEN_COLLECTION_RUNTIME" \
    "$CODEGEN_CALL_EMITTER"; do
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

assignment_type_body="$(function_body \
    "$ASSIGNMENT_FACTS" 'SemanticAstAssignmentTypeFactsFromArtifact')"
borrow_ready_count="$(grep -Fc \
    'SemanticAstExpressionSurfaceBorrowReady(' \
    <<<"$assignment_type_body" || true)"
if [[ "$borrow_ready_count" -ne 1 ]]; then
    echo "[self-host-parity:semantic-environment-lifetime] assignment owner must prove the expression surface exactly once" >&2
    exit 1
fi
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' \
    <<<"$assignment_type_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] assignment hot loop lost the borrowed match-binding seam" >&2
    exit 1
}
if grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindings(' \
    <<<"$assignment_type_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] assignment hot loop repeats checked expression-graph readiness" >&2
    exit 1
fi

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
require_borrowed_environment_push "$OWNER_FIELDS" 'SemanticAstExpressionSeedOwnerFields'
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
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$initializer_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] initializer hot loop lost ready-artifact match environment" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' <<<"$initializer_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] initializer hot loop repeats checked match environment" >&2
    exit 1
fi
initializer_wrapper_body="$(function_body "$INITIALIZER_FACTS" 'SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObserved')"
grep -Fq 'SemanticAstExpressionFunctionTableFactsRelease(function_tables);' \
    <<<"$initializer_wrapper_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] standalone initializer wrapper leaked its owned callable table" >&2
    exit 1
}

call_target_body="$(function_body "$CALL_TARGETS" 'SemanticAstAnalysisResolveCallTargetsFromBody')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$call_target_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] call-target resolver lost ready-artifact match environment" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' <<<"$call_target_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] call-target resolver repeats checked match environment" >&2
    exit 1
fi

place_body="$(function_body "$PLACE_FACTS" 'SemanticAstAnalysisResolveExpressionPlacesFromBody')"
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$place_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] expression-place resolver lost ready-artifact match environment" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' <<<"$place_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] expression-place resolver repeats checked match environment" >&2
    exit 1
fi
statement_body="$(function_body "$STATEMENT_FACTS" 'SemanticAstStatementTypeFactsFromArtifact')"
grep -Fq 'SemanticAstExpressionSurfaceBorrowReady(' <<<"$statement_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] statement resolver lost expression-surface readiness proof" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$statement_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] statement resolver lost ready-artifact match environment" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' <<<"$statement_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] statement resolver repeats checked match environment" >&2
    exit 1
fi


generic_body="$(function_body "$GENERIC_FACTS" 'SemanticAstGenericSpecializationFactsFromBody')"
grep -Fq 'SemanticAstExpressionSurfaceBorrowReady(' <<<"$generic_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] generic resolver lost expression-surface readiness proof" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact(' <<<"$generic_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] generic resolver lost ready-artifact match environment" >&2
    exit 1
}
if grep -Eq 'SemanticAstExpressionSeedVisibleMatchBindings[[:space:]]*\(' <<<"$generic_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] generic resolver repeats checked match environment" >&2
    exit 1
fi

mir_ready_body="$(function_body "$MIR_FACTS" 'SelfMirProgramFactsFromReadyArtifact')"
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

driver_mir_body="$(function_body "$DRIVER" 'DriverRung2MirProjectionFromAnalysisObserved')"
grep -Fq 'SelfMirProgramFactsFromReadyArtifact(' <<<"$driver_mir_body" || {
    echo "[self-host-parity:semantic-environment-lifetime] verified driver lost ready-artifact MIR path" >&2
    exit 1
}
if grep -Fq 'SelfMirProgramFactsFromArtifact(' <<<"$driver_mir_body"; then
    echo "[self-host-parity:semantic-environment-lifetime] verified driver repeats checked MIR proof" >&2
    exit 1
fi

for consumer_contract in \
    "$ASSIGNMENT_FACTS|SemanticAstAssignmentTypeFactsFromArtifact" \
    "$CALL_TARGETS|SemanticAstAnalysisResolveCallTargetsFromBody" \
    "$PLACE_FACTS|SemanticAstAnalysisResolveExpressionPlacesFromBody" \
    "$GENERIC_FACTS|SemanticAstGenericSpecializationFactsFromBody" \
    "$INITIALIZER_FACTS|SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObservedWithFunctionTables" \
    "$ITERATION_FACTS|SemanticAstIterationTypeFactsFromArtifactWithFunctionTables" \
    "$STATEMENT_FACTS|SemanticAstStatementTypeFactsFromArtifact"; do
    path="${consumer_contract%%|*}"
    function_name="${consumer_contract#*|}"
    function_body "$path" "$function_name" |
        grep -Fq 'SemanticAstExpressionEnvironmentClear(names, types, modes);' || {
        echo "[self-host-parity:semantic-environment-lifetime] missing last-consumer cleanup in $function_name" >&2
        exit 1
    }
done

for reuse_contract in \
    "$CALL_TARGETS|SemanticAstAnalysisResolveCallTargetsFromBody" \
    "$INITIALIZER_CURSOR|SemanticAstInitializerEnvironmentCursorAdvance"; do
    path="${reuse_contract%%|*}"
    function_name="${reuse_contract#*|}"
    function_body "$path" "$function_name" |
        grep -Fq 'SemanticAstExpressionEnvironmentReset(names, types, modes);' || {
        echo "[self-host-parity:semantic-environment-lifetime] growing environment backing is not reused in $function_name" >&2
        exit 1
    }
done

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
grep -Fq 'func RewriteSemanticOwnedStringArrayCall(' "$CODEGEN_CALL_EMITTER"
grep -Fq '"Array<String>", "inout"' "$CODEGEN_CALL_EMITTER"
grep -Fq 'CollectionRuntimeCOwnedStringPushFn()' "$CODEGEN_CALL_EMITTER"
grep -Fq 'CollectionRuntimeCOwnedStringDropFn()' "$CODEGEN_CALL_EMITTER"

echo "[self-host-parity:semantic-environment-lifetime] borrowed environment reset and owned table cleanup are owner-directed"
