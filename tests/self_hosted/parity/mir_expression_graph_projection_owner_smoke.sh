#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/expression_graph_fact_owner.pgy"
PROGRAM_OWNER="$ROOT_DIR/src/self_hosted/mir/program_fact_owner.pgy"
BODY="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.body"
ROWS_BODY="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.rows"
APPEND_BODY="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.append"
trap 'rm -f "$BODY" "$ROWS_BODY" "$APPEND_BODY"' EXIT

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:mir-expression-graph-projection] owner is missing" >&2
    exit 1
}

awk '
    /^func SelfMirExpressionGraphAttachLast\(/ { inside=1 }
    inside { print }
    inside && /^}/ { exit }
' "$OWNER" >"$BODY"

awk '
    /^struct SelfMirExpressionGraphRows / { inside=1 }
    inside { print }
    inside && /^}/ { exit }
' "$OWNER" >"$ROWS_BODY"

awk '
    /^func SelfMirExpressionGraphRowsAppendRows\(/ { inside=1 }
    inside { print }
    inside && /^}/ { exit }
' "$OWNER" >"$APPEND_BODY"

for required in \
    'SemanticExpressionGraphSubtreeStart(' \
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

if grep -Fq 'graph: SemanticExpressionGraphFacts;' "$ROWS_BODY"; then
    echo "[self-host-parity:mir-expression-graph-projection] instruction rows re-own the program graph" >&2
    exit 1
fi
if grep -Fq 'SemanticExpressionGraphFactsEqual(' "$OWNER"; then
    echo "[self-host-parity:mir-expression-graph-projection] whole-graph equality reintroduced" >&2
    exit 1
fi
grep -Fq 'expression_graph: SemanticExpressionGraphFacts;' \
    "$PROGRAM_OWNER" || {
    echo "[self-host-parity:mir-expression-graph-projection] program graph owner is missing" >&2
    exit 1
}
if grep -Fq 'SemanticExpressionGraphFactsEqual(' "$APPEND_BODY"; then
    echo "[self-host-parity:mir-expression-graph-projection] CFG append revalidates the whole graph" >&2
    exit 1
fi

for migrated_consumer in \
    'src/self_hosted/mir/json_projection_owner.pgy|facts.expression_graph' \
    'src/self_hosted/mir/program_verify_owner.pgy|facts.expression_graph' \
    'src/self_hosted/tools/initializer_projection_probe/main.pgy|facts.expression_graph' \
    'src/self_hosted/tools/machine_layer_mir_projection_probe/main.pgy|MachineProbeProgramExpressionGraph()' \
    'src/self_hosted/tools/machine_layer_mir_projection_probe/main.pgy|routines, cfg5, SelfMirInstructionAbiReceiptRowsAppend(' \
    'src/self_hosted/tools/machine_layer_mir_projection_probe/main.pgy|), program_graph.graph'; do
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
    if grep -RF --include='*.pgy' -- "$retired_read" \
        "$ROOT_DIR/src/self_hosted/mir" \
        "$ROOT_DIR/src/self_hosted/tools/initializer_projection_probe" \
        >/dev/null; then
        echo "[self-host-parity:mir-expression-graph-projection] raw MIR graph read reopened: $retired_read" >&2
        exit 1
    fi
done

echo "[self-host-parity:mir-expression-graph-projection] MIR projection carries semantic graph handles"
