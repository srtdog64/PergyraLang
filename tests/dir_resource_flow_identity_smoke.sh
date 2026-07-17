#!/usr/bin/env bash
# DIR must consume the HIR-owned ResourceFlowUniverse carrier rather than
# reopening SemanticResult rows or recovering identity from AST names/pointers.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DIR_TEST="${DIR_TEST_BIN:-$ROOT_DIR/bin/test_dir}"
if [[ -n "${DIR_TEST_BIN:-}" ]]; then
    : # caller supplied the platform-specific test path
elif [[ ! -x "$DIR_TEST" && -x "$ROOT_DIR/bin/test_dir.exe" ]]; then
    DIR_TEST="$ROOT_DIR/bin/test_dir.exe"
fi

if [[ ! -x "$DIR_TEST" ]]; then
    echo "[dir-resource-flow] missing DIR test binary: $DIR_TEST" >&2
    exit 1
fi

OUTPUT="$($DIR_TEST)"
[[ "$OUTPUT" == *"[dir-resource-flow] semantic ResourceFlowUniverse rows are copied, validated, and fail closed"* ]]
DRIVER_SOURCE="$(< "$ROOT_DIR/src/compiler/driver_app.c")"
DIR_HEADER="$(< "$ROOT_DIR/src/compiler/dir.h")"
DIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/dir_validate.c")"
[[ "$DRIVER_SOURCE" == *"dir_lower_with_hir_resource_flow_facts"* ]]
if [[ "$DRIVER_SOURCE" == *"dir_lower_with_resource_flow_facts"* ]]; then
    echo "[dir-resource-flow] production driver reopened SemanticResult facts" >&2
    exit 1
fi
[[ "$DIR_HEADER" == *"resource_flow_facts"* ]]
[[ "$DIR_VALIDATOR" == *"DIR ResourceFlowUniverse snapshot is incomplete"* ]]

echo "[dir-resource-flow] DIR receives the HIR-owned snapshot with stable identity and negative validation"
