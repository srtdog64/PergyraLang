#!/usr/bin/env bash
# CLOSED selfhost.expression_surface fallbacks:
# TypedAstArenaAtomText
# TypedAstArenaValueText
# TypedAstArenaAuxValueText
# ContainsCallOutsideStrings
# CodegenExpressionUsageFactsFromArena
# CodegenAstArenaExpressionPartsAt
# FindTopLevelPlus
# RewriteBool
# RewriteIndexing
# RewriteInoutCallArgs
# ExprSequenceItemAt
# generic_return_text_inference
# codegen_generic_graph_rescan
# expr_semantic_shape_emit_owner
# callable_type_from_text
# callable_target_name_dispatch
# SemanticAstIntentExpressionOwnerNodeId
# SemanticAstExpressionVerdictFromPayload
# expression_graph_text_type_fallback
# codegen_parser_implementation_import

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:expression-surface-closed"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }

PROBE="$ROOT_DIR/src/self_hosted/tools/expression_surface_closure_probe/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/expression_surface_closed}"

for path in "$PROBE"; do
    [[ -f "$path" ]] || { echo "[$LABEL] required owner is missing: $path" >&2; exit 1; }
done

mkdir -p "$BUILD_DIR"
TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PROBE")"
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"
grep -Fq 'expression-surface-closure=parser-owned-fail-closed' \
    "$BUILD_DIR/main_c_leg.out" || {
    echo "[$LABEL] executable closure receipt is missing" >&2
    exit 1
}

echo "[$LABEL] parser-owned graph carriage, missing-fact rejection, and C/LLVM contract parity: PASS"
