#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${TEST_CASE_INCLUDE_MAX_LINES:-990}"
PRODUCTION_LIMIT="${PRODUCTION_OWNER_MAX_LINES:-600}"

inc_files="$(
    cd "$ROOT_DIR"
    find src -type f -name '*.inc' -print
)"

if [[ -n "$inc_files" ]]; then
    echo "production .inc files are not allowed:" >&2
    echo "$inc_files" >&2
    exit 1
fi

implementation_headers="$(
    cd "$ROOT_DIR"
    grep -RIn --include='*.h' '_IMPLEMENTATION' src || true
)"

if [[ -n "$implementation_headers" ]]; then
    echo "header-only _IMPLEMENTATION blocks are not allowed in production headers:" >&2
    echo "$implementation_headers" >&2
    exit 1
fi

removed_implementation_headers=(
    "src/runtime/world_roster_plan_stats.h"
)

removed_header_violations="$(
    cd "$ROOT_DIR"
    for header in "${removed_implementation_headers[@]}"; do
        if [[ -e "$header" ]]; then
            echo "$header"
        fi
    done
)"

if [[ -n "$removed_header_violations" ]]; then
    echo "removed implementation headers must not reappear:" >&2
    echo "$removed_header_violations" >&2
    exit 1
fi

declaration_only_headers=(
    "src/codegen/transpiler_context.h"
    "src/codegen/transpiler_event_builtin_emit.h"
    "src/codegen/transpiler_event_emit.h"
    "src/codegen/transpiler_expr_stdlib_misc_builtin.h"
    "src/codegen/transpiler_format.h"
    "src/codegen/transpiler_helpers.h"
    "src/codegen/transpiler_intent_emit_metadata_helpers.h"
    "src/codegen/transpiler_intent_failure_emit.h"
    "src/codegen/transpiler_mir_phi_emit.h"
    "src/codegen/transpiler_mir_inventory_intent.h"
    "src/codegen/transpiler_mir_emit_state.h"
    "src/codegen/transpiler_mir_resource_name_helpers.h"
    "src/codegen/transpiler_role_ability_helpers.h"
)

declaration_only_body_violations="$(
    cd "$ROOT_DIR"
    for header in "${declaration_only_headers[@]}"; do
        [[ -f "$header" ]] || continue
        awk '
            /^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]*]+\(.*\)[[:space:]]*\{/ {
                print FILENAME ":" FNR ":" $0
            }
        ' "$header"
    done
)"

if [[ -n "$declaration_only_body_violations" ]]; then
    echo "declaration-only headers must not grow function bodies:" >&2
    echo "$declaration_only_body_violations" >&2
    exit 1
fi

violations="$(
    cd "$ROOT_DIR"
    find src/tests -name '*.cases.h' -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$violations" ]]; then
    echo "test case include size violations; limit is ${LIMIT} LOC:" >&2
    echo "$violations" >&2
    exit 1
fi

production_violations="$(
    cd "$ROOT_DIR"
    find src -type f \( -name '*.c' -o -name '*.h' \) \
        ! -path 'src/tests/*' \
        ! -name 'test_*.c' \
        -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$PRODUCTION_LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$production_violations" ]]; then
    echo "production owner size violations; limit is ${PRODUCTION_LIMIT} LOC:" >&2
    echo "$production_violations" >&2
    exit 1
fi

echo "[test-inc-size] src has no .inc files or _IMPLEMENTATION header blocks; declaration-only headers stay body-free; production owners <= ${PRODUCTION_LIMIT} LOC; src/tests .cases.h files <= ${LIMIT} LOC"
