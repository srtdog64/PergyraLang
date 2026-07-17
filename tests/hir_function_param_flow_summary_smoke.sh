#!/usr/bin/env bash
# HIR must consume the semantic function-parameter summary snapshot by
# stable SyntaxNodeId and reject malformed or unattachable rows.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HIR_TEST="${HIR_TEST_BIN:-$ROOT_DIR/bin/test_hir}"
if [[ -n "${HIR_TEST_BIN:-}" ]]; then
    : # caller supplied the platform-specific test path
elif [[ ! -x "$HIR_TEST" && -x "$ROOT_DIR/bin/test_hir.exe" ]]; then
    HIR_TEST="$ROOT_DIR/bin/test_hir.exe"
fi

if [[ ! -x "$HIR_TEST" ]]; then
    echo "[hir-function-param-flow] missing HIR test binary: $HIR_TEST" >&2
    exit 1
fi

OUTPUT="$($HIR_TEST)"
[[ "$OUTPUT" == *"HIR carries function parameter flow summaries by stable SyntaxNodeId"* ]]
DRIVER_SOURCE="$(< "$ROOT_DIR/src/compiler/driver_app.c")"
HIR_HEADER="$(< "$ROOT_DIR/src/compiler/hir.h")"
HIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/hir_validate.c")"
[[ "$DRIVER_SOURCE" == *"hir_lower_with_resource_and_param_flow_facts"* ]]
[[ "$HIR_HEADER" == *"function_param_flow_summaries"* ]]
[[ "$HIR_HEADER" == *"parameter_count;"* ]]
[[ "$HIR_VALIDATOR" == *"hir_validate_function_param_flow_summaries"* ]]
if [[ "$HIR_VALIDATOR" == *"ast_func_param_count(routine->ast)"* ]]; then
    echo "[hir-function-param-flow] HIR validation must use HIR-owned parameter_count" >&2
    exit 1
fi

echo "[hir-function-param-flow] semantic summary rows reach HIR with stable identity and validation"
