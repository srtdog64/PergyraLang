#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/expression_graph_fact_owner.pgy"
BODY="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.body"
PREFLIGHT="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.preflight"
trap 'rm -f "$BODY" "$PREFLIGHT"' EXIT

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:mir-expression-graph-projection] owner is missing" >&2
    exit 1
}

awk '
    /^func SelfMirExpressionGraphAttachLast\(/ { inside=1 }
    inside { print }
    inside && /^func / && !/SelfMirExpressionGraphAttachLast/ { exit }
' "$OWNER" >"$BODY"

awk '
    /let has_roots: Array<Int> = rows.has_roots;/ { exit }
    { print }
' "$BODY" >"$PREFLIGHT"

for required in \
    'SelfMirExpressionGraphSubtreeStart(' \
    'SemanticExpressionGraphFactsEqual(' \
    'ArraySet(roots, instruction_index, view.root_id)' \
    'ArraySet(starts, instruction_index, source_start)' \
    'ArraySet(counts, instruction_index, copied_count)'; do
    grep -Fq "$required" "$BODY" || {
        echo "[self-host-parity:mir-expression-graph-projection] missing handle operation: $required" >&2
        exit 1
    }
done

for forbidden in \
    'let kinds: Array<Int>' \
    'let texts: Array<String>' \
    'ArrayPush(kinds,' \
    'ArrayPush(texts,' \
    'rows.node_kinds' \
    'rows.node_texts' \
    'rows.left_children' \
    'rows.right_children' \
    'rows.call_target_kinds' \
    'rows.call_target_names'; do
    if grep -Fq "$forbidden" "$BODY"; then
        echo "[self-host-parity:mir-expression-graph-projection] copied MIR topology reopened: $forbidden" >&2
        exit 1
    fi
done

if grep -Fq 'ArrayPush(' "$BODY"; then
    echo "[self-host-parity:mir-expression-graph-projection] attachment still materializes node rows" >&2
    exit 1
fi

if grep -Fq 'view.graph.arena.' "$BODY"; then
    echo "[self-host-parity:mir-expression-graph-projection] attachment reads graph storage directly" >&2
    exit 1
fi

grep -Fq 'SemanticExpressionGraphFactsEqual(graph, view.graph)' \
    "$PREFLIGHT" || {
    echo "[self-host-parity:mir-expression-graph-projection] foreign graph is not rejected before mutation" >&2
    exit 1
}
if grep -Fq 'ArraySet(' "$PREFLIGHT"; then
    echo "[self-host-parity:mir-expression-graph-projection] instruction ranges mutate before graph identity preflight" >&2
    exit 1
fi

for migrated_consumer in \
    'src/self_hosted/mir/json_projection_owner.pgy|SemanticExpressionGraphCallTargetKind(rows.graph, node)' \
    'src/self_hosted/mir/program_verify_owner.pgy|SemanticExpressionGraphNodeKind(' \
    'src/self_hosted/tools/initializer_projection_probe/main.pgy|facts.cfg.instructions.expr0_graphs.graph'; do
    rel="${migrated_consumer%%|*}"
    term="${migrated_consumer#*|}"
    grep -Fq "$term" "$ROOT_DIR/$rel" || {
        echo "[self-host-parity:mir-expression-graph-projection] consumer did not migrate: $rel" >&2
        exit 1
    }
done

for retired_read in \
    'rows.node_kinds' \
    'rows.node_texts' \
    'rows.left_children' \
    'rows.right_children' \
    'rows.call_target_kinds' \
    'rows.call_target_names' \
    'expr0_graphs.call_target_kinds' \
    'expr0_graphs.call_target_names'; do
    if rg -F --glob '*.pgy' -- "$retired_read" \
        "$ROOT_DIR/src/self_hosted/mir" \
        "$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe" \
        >/dev/null; then
        echo "[self-host-parity:mir-expression-graph-projection] raw MIR graph read reopened: $retired_read" >&2
        exit 1
    fi
done

echo "[self-host-parity:mir-expression-graph-projection] MIR projection carries semantic graph handles"
