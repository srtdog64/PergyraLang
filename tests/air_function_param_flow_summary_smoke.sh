#!/usr/bin/env bash
# AIR must project MIR-owned function-parameter flow rows without reopening
# HIR/AST bodies, and its presence flag/identity checks must fail closed.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AIR_TEST="${AIR_TEST_BIN:-$ROOT_DIR/bin/test_air}"

if [[ ! -x "$AIR_TEST" ]]; then
    echo "[air-function-param-flow] missing AIR test binary: $AIR_TEST" >&2
    exit 1
fi

OUTPUT="$($AIR_TEST)"
grep -Fq -- "AIR carries MIR function parameter flow summaries by stable identity" <<<"$OUTPUT"
grep -Fq -- "AIRFunctionParamFlowSummary" "$ROOT_DIR/src/compiler/air.h"
grep -Fq -- "air_collect_function_param_flow_summaries" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c"
grep -Fq -- "air_validate_function_param_flow_summary_inventory" \
    "$ROOT_DIR/src/compiler/air_validate.c"

echo "[air-function-param-flow] MIR summary rows reach AIR with stable identity and fail-closed validation"
