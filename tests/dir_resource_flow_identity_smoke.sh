#!/usr/bin/env bash
# ResourceFlowUniverse rows stay routine-local in HIR/RIR/MIR. DIR owns the
# domain graph and must not flatten, copy, or revalidate that fact family.

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
[[ "$OUTPUT" == *"[dir-resource-flow] HIR owns validated routine-local rows; DIR carries no duplicate snapshot"* ]]
DRIVER_SOURCE="$(< "$ROOT_DIR/src/compiler/driver_app.c")"
DIR_HEADER="$(< "$ROOT_DIR/src/compiler/dir.h")"
DIR_SOURCE="$(< "$ROOT_DIR/src/compiler/dir.c")"
DIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/dir_validate.c")"
FLOW_UNIVERSE="$(< "$ROOT_DIR/src/semantic/type_checker_flow_universe.c")"
HIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/hir_validate.c")"
LIFETIME_TEST="$(< "$ROOT_DIR/src/tests/semantic/test_semantic_resource_flow_lifetime.cases.h")"
MAKEFILE_TEXT="$(< "$ROOT_DIR/Makefile")"
[[ "$DRIVER_SOURCE" == *"dir_lower_with_hir_facts"* ]]
if [[ "$DRIVER_SOURCE" == *"dir_lower_with_resource_flow_facts"* ]]; then
    echo "[dir-resource-flow] production driver reopened SemanticResult rows" >&2
    exit 1
fi
for text in "$DIR_HEADER" "$DIR_SOURCE" "$DIR_VALIDATOR"; do
    if [[ "$text" == *"resource_flow_facts"* \
       || "$text" == *"resource_flow_fact_count"* ]]; then
        echo "[dir-resource-flow] DIR reintroduced a flattened resource-flow snapshot" >&2
        exit 1
    fi
done
[[ "$HIR_VALIDATOR" == *"hir_validate_resource_flow_symbols"* ]]
[[ "$HIR_VALIDATOR" == *"resource-flow symbols share stable index"* ]]
[[ "$FLOW_UNIVERSE" == *"scope_lookup_current(scope, entry->name)"* ]]
if [[ "$FLOW_UNIVERSE" == *"current_symbol"* ]]; then
    echo "[dir-resource-flow] universe reintroduced borrowed Symbol pointer authority" >&2
    exit 1
fi
[[ "$LIFETIME_TEST" == *"resource-flow facts outlive inner-block Symbol storage"* ]]
[[ "$LIFETIME_TEST" == *"ast_assign_stable_ids(program)"* ]]
[[ "$LIFETIME_TEST" == *"result->resource_flow_facts[i]"* ]]
[[ "$MAKEFILE_TEXT" == *"ASAN_UNIT_BATTERIES ?= test_air test_semantic test_parser test_mir"* ]]

echo "[dir-resource-flow] HIR-only stable identity, no DIR reserialization, inner-block lifetime, and ASan coverage are closed"
