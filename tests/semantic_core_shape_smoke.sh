#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    echo "[semantic-core-shape] $*" >&2
    exit 1
}

type_checker_loc="$(python3 "$ROOT/scripts/source_size_count.py" src/semantic/type_checker.c)" || fail "source-size measurement failed"
if [ "$type_checker_loc" -gt 600 ]; then
    fail "src/semantic/type_checker.c is ${type_checker_loc} LOC; expected <= 600"
fi

for path in \
    src/semantic/type_checker_event.c \
    src/semantic/type_checker_bind_stmt.c \
    src/semantic/type_checker_generic_validation.c \
    src/semantic/type_checker_qubit.c \
    src/semantic/type_checker_domain_role_lookup.c \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_helpers.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_metadata_diagnostics.c \
    src/semantic/type_checker_resolution_graph_inventory.c \
    src/semantic/type_checker_resolution_stage.c \
    src/semantic/type_checker_builtins_resolve.c \
    src/semantic/type_checker_builtins_intent_observability.c \
    src/semantic/type_checker_builtins_nominal.c \
    src/semantic/type_checker_builtins_nominal.h \
    src/semantic/type_checker_builtins_query.c \
    src/semantic/type_checker_builtins_query_channel.c \
    src/semantic/type_checker_builtins_query_channel.h \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_builtins_query_domain.h \
    src/semantic/type_checker_builtins_query_world.c \
    src/semantic/type_checker_builtins_secure_token.c \
    src/semantic/type_checker_builtins_slotops.c \
    src/semantic/type_checker_builtins_slotops.h \
    src/semantic/type_checker_builtins_state_tools.c \
    src/semantic/type_checker_builtins_stdlib_scalar.c \
    src/semantic/type_checker_builtins_stdlib_map.c \
    src/semantic/type_checker_builtins_stdlib_collections.c \
    src/semantic/type_checker_builtins_stdlib_array.c \
    src/semantic/type_checker_resolution_stage_alias.c \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_resolution_stage_lookup.c \
    src/semantic/type_checker_resolution_stage_stats.c \
    src/semantic/type_checker_resolution_stage_domain.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_slot_view_active.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_flow_match.c \
    src/semantic/type_checker_generic_contracts.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_enum_decl.c \
    src/semantic/type_checker_intent_authority.c \
    src/semantic/type_checker_intent_participants.c \
    src/semantic/type_checker_resolution_retired.c \
    src/semantic/type_checker_type_helpers.c \
    src/semantic/type_checker_expr.c \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_host.c \
    src/semantic/type_checker_helpers_effects.c \
    src/semantic/type_checker_projection_path.c \
    src/semantic/type_checker_world_embedding.c \
    src/semantic/slot_analyzer_escape.c \
    src/semantic/slot_analyzer_summary.c
do
    [ -f "$path" ] || fail "missing semantic owner TU: $path"
done

[ -f src/semantic/type_checker_flow_match.c ] \
    || fail "missing CFG match-flow owner TU: src/semantic/type_checker_flow_match.c"

for path in \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_helpers.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_stage.c \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_slot_view_active.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_enum_decl.c \
    src/semantic/type_checker_builtins_resolve.c \
    src/semantic/type_checker_builtins_intent_observability.c \
    src/semantic/type_checker_builtins_nominal.c \
    src/semantic/type_checker_builtins_nominal.h \
    src/semantic/type_checker_builtins_query.c \
    src/semantic/type_checker_builtins_query_channel.c \
    src/semantic/type_checker_builtins_query_channel.h \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_builtins_query_domain.h \
    src/semantic/type_checker_builtins_query_world.c \
    src/semantic/type_checker_builtins_secure_token.c \
    src/semantic/type_checker_builtins_slotops.c \
    src/semantic/type_checker_builtins_slotops.h \
    src/semantic/type_checker_builtins_state_tools.c \
    src/semantic/type_checker_builtins_stdlib_scalar.c \
    src/semantic/type_checker_builtins_stdlib_map.c \
    src/semantic/type_checker_builtins_stdlib_body.c \
    src/semantic/type_checker_builtins_stdlib_collections.c \
    src/semantic/type_checker_builtins_stdlib_array.c \
    src/semantic/type_checker_intent_ability.c \
    src/semantic/type_checker_intent_role_fields.c \
    src/semantic/type_checker_intent_decl.c \
    src/semantic/type_checker_intent_authority.c \
    src/semantic/type_checker_intent_participants.c \
    src/semantic/type_checker_resolution_retired.c \
    src/semantic/type_checker_type_helpers.c \
    src/semantic/type_checker_expr.c \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_host.c \
    src/semantic/type_checker_helpers_effects.c \
    src/semantic/type_checker_projection_path.c \
    src/semantic/type_checker_world_embedding.c \
    src/semantic/type_checker_flow.c \
    src/semantic/type_checker_flow_match.c \
    src/semantic/slot_analyzer_escape.c \
    src/semantic/slot_analyzer_summary.c
do
    loc="$(python3 "$ROOT/scripts/source_size_count.py" "$path")" || fail "source-size measurement failed"
    if [ "$loc" -gt 600 ]; then
        fail "$path is ${loc} LOC; expected <= 600"
    fi
done

echo "[semantic-core-shape] semantic owner boundaries ok"
