#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIMIT="${TEST_CASE_INCLUDE_MAX_LINES:-699}"
PRODUCTION_LIMIT="${PRODUCTION_OWNER_MAX_LINES:-699}"
HELPER_LIMIT="${HELPER_OWNER_MAX_LINES:-500}"

grep -Fq "Helper-layer escalation rule" "$ROOT_DIR/TODO.md"
grep -Fq "\`_helpers\` is not an ownership model" "$ROOT_DIR/TODO.md"

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
    "src/codegen/transpiler_lambda_emit.h"
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
    "src/codegen/transpiler_block_intent_helpers.h"
    "src/codegen/transpiler_block_intent_rebind_helpers.h"
    "src/codegen/transpiler_control_flow_emit.h"
    "src/codegen/transpiler_context.h"
    "src/codegen/transpiler_event_builtin_emit.h"
    "src/codegen/transpiler_event_emit.h"
    "src/codegen/transpiler_call_result_option_builtin_emit.h"
    "src/codegen/transpiler_domain_constructor_emit.h"
    "src/codegen/transpiler_expr_builtin_dispatch.h"
    "src/codegen/transpiler_expr_array_access_emit.h"
    "src/codegen/transpiler_expr_composite_literal_emit.h"
    "src/codegen/transpiler_expr_core_emit.h"
    "src/codegen/transpiler_expr_domain_query_builtin.h"
    "src/codegen/transpiler_expr_io_builtin.h"
    "src/codegen/transpiler_expr_stdlib_map_builtin.h"
    "src/codegen/transpiler_expr_stdlib_misc_builtin.h"
    "src/codegen/transpiler_expr_stdlib_queue_builtin.h"
    "src/codegen/transpiler_format.h"
    "src/codegen/transpiler_helpers.h"
    "src/codegen/transpiler_intent_observability_builtin_emit.h"
    "src/codegen/transpiler_intent_cleanup_emit.h"
    "src/codegen/transpiler_intent_emit.h"
    "src/codegen/transpiler_intent_emit_metadata_helpers.h"
    "src/codegen/transpiler_intent_failure_emit.h"
    "src/codegen/transpiler_intent_prologue_emit.h"
    "src/codegen/transpiler_intent_zone_binding_emit.h"
    "src/codegen/transpiler_match_bindings.h"
    "src/codegen/transpiler_zone_decl_emit.h"
    "src/codegen/transpiler_zone_frontier_emit.h"
    "src/codegen/transpiler_future_type_query.h"
    "src/codegen/transpiler_let_box_emit.h"
    "src/codegen/transpiler_let_channel_emit.h"
    "src/codegen/transpiler_let_collection_emit.h"
    "src/codegen/transpiler_let_slot_emit.h"
    "src/codegen/transpiler_let_type_register_emit.h"
    "src/codegen/transpiler_mir_phi_emit.h"
    "src/codegen/transpiler_mir_inventory_intent.h"
    "src/codegen/transpiler_mir_cfg_control_emit.h"
    "src/codegen/transpiler_mir_match_condition_emit.h"
    "src/codegen/transpiler_mir_emit_state.h"
    "src/codegen/transpiler_mir_resource_name_helpers.h"
    "src/codegen/transpiler_role_ability_helpers.h"
    "src/codegen/transpiler_zone_specialization_emit.h"
    "src/codegen/transpiler_zone_struct_emit.h"
    "src/codegen/llvm_decl_authority.h"
    "src/codegen/llvm_internal_api.h"
    "src/compiler/mir_cfg_contract_validate_cleanup.h"
    "src/semantic/type_checker_assignment.h"
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

semantic_header_body_violations="$(
    cd "$ROOT_DIR"
    find src/semantic -name '*.h' -print0 \
        | xargs -0 awk '
            /^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]*]+\(.*\)[[:space:]]*\{/ {
                print FILENAME ":" FNR ":" $0
            }
        '
)"

if [[ -n "$semantic_header_body_violations" ]]; then
    echo "semantic headers must stay declaration-only; move bodies to .c owners:" >&2
    echo "$semantic_header_body_violations" >&2
    exit 1
