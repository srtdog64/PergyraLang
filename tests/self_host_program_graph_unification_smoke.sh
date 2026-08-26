#!/usr/bin/env bash
# Ratchets the self-hosted compiler toward one structural expression graph.
# Before the target lands, three storage owners are the explicit baseline.
# Once it exists, the HIR, semantic, and MIR topology declarations must be
# gone; only the program graph may store node rows.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC="docs/180_compiler_logical_spine_handles_gates.md"
TARGET_OWNER="src/self_hosted/hir/program_graph_owner.pgy"
LEGACY_OWNERS=(
    "src/self_hosted/hir/ast_expression_graph_owner.pgy"
    "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy"
    "src/self_hosted/mir/expression_graph_fact_owner.pgy"
)
LEGACY_TOPOLOGY_OWNERS=(
    "src/self_hosted/hir/ast_expression_graph_owner.pgy"
    "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy"
)
POST_REPOINT_OWNERS=(
    "$TARGET_OWNER"
)

fail() {
    echo "[self-host-program-graph-unification] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

reject_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel retained forbidden term: $term"
    fi
}

is_structural_expression_store() {
    local rel="$1"
    local path="$ROOT_DIR/$rel"
    grep -Fq -- "node_kinds: Array<Int>;" "$path" &&
        grep -Fq -- "node_texts: Array<String>;" "$path" &&
        grep -Fq -- "left_children: Array<Int>;" "$path" &&
        grep -Fq -- "right_children: Array<Int>;" "$path"
}

is_allowed_storage_owner() {
    local rel="$1"
    if [[ "$rel" == "$TARGET_OWNER" ]]; then
        return 0
    fi
    local legacy=""
    for legacy in "${LEGACY_OWNERS[@]}"; do
        if [[ "$rel" == "$legacy" ]]; then
            return 0
        fi
    done
    return 1
}

require_file "$DOC"
for term in \
    "## 2.1 Self-Hosted Program Graph Unification Contract" \
    'Target structural owner: `src/self_hosted/hir/program_graph_owner.pgy`.' \
    "one immutable expression topology" \
    "typed overlays" \
    "CompilationRevisionId" \
    "ExpressionNodeId" \
    "AIR remains a verifier" \
    '`selfhost.expression_graph` semantic authority' \
    "docs/semantics/boundary_migration_manifest.md" \
    "semantic topology repointed" \
    "Forbidden fallback" \
    "Program Graph Unification Gate"; do
    require_text "$DOC" "$term"
done

require_text "Makefile" "self-host-program-graph-unification-test-smoke:"
require_text "Makefile" '"$(BASH)" tests/self_host_program_graph_unification_smoke.sh'
require_text "Makefile" '"$(BASH)" tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh'
make_wiring_count="$(grep -Fc -- '"$(BASH)" tests/self_host_program_graph_unification_smoke.sh' "$ROOT_DIR/Makefile")"
[[ "$make_wiring_count" -eq 2 ]] ||
    fail "expected target and preparation wiring in Makefile, got $make_wiring_count"
projection_wiring_count="$(grep -Fc -- '"$(BASH)" tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh' "$ROOT_DIR/Makefile")"
[[ "$projection_wiring_count" -eq 2 ]] ||
    fail "expected MIR projection target and preparation wiring in Makefile, got $projection_wiring_count"
require_text "src/self_hosted/compiler/gate_dashboard_owner.pgy" \
    '"program_graph_unification"'
require_text "src/self_hosted/compiler/gate_dashboard_owner.pgy" \
    '"self-host-program-graph-unification-test-smoke"'
require_text "src/self_hosted/compiler/gate_dashboard_owner.pgy" \
    '"selfhost.expression_graph"'
require_text "$TARGET_OWNER" "func ProgramExpressionGraphStorageSchema"
require_text "$TARGET_OWNER" \
    "func ProgramExpressionGraphStorageRowsAligned"
require_text "src/self_hosted/hir/ast_expression_graph_owner.pgy" \
    'import "program_graph_owner.pgy";'
require_text "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy" \
    "topology: AstExpressionArena;"
require_text "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy" \
    "SemanticExpressionGraphArenaFromTopology("
semantic_graph_compact="$(tr -d '[:space:]' < \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy")"
[[ "$semantic_graph_compact" == *"SemanticExpressionGraphArenaFromTopology(rows.arena,texts,call_target_kinds,call_target_names,call_return_type_names,place_kinds)"* ]] ||
    fail "semantic expression graph no longer preserves topology plus typed overlays"
for forbidden_copy in \
    "ArrayPush(kinds, rows.arena.node_kinds[i]);" \
    "ArrayPush(left_children, rows.arena.left_children[i]);" \
    "ArrayPush(right_children, rows.arena.right_children[i]);"; do
    reject_text "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy" \
        "$forbidden_copy"
done
require_text "src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy" \
    "SemanticExpressionGraphArenaFromTopologyWithIdentities("
reject_text "src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy" \
    "SemanticExpressionGraphArenaFromTopology("
reject_text "src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy" \
    "SemanticExpressionGraphArenaFromRows("

cd "$ROOT_DIR"
structural_owners=()
while IFS= read -r rel; do
    if is_structural_expression_store "$rel"; then
        structural_owners+=("$rel")
    fi
done < <(
    grep -RlF --include='*.pgy' -- "node_kinds: Array<Int>;" src/self_hosted |
        tr '\\' '/'
)

if [[ "${#structural_owners[@]}" -eq 0 ]]; then
    fail "no structural expression graph owner found"
fi

owner=""
for owner in "${structural_owners[@]}"; do
    if ! is_allowed_storage_owner "$owner"; then
        fail "unregistered structural expression graph owner: $owner"
    fi
done

if [[ ! -f "$ROOT_DIR/$TARGET_OWNER" ]]; then
    if [[ "${#structural_owners[@]}" -ne "${#LEGACY_OWNERS[@]}" ]]; then
        fail "target absent but legacy baseline changed: expected ${#LEGACY_OWNERS[@]}, got ${#structural_owners[@]}"
    fi
    for owner in "${LEGACY_OWNERS[@]}"; do
        is_structural_expression_store "$owner" ||
            fail "target absent and legacy structural owner is missing: $owner"
    done
    phase="baseline"
else
    is_structural_expression_store "$TARGET_OWNER" ||
        fail "target owner exists without the structural graph contract: $TARGET_OWNER"
    if [[ "${#structural_owners[@]}" -ne "${#POST_REPOINT_OWNERS[@]}" ]]; then
        fail "program graph unification requires exactly ${#POST_REPOINT_OWNERS[@]} structural owners, got ${#structural_owners[@]}"
    fi
    for owner in "${POST_REPOINT_OWNERS[@]}"; do
        is_structural_expression_store "$owner" ||
            fail "semantic topology repointing is missing owner: $owner"
    done
    for owner in "${LEGACY_TOPOLOGY_OWNERS[@]}"; do
        if is_structural_expression_store "$owner"; then
            fail "semantic_topology_copy remains in retired owner: $owner"
        fi
    done
    phase="unified"
fi

echo "[self-host-program-graph-unification] phase=$phase structural_owners=${#structural_owners[@]} target=$TARGET_OWNER"
