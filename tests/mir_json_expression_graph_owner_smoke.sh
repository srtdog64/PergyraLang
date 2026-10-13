#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORCHESTRATOR="$ROOT_DIR/src/compiler/mir_json_expression_graph.c"
MATERIALIZER="$ROOT_DIR/src/compiler/mir_json_expression_graph_materialize.c"

fail() {
    echo "[mir-json-expression-graph-owner] $*" >&2
    exit 1
}

for owner in "$ORCHESTRATOR" "$MATERIALIZER"; do
    [[ -f "$owner" ]] || fail "missing owner: $owner"
done

for owner in "$ORCHESTRATOR" "$MATERIALIZER"; do
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$owner")" || fail "source-size measurement failed"
    (( lines <= 699 )) || fail "owner exceeds 699 LOC: $owner ($lines)"
done

echo "[mir-json-expression-graph-owner] owner files present within the 699 LOC cap"