fi

compiler_header_body_violations="$(
    cd "$ROOT_DIR"
    find src/compiler -name '*.h' -print0 \
        | xargs -0 awk '
            /^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]*]+\(.*\)[[:space:]]*\{/ {
                print FILENAME ":" FNR ":" $0
            }
        '
)"

if [[ -n "$compiler_header_body_violations" ]]; then
    echo "compiler headers must stay declaration-only; move bodies to .c owners:" >&2
    echo "$compiler_header_body_violations" >&2
    exit 1
fi

frontend_header_body_violations="$(
    cd "$ROOT_DIR"
    find src/parser src/lexer -name '*.h' -print0 \
        | xargs -0 awk '
            /^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]*]+\(.*\)[[:space:]]*\{/ {
                print FILENAME ":" FNR ":" $0
            }
        '
)"

if [[ -n "$frontend_header_body_violations" ]]; then
    echo "parser/lexer headers must stay declaration-only; move bodies to .c owners:" >&2
    echo "$frontend_header_body_violations" >&2
    exit 1
fi

codegen_header_body_violations="$(
    cd "$ROOT_DIR"
    find src/codegen -name '*.h' ! -name 'llvm_limits_internal.h' -print0 \
        | xargs -0 awk '
            /^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]*]+\(.*\)[[:space:]]*\{/ {
                print FILENAME ":" FNR ":" $0
            }
        '
)"

if [[ -n "$codegen_header_body_violations" ]]; then
    echo "codegen headers must stay declaration-only; llvm_limits_internal.h is the only current macro-body exception:" >&2
    echo "$codegen_header_body_violations" >&2
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

semantic_owner_violations="$(
    cd "$ROOT_DIR"
    find src/semantic -maxdepth 1 -type f -name '*.c' -print0 \
        | xargs -0 wc -l \
        | awk '$2 != "total" && $1 > 599 { print }'
)"

if [[ -n "$semantic_owner_violations" ]]; then
    echo "semantic owner size violations; limit is 599 LOC:" >&2
    echo "$semantic_owner_violations" >&2
    exit 1
fi

type_system_owners=(
    "src/compiler/mir_source_local_types.c"
    "src/compiler/mir_source_local_type_shape.c"
    "src/compiler/mir_source_local_expr_binding_facts.c"
    "src/compiler/mir_source_local_expr_call_facts.c"
    "src/compiler/mir_source_local_expr_types.c"
)

type_system_owner_violations="$(
    cd "$ROOT_DIR"
    wc -l "${type_system_owners[@]}" \
        | awk '$2 != "total" && $1 > 599 { print }'
)"

if [[ -n "$type_system_owner_violations" ]]; then
    echo "type-system owner size violations; limit is 599 LOC:" >&2
    echo "$type_system_owner_violations" >&2
    exit 1
fi

helper_violations="$(
    cd "$ROOT_DIR"
    find src -type f \( -name '*helper*.c' -o -name '*helper*.h' \) \
        ! -path 'src/tests/*' \
        ! -name 'test_*.c' \
        -print0 \
        | xargs -0 wc -l \
        | awk -v limit="$HELPER_LIMIT" '$2 != "total" && $1 > limit { print }'
)"

if [[ -n "$helper_violations" ]]; then
    echo "helper owner size violations; limit is ${HELPER_LIMIT} LOC:" >&2
    echo "$helper_violations" >&2
    echo "helper growth must escalate into a responsibility-named layer/owner instead of another generic helper bucket" >&2
    exit 1
fi

echo "[test-inc-size] src has no .inc files or _IMPLEMENTATION header blocks; frontend/semantic/compiler/codegen headers stay body-free except the named LLVM macro exception; production owners <= ${PRODUCTION_LIMIT} LOC hard cap; semantic and source-local type owners <= 599 LOC; helper owners <= ${HELPER_LIMIT} LOC; helper growth is a layer-escalation signal; src/tests .cases.h files <= ${LIMIT} LOC"
