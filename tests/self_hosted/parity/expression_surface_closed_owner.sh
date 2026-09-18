#!/usr/bin/env bash
# CLOSED selfhost.expression_surface fallbacks:
# TypedAstArenaAtomText
# TypedAstArenaValueText
# TypedAstArenaAuxValueText
# ContainsCallOutsideStrings
# CodegenExpressionUsageFactsFromArena
# CodegenAstArenaExpressionPartsAt
# FindTopLevelPlus
# RewriteBool
# RewriteIndexing
# RewriteInoutCallArgs
# ExprSequenceItemAt
# generic_return_text_inference
# codegen_generic_graph_rescan
# expr_semantic_shape_emit_owner
# callable_type_from_text
# callable_target_name_dispatch
# SemanticAstIntentExpressionOwnerNodeId
# SemanticAstExpressionVerdictFromPayload
# expression_graph_text_type_fallback
# codegen_parser_implementation_import

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:expression-surface-closed"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }

AUTHORITY="$ROOT_DIR/src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy"
PARSER_BOUNDARY="$ROOT_DIR/src/self_hosted/parser/ast_text_artifact_parse_owner.pgy"
TYPED_BINDING="$ROOT_DIR/src/self_hosted/semantic/ast_expression_typed_binding_owner.pgy"
VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_artifact_verdict_owner.pgy"
EXPRESSION_VERDICT="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
PROGRAM_ENTRY="$ROOT_DIR/src/self_hosted/codegen/emission/program_entry_owner.pgy"
DRIVER_PIPELINE="$ROOT_DIR/src/self_hosted/compiler/driver_pipeline_owner.pgy"
PROBE="$ROOT_DIR/src/self_hosted/tools/expression_surface_closure_probe/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/expression_surface_closed}"

for path in "$AUTHORITY" "$PARSER_BOUNDARY" "$TYPED_BINDING" "$VERDICT" \
    "$EXPRESSION_VERDICT" \
    "$PROGRAM_ENTRY" "$DRIVER_PIPELINE" "$PROBE"; do
    [[ -f "$path" ]] || { echo "[$LABEL] required owner is missing: $path" >&2; exit 1; }
done

for retired_verdict_term in \
    'SemanticAstExpressionVerdictFromPayload' \
    'CheckCallFromGraphIdentifiers(' \
    'CheckTryOperand(' \
    'CheckLogicalOperands(' \
    'CheckBinaryOperands(' \
    'ExprType('; do
    if grep -Fq "$retired_verdict_term" "$EXPRESSION_VERDICT"; then
        echo "[$LABEL] graph verdict reopened text fallback: $retired_verdict_term" >&2
        exit 1
    fi
done
grep -Fq '"expression_graph_type"' "$EXPRESSION_VERDICT" || {
    echo "[$LABEL] unowned graph type is not fail-closed" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionGraphTypeFailClosedContractReady()' "$PROBE" || {
    echo "[$LABEL] executable unowned-graph negative contract is missing" >&2
    exit 1
}

for retired in \
    src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy \
    src/self_hosted/codegen/emission/expr_rewrite.pgy \
    src/self_hosted/codegen/emission/literal_rewrite.pgy \
    src/self_hosted/codegen/emission/struct_value_emit.pgy \
    src/self_hosted/codegen/text/expr_scan.pgy \
    src/self_hosted/codegen/text/expr_sequence_owner.pgy \
    src/self_hosted/codegen/text/struct_field_access_owner.pgy \
    src/self_hosted/codegen/text/struct_literal_call_owner.pgy \
    src/self_hosted/codegen/text/struct_literal_field_owner.pgy; do
    [[ ! -e "$ROOT_DIR/$retired" ]] || {
        echo "[$LABEL] retired text owner returned: $retired" >&2
        exit 1
    }
done

