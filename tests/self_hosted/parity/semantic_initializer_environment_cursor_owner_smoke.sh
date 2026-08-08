#!/usr/bin/env bash
# Ratchets the initializer-only sequential visible-local environment cursor.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CURSOR="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_environment_cursor_owner.pgy"
INITIALIZER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
PROBE="$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe/main.pgy"
PARITY="$ROOT_DIR/tests/self_hosted/parity/initializer_projection_probe_parity.sh"
LABEL="self-host-parity:initializer-environment-cursor"

fail() {
    echo "[$LABEL] $*" >&2
    exit 1
}

for path in "$CURSOR" "$INITIALIZER" "$PROBE" "$PARITY"; do
    [[ -f "$path" ]] || fail "missing $path"
done

function_body() {
    local path="$1"
    local function_name="$2"
    sed -n "/func ${function_name}(/,/^}/p" "$path"
}

# Repointed: the admitted spelling is the production route; the un-admitted
# name is now a delegating bridge without the cursor walk.
initializer_body="$(function_body \
    "$INITIALIZER" \
    'SemanticAstInitializerTypeFactsFromAdmittedArtifactWithIterationRowsObservedWithFunctionTables')"
[[ -n "$initializer_body" ]] || fail "initializer production body is missing"

for required in \
    'SemanticAstInitializerEnvironmentCursorEmpty(' \
    'SemanticAstInitializerEnvironmentCursorAdvance(' \
    'SemanticAstInitializerEnvironmentTruncateTransient(' \
    'SemanticAstInitializerEnvironmentCursorCommitCompletedNode('; do
    grep -Fq "$required" <<<"$initializer_body" ||
        fail "initializer production route lost $required"
done

for forbidden in \
    'SemanticAstExpressionSeedVisibleLocals(' \
    'SemanticAstExpressionSeedVisibleLocalModes(' \
    'SemanticAstLocalBindingRangeForFunction('; do
    if grep -Fq "$forbidden" <<<"$initializer_body"; then
        fail "initializer production route restored full-range fallback: $forbidden"
    fi
done

advance_body="$(function_body \
    "$CURSOR" 'SemanticAstInitializerEnvironmentCursorAdvance')"
commit_body="$(function_body \
    "$CURSOR" 'SemanticAstInitializerEnvironmentCursorCommitCompletedNode')"
truncate_body="$(function_body \
    "$CURSOR" 'SemanticAstInitializerEnvironmentTruncateTransient')"

[[ -n "$advance_body" && -n "$commit_body" && -n "$truncate_body" ]] ||
    fail "cursor owner API is incomplete"

for required in \
    'SemanticAstInitializerEnvironmentCursorShapeReady(' \
    'function_node_id <= cursor.function_node_id' \
    'use_node_id < cursor.last_node_id' \
    'cursor.blocked_scope_node_id' \
    'SemanticAstExpressionScopeVisible(' \
    'ArrayPop(active_local_rows);' \
    'ArrayPop(names);' \
    'ArrayPop(types);' \
    'ArrayPop(modes);'; do
    grep -Fq "$required" <<<"$advance_body" ||
        fail "cursor advance invariant is missing: $required"
done

for required in \
    'locals.node_ids[row_index + 1] == locals.node_ids[row_index]' \
    'locals.node_ids[group_start - 1] == locals.node_ids[row_index]' \
    'locals.scope_node_ids[i] != locals.scope_node_ids[row_index]' \
    'if !publishable {' \
    'cursor.blocked_scope_node_id = locals.scope_node_ids[row_index];' \
    'ArrayPush(names, locals.names[i]);' \
    'ArrayPush(types, type_name);' \
    'ArrayPush(modes, "local");' \
    'ArrayPush(active_local_rows, i);'; do
    grep -Fq "$required" <<<"$commit_body" ||
        fail "completed-node atomic publication is missing: $required"
done

for required in \
    'while ArrayLength(names) > persistent_count' \
    'ArrayPop(names);' \
    'ArrayPop(types);' \
    'ArrayPop(modes);'; do
    grep -Fq "$required" <<<"$truncate_body" ||
        fail "transient environment restoration is missing: $required"
done

if grep -Fq 'ArrayPushOwnedString' "$CURSOR"; then
    fail "cursor copied a borrowed semantic environment row"
fi
if grep -Fq 'ArrayDropOwnedStrings' "$CURSOR"; then
    fail "cursor attempted to free live borrowed semantic rows"
fi
if grep -Fq 'SemanticAstLocalBindingRangeForFunction' "$CURSOR"; then
    fail "cursor restored a full-function local range scan"
fi

for fixture in \
    '--cursor-outer-shadow-positive' \
    '--cursor-nested-exit-positive' \
    '--cursor-destructure-atomic-positive' \
    '--cursor-self-reference' \
    '--cursor-sibling-leak'; do
    grep -Fq -- "$fixture" "$PROBE" ||
        fail "executable cursor fixture is missing: $fixture"
    grep -Fq -- "${fixture#--}" "$PARITY" ||
        fail "C/LLVM cursor parity route is missing: $fixture"
done

echo "[$LABEL] initializer locals advance once and publish completed syntax nodes atomically"
