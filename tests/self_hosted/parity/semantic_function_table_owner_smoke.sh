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
DRIVER_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
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
BUILTIN_OWNER="$ROOT_DIR/src/self_hosted/semantic/builtin_signature_owner.pgy"

fail() {
    echo "[self-host-parity:semantic-function-table-owner] $*" >&2
    exit 1
}

for file in "$ENV_OWNER" "$FACT_OWNER" "$ANALYSIS_OWNER" "$CAPTURE_OWNER" "$BODY_OWNER" "$DRIVER_OWNER" "$INIT_OWNER" "$BRIDGE_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER" "$ASSIGNMENT_OWNER" "$STATEMENT_OWNER" "$GENERIC_OWNER" "$PLACE_OWNER" "$MATCH_OWNER" "$BUILTIN_OWNER"; do
    [[ -f "$file" ]] || fail "owner is missing: ${file#$ROOT_DIR/}"
done

grep -Fq 'struct SemanticAstExpressionFunctionTableFacts' "$FACT_OWNER" ||
    fail "callable-table fact owner is missing"
grep -Fq 'func SemanticAstExpressionFunctionTableFactsFromArtifact' "$ENV_OWNER" ||
    fail "callable-table producer is missing"
grep -Fq 'SemanticAstExpressionFunctionTableFactsReady' "$FACT_OWNER" ||
    fail "callable-table readiness gate is missing"
grep -Fq 'func SemanticAstExpressionFunctionTableFactsRelease' "$FACT_OWNER" ||
    fail "callable-table release owner is missing"
grep -Fq 'facts.ok = false;' "$FACT_OWNER" ||
    fail "callable-table release does not fail closed"
if grep -Fq 'ArrayDropOwnedStrings(facts.' "$FACT_OWNER"; then
    fail "callable-table release passes a struct field across the inout boundary"
fi
grep -Fq 'facts.names = names;' "$FACT_OWNER" ||
    fail "callable-table release does not publish the dropped names field"
grep -Fq 'facts.returns = returns;' "$FACT_OWNER" ||
    fail "callable-table release does not publish the dropped returns field"
grep -Fq 'facts.params = params;' "$FACT_OWNER" ||
    fail "callable-table release does not publish the dropped params field"
grep -Fq 'SemanticAstExpressionFunctionTableFactsFromArtifact(' "$ANALYSIS_OWNER" ||
    fail "artifact analysis does not own callable-table production"

table_body="$(awk '/^func SemanticAstExpressionFunctionTables\(/ { found=1 } found { print } /^func SemanticAstExpressionFunctionTableFactsFromArtifact\(/ { exit }' "$ENV_OWNER")"
if grep -Eq 'ArrayPush\((names|returns|params)' <<<"$table_body"; then
    fail "callable-table producer retains ordinary String-array insertion"
fi
grep -Fq 'func SeedSemanticOwnedBuiltinSignatures' "$BUILTIN_OWNER" ||
    fail "owned builtin table seed is missing"
grep -Fq 'SeedSemanticOwnedBuiltinSignatures(names, returns, params)' "$ENV_OWNER" ||
    fail "callable-table producer does not request owned builtin rows"

for consumer in "$CAPTURE_OWNER" "$BODY_OWNER" "$INIT_OWNER" "$ITER_OWNER" "$REFINE_OWNER" "$CALL_TARGET_OWNER" "$ASSIGNMENT_OWNER" "$STATEMENT_OWNER" "$GENERIC_OWNER" "$PLACE_OWNER" "$MATCH_OWNER"; do
    if grep -Fq 'SemanticAstExpressionFunctionTables(' "$consumer"; then
        fail "consumer rebuilds callable tables: ${consumer#$ROOT_DIR/}"
    fi
done

grep -Fq 'analysis.function_tables' "$BODY_OWNER" ||
    fail "body owner does not consume the artifact-owned callable-table fact"
grep -Fq 'function_tables: SemanticAstExpressionFunctionTableFacts' "$CAPTURE_OWNER" ||
    fail "call-target capture does not consume the shared callable-table fact"
# Repointed: the body owner now routes through the admission-first spellings;
# the shared callable-table subject still rides the WithFunctionTables suffix.
for consumer in \
    'SemanticAstInitializerTypeFactsFromAdmittedArtifactObservedWithFunctionTables(' \
    'SemanticAstIterationTypeFactsFromAdmittedArtifactWithFunctionTables(' \
    'SemanticAstInitializerTypeFactsRefinedByIterationsFromAdmittedFactsWithFunctionTables('; do
    grep -Fq "$consumer" "$BODY_OWNER" ||
        fail "body owner does not route shared fact to: $consumer"
done

grep -Fq 'SemanticAstAnalysisResolveCallTargetsFromAdmittedBody(' "$BODY_OWNER" ||
    fail "body owner does not route shared fact to call-target resolver"
grep -Fq 'iteration_types, function_tables' "$BODY_OWNER" ||
    fail "call-target resolver does not receive shared callable-table fact"
# Repointed: the admitted assignment/statement resolvers gained
# function_scopes and their argument lists wrap, so the pins read the fact
# inside each resolver's own call window instead of one spliced line.
grep -A 8 -F 'SemanticAstAssignmentTypeFactsFromAdmittedArtifact(' \
    "$BODY_OWNER" | grep -Fq 'function_tables' ||
    fail "assignment resolver does not receive shared callable-table fact"
grep -A 8 -F 'SemanticAstStatementTypeFactsFromAdmittedArtifact(' \
    "$BODY_OWNER" | grep -Fq 'function_tables' ||
    fail "statement resolver does not receive shared callable-table fact"
grep -Fq 'analysis.expression_surfaces, function_tables' "$BODY_OWNER" ||
    fail "generic resolver does not receive shared callable-table fact"
if grep -Fq 'SemanticAstExpressionFunctionTableFactsRelease' "$BODY_OWNER"; then
    fail "body owner releases callable-table data before graph/MIR consumers finish"
fi
grep -Fq 'let terminal_analysis: SemanticAstArtifactAnalysis = projection.analysis;' "$DRIVER_OWNER" ||
    fail "driver does not bind the terminal semantic analysis"
grep -Fq 'let function_tables: SemanticAstExpressionFunctionTableFacts =' "$DRIVER_OWNER" ||
    fail "driver does not bind the terminal callable-table fact"
grep -Fq 'SemanticAstExpressionFunctionTableFactsRelease(function_tables);' "$DRIVER_OWNER" ||
    fail "driver does not release the callable-table fact"
grep -Fq 'terminal_analysis.function_tables = function_tables;' "$DRIVER_OWNER" ||
    fail "driver does not publish the released callable-table fact"
grep -Fq 'projection.analysis = terminal_analysis;' "$DRIVER_OWNER" ||
    fail "projection release does not publish the terminal semantic analysis"
grep -Fq 'DriverRung2MirProjectionRelease(projection);' "$DRIVER_OWNER" ||
    fail "driver does not release the terminal MIR projection"
release_line="$(grep -n 'DriverRung2MirProjectionRelease(projection);' "$DRIVER_OWNER" | tail -n 1 | cut -d: -f1)"
json_done_line="$(grep -n 'json:done' "$DRIVER_OWNER" | tail -n 1 | cut -d: -f1)"
if [[ -z "$release_line" || -z "$json_done_line" || "$release_line" -le "$json_done_line" ]]; then
    fail "callable-table release is not after the final MIR JSON consumer"
fi
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