for retired_term in \
    SemanticAstArtifactAnalyzeCompactBridge \
    SemanticAstExpressionSurfaceFactsFromArtifactCompactBridge \
    SemanticExpressionGraphBuildCompactBridgeFromText; do
    if grep -R -Fq --include='*.pgy' "$retired_term" "$ROOT_DIR/src/self_hosted"; then
        echo "[$LABEL] retired compact expression bridge returned: $retired_term" >&2
        exit 1
    fi
done

if grep -R -n -E --include='*.pgy' \
    '^[[:space:]]*import .*parser/' "$ROOT_DIR/src/self_hosted/codegen"; then
    echo "[$LABEL] codegen reopened a parser implementation import" >&2
    exit 1
fi

grep -Fq 'func ParserSerializedExpressionFact(' "$PARSER_BOUNDARY" || {
    echo "[$LABEL] serialized expression grammar is not parser-owned" >&2
    exit 1
}
grep -Fq 'func ParserAstTreeArtifactFromText(' "$PARSER_BOUNDARY" || {
    echo "[$LABEL] serialized AST does not bind parser-owned graphs" >&2
    exit 1
}
grep -Fq 'artifact.expression_graphs' "$TYPED_BINDING" || {
    echo "[$LABEL] semantic binding ignores parser-owned graph rows" >&2
    exit 1
}
grep -Fq 'SemanticAstExpressionSurfaceFactsFromTypedArtifact(artifact)' "$VERDICT" || {
    echo "[$LABEL] semantic verdict bypasses typed expression binding" >&2
    exit 1
}
grep -Fq 'SemanticAstArtifactAnalyzeTyped(artifact, require_entrypoint)' "$PROGRAM_ENTRY" || {
    echo "[$LABEL] production codegen bypasses typed expression admission" >&2
    exit 1
}
grep -Fq 'SemanticAstArtifactAnalyzeTyped(artifact, true)' "$DRIVER_PIPELINE" || {
    echo "[$LABEL] compiler pipeline bypasses typed expression admission" >&2
    exit 1
}
for forbidden_parse in 'ParseExprFact(' 'ParserSerializedExpressionFact(' \
    'SemanticExpressionGraphImportSerializedParserFact('; do
    if grep -Fq "$forbidden_parse" "$AUTHORITY"; then
        echo "[$LABEL] semantic authority reparses expression spelling: $forbidden_parse" >&2
        exit 1
    fi
done

fallbacks=(
    TypedAstArenaAtomText TypedAstArenaValueText TypedAstArenaAuxValueText
    ContainsCallOutsideStrings CodegenExpressionUsageFactsFromArena
    CodegenAstArenaExpressionPartsAt FindTopLevelPlus RewriteBool
    RewriteIndexing RewriteInoutCallArgs ExprSequenceItemAt
    generic_return_text_inference codegen_generic_graph_rescan
    expr_semantic_shape_emit_owner callable_type_from_text
    callable_target_name_dispatch SemanticAstIntentExpressionOwnerNodeId
    SemanticAstExpressionVerdictFromPayload expression_graph_text_type_fallback
    codegen_parser_implementation_import
)
while IFS= read -r consumer; do
    [[ -n "$consumer" ]] || continue
    [[ "$consumer" == "$AUTHORITY" ]] && continue
    for fallback in "${fallbacks[@]}"; do
        if grep -Fq "$fallback" "$consumer"; then
            echo "[$LABEL] canonical-fact consumer reopened $fallback: ${consumer#"$ROOT_DIR/"}" >&2
            exit 1
        fi
    done
done < <(grep -R -l -F --include='*.pgy' --include='*.c' --include='*.h' \
    'SemanticAstExpressionSurfaceFacts' "$ROOT_DIR/src" || true)

mkdir -p "$BUILD_DIR"
TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PROBE")"
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"
grep -Fq 'expression-surface-closure=parser-owned-fail-closed' \
    "$BUILD_DIR/main_c_leg.out" || {
    echo "[$LABEL] executable closure receipt is missing" >&2
    exit 1
}

echo "[$LABEL] parser-owned graph carriage, missing-fact rejection, and C/LLVM contract parity: PASS"
