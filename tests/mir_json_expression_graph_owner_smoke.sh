#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORCHESTRATOR="$ROOT_DIR/src/compiler/mir_json_expression_graph.c"
MATERIALIZER="$ROOT_DIR/src/compiler/mir_json_expression_graph_materialize.c"
MATERIALIZER_HEADER="$ROOT_DIR/src/compiler/mir_json_expression_graph_materialize.h"

fail() {
    echo "[mir-json-expression-graph-owner] $*" >&2
    exit 1
}

for owner in "$ORCHESTRATOR" "$MATERIALIZER" "$MATERIALIZER_HEADER"; do
    [[ -f "$owner" ]] || fail "missing owner: $owner"
done

for owner in "$ORCHESTRATOR" "$MATERIALIZER"; do
    lines="$(wc -l < "$owner")"
    (( lines <= 699 )) || fail "owner exceeds 699 LOC: $owner ($lines)"
done

for required in \
    "mir_json_instruction_expression(const MIRInstruction *inst, int lane)" \
    "mir_expression_graph_digest(" \
    "mir_expression_graph_identity(" \
    "mir_json_emit_instruction_expression_graph(" \
    "if (root < 0)" \
    "return false;" \
    'fputs("null", out);'; do
    grep -Fq -- "$required" "$ORCHESTRATOR" ||
        fail "orchestration/identity term missing: $required"
done

for required in \
    "mir_json_expression_graph_build(" \
    "mir_json_expression_graph_build_call(" \
    "mir_json_expression_graph_build_struct(" \
    "case AST_ARRAY_LITERAL:" \
    "if (type == TOKEN_QUESTION)" \
    "generic_type_actual" \
    "generic_callee" \
    "type_name" \
    "default:" \
    "return -1;"; do
    grep -Fq -- "$required" "$MATERIALIZER" ||
        fail "materialization term missing: $required"
done

for required in \
    "MIRJsonExpressionGraphNode" \
    "MIRJsonExpressionGraph" \
    "mir_json_expression_graph_dispose(" \
    "mir_json_expression_graph_build("; do
    grep -Fq -- "$required" "$MATERIALIZER_HEADER" ||
        fail "materialization contract term missing: $required"
done

for forbidden in \
    "mir_json_expression_graph_build_call(" \
    "case AST_ARRAY_LITERAL:"; do
    if grep -Fq -- "$forbidden" "$ORCHESTRATOR"; then
        fail "orchestrator re-owned graph materialization: $forbidden"
    fi
done

for forbidden in \
    "mir_expression_graph_digest(" \
    "mir_json_emit_instruction_expression_graph("; do
    if grep -Fq -- "$forbidden" "$MATERIALIZER"; then
        fail "materializer re-owned identity/emission: $forbidden"
    fi
done

for owner in "$ORCHESTRATOR" "$MATERIALIZER"; do
    for forbidden in "parser_parse" "ParseExpr"; do
        if grep -Fq -- "$forbidden" "$owner"; then
            fail "text/parser fallback reopened in $owner: $forbidden"
        fi
    done
done

grep -Fq -- '$(COMPILER_DIR)/mir_json_expression_graph_materialize.c' "$ROOT_DIR/Makefile" ||
    fail "materialization source missing from Makefile"
grep -Fq -- '$(BUILD_DIR)/compiler/mir_json_expression_graph_materialize.o' "$ROOT_DIR/Makefile" ||
    fail "materialization object missing from MIR core link"

echo "[mir-json-expression-graph-owner] materialization and identity owners are closed"
