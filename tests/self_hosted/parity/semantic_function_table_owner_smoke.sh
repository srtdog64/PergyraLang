#!/usr/bin/env bash
# Owns the body-analysis callable-table lifetime boundary.
# Forbidden fallback: rebuilding the same function/constructor/enum table in
# each initializer, iteration, refinement, call-target, assignment, statement,
# generic-specialization, expression-place, or match-binding consumer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"
FACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy"
ANALYSIS_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_artifact_verdict_owner.pgy"
CAPTURE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy"
BODY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy"
INIT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
BRIDGE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_function_table_bridge_owner.pgy"
ITER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy"
REFINE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_iteration_refinement_owner.pgy"
CALL_TARGET_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy"
ASSIGNMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
STATEMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
GENERIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy"
PLACE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_place_fact_owner.pgy"
MATCH_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_match_binding_environment_owner.pgy"

fail() {
    echo "[self-host-parity:semantic-function-table-owner] $*" >&2
    exit 1
}

for file in "$ENV_OWNER" "$FACT_OWNER" "$ANALYSIS_OWNER" "$CAPTURE_OWNER" "$BODY_OWNER" "$INIT_OWNER" "$BRIDGE_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER" "$ASSIGNMENT_OWNER" "$STATEMENT_OWNER" "$GENERIC_OWNER" "$PLACE_OWNER" "$MATCH_OWNER"; do
    [[ -f "$file" ]] || fail "owner is missing: ${file#$ROOT_DIR/}"
done

grep -Fq 'struct SemanticAstExpressionFunctionTableFacts' "$FACT_OWNER" ||
    fail "callable-table fact owner is missing"
grep -Fq 'func SemanticAstExpressionFunctionTableFactsFromArtifact' "$ENV_OWNER" ||
    fail "callable-table producer is missing"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady' "$FACT_OWNER" ||
    fail "callable-table readiness gate is missing"
grep -Fq 'SemanticAstExpressionFunctionTableFactsFromArtifact(' "$ANALYSIS_OWNER" ||
    fail "artifact analysis does not own callable-table production"

for consumer in "$CAPTURE_OWNER" "$BODY_OWNER" "$INIT_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER" "$ASSIGNMENT_OWNER" "$STATEMENT_OWNER" "$GENERIC_OWNER" "$PLACE_OWNER" "$MATCH_OWNER"; do
    if grep -Fq 'SemanticAstExpressionFunctionTables(' "$consumer"; then
        fail "consumer rebuilds callable tables: ${consumer#$ROOT_DIR/}"
    fi
done

grep -Fq 'analysis.function_tables' "$BODY_OWNER" ||
    fail "body owner does not consume the artifact-owned callable-table fact"
grep -Fq 'function_tables: SemanticAstExpressionFunctionTableFacts' "$CAPTURE_OWNER" ||
    fail "call-target capture does not consume the shared callable-table fact"
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
grep -Fq 'analysis.assignments, function_tables' "$BODY_OWNER" ||
    fail "assignment resolver does not receive shared callable-table fact"
grep -Fq 'analysis.statements, function_tables' "$BODY_OWNER" ||
    fail "statement resolver does not receive shared callable-table fact"
grep -Fq 'analysis.expression_surfaces, function_tables' "$BODY_OWNER" ||
    fail "generic resolver does not receive shared callable-table fact"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady(function_tables)' "$CALL_TARGET_OWNER" ||
    fail "call-target resolver does not fail closed on callable-table fact"

grep -Fq 'function_tables' "$INIT_OWNER" ||
    fail "initializer owner does not consume the shared fact"
grep -Fq 'function_tables' "$ITER_OWNER" ||
    fail "iteration owner does not consume the shared fact"
grep -Fq 'function_tables' "$REFINE_OWNER" ||
    fail "refinement owner does not consume the shared fact"
grep -Fq 'function_tables' "$ASSIGNMENT_OWNER" ||
    fail "assignment owner does not consume the shared fact"
grep -Fq 'function_tables' "$STATEMENT_OWNER" ||
    fail "statement owner does not consume the shared fact"
grep -Fq 'function_tables' "$GENERIC_OWNER" ||
    fail "generic owner does not consume the shared fact"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady(function_tables)' "$PLACE_OWNER" ||
    fail "expression-place owner does not fail closed on callable-table fact"
grep -Fq 'function_tables' "$MATCH_OWNER" ||
    fail "match-binding owner does not consume the shared fact"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady(function_tables)' "$MATCH_OWNER" ||
    fail "match-binding owner does not fail closed on callable-table fact"

echo "[self-host-parity:semantic-function-table-owner] body analysis shares one callable-table fact"
