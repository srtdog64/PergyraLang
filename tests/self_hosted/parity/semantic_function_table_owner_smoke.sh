#!/usr/bin/env bash
# Owns the body-analysis callable-table lifetime boundary.
# Forbidden fallback: rebuilding the same function/constructor/enum table in
# each initializer, iteration, refinement, or call-target consumer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"
FACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy"
BODY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
INIT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
BRIDGE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_function_table_bridge_owner.pgy"
ITER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy"
REFINE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_iteration_refinement_owner.pgy"
CALL_TARGET_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy"

fail() {
    echo "[self-host-parity:semantic-function-table-owner] $*" >&2
    exit 1
}

for file in "$ENV_OWNER" "$FACT_OWNER" "$BODY_OWNER" "$INIT_OWNER" "$BRIDGE_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER"; do
    [[ -f "$file" ]] || fail "owner is missing: ${file#$ROOT_DIR/}"
done

grep -Fq 'struct SemanticAstExpressionFunctionTableFacts' "$FACT_OWNER" ||
    fail "callable-table fact owner is missing"
grep -Fq 'func SemanticAstExpressionFunctionTableFactsFromArtifact' "$ENV_OWNER" ||
    fail "callable-table producer is missing"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady' "$FACT_OWNER" ||
    fail "callable-table readiness gate is missing"

for consumer in "$INIT_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER"; do
    if grep -Fq 'SemanticAstExpressionFunctionTables(' "$consumer"; then
        fail "consumer rebuilds callable tables: ${consumer#$ROOT_DIR/}"
    fi
done

grep -Fq 'SemanticAstExpressionFunctionTableFactsFromArtifact(' "$BODY_OWNER" ||
    fail "body owner does not produce the shared callable-table fact"
for consumer in \
    'SemanticAstInitializerTypeFactsFromArtifactObservedWithFunctionTables(' \
    'SemanticAstIterationTypeFactsFromArtifactWithFunctionTables(' \
    'SemanticAstInitializerTypeFactsRefinedByIterationsWithFunctionTables('; do
    grep -Fq "$consumer" "$BODY_OWNER" ||
        fail "body owner does not route shared fact to: $consumer"
done

grep -Fq 'SemanticAstAnalysisResolveCallTargetsFromBody(' "$BODY_OWNER" ||
    fail "body owner does not route shared fact to call-target resolver"
grep -Fq 'iteration_types, function_tables' "$BODY_OWNER" ||
    fail "call-target resolver does not receive shared callable-table fact"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady(function_tables)' "$CALL_TARGET_OWNER" ||
    fail "call-target resolver does not fail closed on callable-table fact"

grep -Fq 'function_tables' "$INIT_OWNER" ||
    fail "initializer owner does not consume the shared fact"
grep -Fq 'function_tables' "$ITER_OWNER" ||
    fail "iteration owner does not consume the shared fact"
grep -Fq 'function_tables' "$REFINE_OWNER" ||
    fail "refinement owner does not consume the shared fact"

echo "[self-host-parity:semantic-function-table-owner] body analysis shares one callable-table fact"
