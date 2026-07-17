#!/usr/bin/env bash
# RIR must retain the HIR function-parameter summary rows as validated
# resource-flow evidence, without reopening the semantic callee body.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RIR_TEST="${RIR_TEST_BIN:-$ROOT_DIR/bin/test_rir}"
if [[ -n "${RIR_TEST_BIN:-}" ]]; then
    : # caller supplied the platform-specific test path
elif [[ ! -x "$RIR_TEST" && -x "$ROOT_DIR/bin/test_rir.exe" ]]; then
    RIR_TEST="$ROOT_DIR/bin/test_rir.exe"
fi

if [[ ! -x "$RIR_TEST" ]]; then
    echo "[rir-function-param-flow] missing RIR test binary: $RIR_TEST" >&2
    exit 1
fi

OUTPUT="$($RIR_TEST)"
[[ "$OUTPUT" == *"RIR carries HIR function parameter flow summaries by stable identity"* ]]
RIR_FLOW="$(< "$ROOT_DIR/src/compiler/rir_flow.c")"
RIR_HEADER="$(< "$ROOT_DIR/src/compiler/rir.h")"
RIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/rir_validation.c")"
[[ "$RIR_FLOW" == *"rir_attach_function_param_flow_summaries"* ]]
[[ "$RIR_HEADER" == *"function_param_flow_summary_count"* ]]
[[ "$RIR_HEADER" == *"parameter_count"* ]]
[[ "$RIR_VALIDATOR" == *"function parameter flow summary"* ]]
if [[ "$RIR_FLOW" == *"ast_func_param_count(scope->ast)"* \
   || "$RIR_VALIDATOR" == *"ast_func_param_count(scope->ast)"* ]]; then
    echo "[rir-function-param-flow] RIR reopened AST parameter bounds" >&2
    exit 1
fi
if [[ "$RIR_FLOW" == *"ast_node_stable_id(fact->ast)"* ]]; then
    echo "[rir-function-param-flow] RIR flow enrichment re-derived identity from AST" >&2
    exit 1
fi

echo "[rir-function-param-flow] HIR summary rows reach RIR with stable identity and validation"
