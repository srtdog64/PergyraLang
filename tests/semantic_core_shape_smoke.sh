#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    echo "[semantic-core-shape] $*" >&2
    exit 1
}

program_root_uses="$(mktemp "${TMPDIR:-/tmp}/pgy-program-root-uses.XXXXXX")"
air_raw_uses="$(mktemp "${TMPDIR:-/tmp}/pgy-air-raw-uses.XXXXXX")"
shape_scan_cache=""
cleanup_shape_scan_cache() {
    rm -f "$program_root_uses" "$air_raw_uses"
    if [ -n "$shape_scan_cache" ]; then
        rm -f "$shape_scan_cache"
    fi
}
trap cleanup_shape_scan_cache EXIT

ensure_shape_scan_cache() {
    if [ -z "$shape_scan_cache" ]; then
        shape_scan_cache="$(mktemp "${TMPDIR:-/tmp}/pgy-semantic-shape.XXXXXX")"
        find src/semantic src/compiler src/codegen \
            -type f \( -name '*.c' -o -name '*.h' \) -print0 \
            | xargs -0 grep -nH -E 'data\.|resolve_type_node\(' \
            > "$shape_scan_cache" || true
    fi
}

{ grep -RIn 'ctx->program_root' src/semantic || true; } \
    >"$program_root_uses"
while IFS=: read -r path line text; do
    [ -n "$path" ] || continue
    if [ "$path" = "src/semantic/type_checker_call_contract_helpers.c" ] ||
       [ "$path" = "src/semantic/type_checker_domain_role_lookup.c" ] ||
       [ "$path" = "src/semantic/type_checker_host_helpers.c" ] ||
       [ "$path" = "src/semantic/type_checker_host_lookup.c" ] ||
       [ "$path" = "src/semantic/type_checker_program.c" ] ||
       [ "$path" = "src/semantic/type_checker_resolution_helpers.c" ] ||
       [ "$path" = "src/semantic/type_checker_resolution_stage_lookup.c" ]; then
        continue
    fi
    fail "unexpected semantic program-root consumer: $path:$line: $text"
done <"$program_root_uses"

{ grep -RInE 'air->(intent_count|intents|boundary_count|boundaries|drift_count|drifts|has_hir_input|has_rir_input|has_mir_input|strict_evidence|evidence_count|evidence_nodes)\b' src/compiler || true; } \
    >"$air_raw_uses"
while IFS=: read -r path line text; do
    [ -n "$path" ] || continue
    if [ "$path" = "src/compiler/air.c" ] ||
       [ "$path" = "src/compiler/air_drift.c" ] ||
       [ "$path" = "src/compiler/air_evidence_node.c" ]; then
        continue
    fi
    fail "unexpected AIR raw graph-field consumer: $path:$line: $text"
done <"$air_raw_uses"

if ! grep -q "mir_routine_inventory_from_program(mir, &inventory)" \
        src/compiler/air_evidence_mir.c; then
    fail "AIR MIR evidence must consume MIR routine inventory through the public surface"
fi
if grep -Eq 'mir->(routine_count|routines)\b' \
        src/compiler/air_evidence_mir.c; then
    fail "AIR MIR evidence must not reopen raw MIR routine arrays"
fi
if ! grep -q "hir_routine_inventory_from_program(hir, &inventory)" \
        src/compiler/air_evidence_hir.c; then
    fail "AIR HIR evidence must consume HIR routine inventory through the public surface"
fi
if grep -Eq 'hir->(routine_count|routines)\b' \
        src/compiler/air_evidence_hir.c; then
    fail "AIR HIR evidence must not reopen raw HIR routine arrays"
fi
if ! grep -q "hir_routine_inventory_from_program(hir, &hir_inventory)" \
        src/compiler/mir.c; then
    fail "MIR lowering must consume HIR routine inventory through the public surface"
fi
if ! grep -q "hir_routine_inventory_from_program(hir, &hir_inventory)" \
        src/compiler/rir_flow.c; then
    fail "RIR flow enrichment must consume HIR routine inventory through the public surface"
fi
if ! grep -q "rir_mutable_scope_inventory_from_program(rir, &rir_inventory)" \
        src/compiler/rir_flow.c; then
    fail "RIR flow enrichment must consume mutable RIR scope inventory through the public surface"
fi
if grep -Eq 'hir->(routine_count|routines)\b' \
        src/compiler/mir.c src/compiler/rir_flow.c; then
    fail "HIR routine consumers must not reopen raw HIR routine arrays"
fi
if grep -Eq 'rir->(scope_count|scopes)\b' \
        src/compiler/rir_flow.c; then
    fail "RIR flow enrichment must not reopen raw RIR scope arrays"
fi
for term in \
    "rir_scope_fact_count(scope)" \
    "rir_scope_fact_at(scope, i)" \
    "rir_scope_op_count(scope)" \
    "rir_scope_op_at(scope, i)" \
    "rir_scope_state_summary_count(scope)" \
    "rir_scope_state_summary_at(scope, fact_i)"; do
    if ! grep -q "$term" src/compiler/rir_flow.c; then
        fail "RIR flow enrichment must consume RIR scope item accessor term: $term"
    fi
done
if grep -Eq 'scope->(facts|fact_count|ops|op_count)\b' \
        src/compiler/rir_flow.c; then
    fail "RIR flow enrichment must consume RIR fact/op item accessors"
fi
if ! grep -q "hir_routine_inventory_from_program(hir, &inventory)" \
        src/compiler/hir_validate.c; then
    fail "HIR validation must consume HIR routine inventory through the public surface"
fi
if grep -Eq 'hir->(routine_count|routines)\b' \
        src/compiler/hir_validate.c; then
    fail "HIR validation must not reopen raw HIR routine arrays"
fi
if ! grep -q "hir_mutable_routine_inventory_from_program(hir, &inventory)" \
        src/compiler/hir_callgraph.c; then
    fail "HIR call graph must consume mutable HIR routine inventory through the public surface"
fi
if grep -Eq 'hir->(routine_count|routines)\b' \
        src/compiler/hir_callgraph.c; then
    fail "HIR call graph must not reopen raw HIR routine arrays"
fi
if ! grep -q "rir_scope_inventory_from_program(rir, &inventory)" \
        src/compiler/air_evidence_rir.c; then
    fail "AIR RIR evidence must consume RIR scope inventory through the public surface"
fi
if grep -Eq 'rir->(scope_count|scopes)\b' \
        src/compiler/air_evidence_rir.c; then
    fail "AIR RIR evidence must not reopen raw RIR scope arrays"
fi
if ! grep -q "rir_scope_op_at(scope, i)" \
        src/compiler/air_evidence_rir_match.c; then
    fail "AIR RIR matching must consume RIR scope ops through public accessors"
fi
if grep -R -E 'scope->(facts|fact_count|ops|op_count|state_summaries|state_summary_count)\b' \
        src/compiler/air_evidence_rir*.c >/dev/null; then
    fail "AIR RIR evidence must not reopen raw RIR fact/op/summary arrays"
fi
if ! grep -q "rir_scope_inventory_from_program(rir, &inventory)" \
        src/compiler/mir_base_helpers.c; then
    fail "MIR RIR-scope matching must consume RIR scope inventory through the public surface"
fi
if grep -E 'rir->(scope_count|scopes)\b' \
        src/compiler/mir_base_helpers.c >/dev/null; then
    fail "MIR RIR-scope matching must not reopen raw RIR scope arrays"
fi
if grep -R -E 'rir_scope->(facts|fact_count|ops|op_count|flow_blocks|flow_block_count|conservative_semantics)\b|flow->(facts|fact_count|entry_semantics|exit_semantics)\b' \
        src/compiler/mir_cleanup.c \
        src/compiler/mir_lower_population.c >/dev/null; then
    fail "MIR RIR-scope consumers must consume RIR fact/op/flow-block semantic accessors"
fi
if ! grep -q "rir_scope_inventory_from_program(rir, &inventory)" \
        src/compiler/rir_validation.c; then
    fail "RIR validation must consume RIR scope inventory through the public surface"
fi
if ! grep -q "rir_scope_inventory_from_program(rir, &inventory)" \
        src/compiler/rir_validation_dir.c; then
    fail "RIR/DIR validation must consume RIR scope inventory through the public surface"
fi
if grep -E 'rir->(scope_count|scopes)\b' \
        src/compiler/rir_validation.c \
        src/compiler/rir_validation_dir.c >/dev/null; then
    fail "RIR validation must not reopen raw RIR scope arrays"
fi
if grep -R -E 'scope->(kind|name|owner_name|has_state_errors|facts|fact_count|ops|op_count|state_summaries|state_summary_count)\b' \
        src/compiler/rir_validation.c \
        src/compiler/rir_validation_dir.c >/dev/null; then
    fail "RIR validation must consume RIR scope metadata/item accessors"
fi
if grep -R -E '(^|[^[:alnum:]_])scope_find_state_summary\(|\(RIRScope \*\)scope' \
        src/compiler/rir_validation.c \
        src/compiler/rir_validation_dir.c >/dev/null; then
    fail "RIR validation must consume const RIR state-summary lookup through the public surface"
fi
if grep -q '^rir_scope_find_projection_fact' src/compiler/rir_validation.c; then
    fail "RIR projection lookup implementation must live in the public surface owner"
fi
if grep -q '^rir_scope_find_fact_by_name_kind' src/compiler/rir_validation_dir.c ||
   grep -q '^rir_scope_has_capability_fact' src/compiler/rir_validation_dir.c; then
    fail "RIR/DIR validation fact lookup implementations must live in the public surface owner"
fi
if grep -E 'rir_scope_fact_(count|at)\(scope' src/compiler/rir_validation_dir.c >/dev/null; then
    fail "RIR/DIR validation must consume public RIR fact lookup helpers, not local fact scans"
fi
for term in \
    "rir_scope_kind(scope)" \
    "rir_scope_display_name(scope)"; do
    if ! grep -q "$term" src/compiler/rir_validation.c; then
        fail "RIR validation must consume RIR scope metadata accessor term: $term"
    fi
done
for term in \
    "rir_scope_kind(scope)" \
    "rir_scope_name(scope)" \
    "rir_scope_display_name(scope)"; do
    if ! grep -q "$term" src/compiler/rir_validation_dir.c; then
        fail "RIR/DIR validation must consume RIR scope metadata accessor term: $term"
    fi
done
if grep -E 'scope->(flow_blocks|flow_block_count)\b|block->(block_id|is_reachable|is_join|facts|fact_count)\b' \
        src/compiler/rir_validation.c >/dev/null; then
    fail "RIR validation must consume RIR flow-block item accessors"
fi
for term in \
    "rir_scope_flow_block_count(scope)" \
    "rir_scope_flow_block_at(scope, j)" \
    "rir_flow_block_id(block)" \
    "rir_flow_block_fact_count(block)" \
    "rir_flow_block_fact_at(block, k)"; do
    if ! grep -q "$term" src/compiler/rir_validation.c; then
        fail "RIR validation must consume RIR flow-block accessor term: $term"
    fi
done
for term in \
    "rir_scope_inventory_from_program(rir, &inventory)" \
    "rir_scope_inventory_get(&inventory, i)" \
    "rir_scope_kind(scope)" \
    "rir_scope_name(scope)" \
    "rir_scope_owner_name(scope)" \
    "rir_scope_display_name(scope)" \
    "rir_scope_has_state_errors(scope)" \
    "rir_scope_find_state_summary(const RIRScope" \
    "rir_scope_find_fact_by_name_kind(const RIRScope" \
    "rir_scope_find_projection_fact(const RIRScope" \
    "rir_scope_has_capability_fact(const RIRScope" \
    "rir_scope_fact_count(scope)" \
    "rir_scope_fact_at(scope, j)" \
    "rir_scope_op_count(scope)" \
    "rir_scope_op_at(scope, j)" \
    "rir_scope_state_summary_count(scope)" \
    "rir_scope_state_summary_at(scope, j)" \
    "rir_scope_conservative_semantics(scope)" \
    "rir_scope_flow_block_count(scope)" \
    "rir_scope_flow_block_at(scope, j)" \
    "rir_flow_block_id(block)" \
    "rir_flow_block_is_reachable(block)" \
    "rir_flow_block_is_join(block)" \
    "rir_flow_block_entry_semantics(block)" \
    "rir_flow_block_exit_semantics(block)" \
    "rir_flow_block_fact_count(block)" \
    "rir_flow_block_fact_at(block, k)"; do
    if ! grep -q "$term" src/compiler/rir_public_surface.c; then
        fail "RIR public dump surface must consume accessor term: $term"
    fi
done

grep() {
    if [ "${1:-}" = "-R" ] && [ "$#" -ge 3 ]; then
        local pattern="$2"
        shift 2
        case "$pattern" in
            *"data\\."*|"resolve_type_node(")
                local line
                local target
                local matched=1
                ensure_shape_scan_cache
                while IFS= read -r line; do
                    for target in "$@"; do
                        case "$line" in
                            "$target":*|"$target"/*)
                                printf '%s\n' "$line"
                                matched=0
                                break
                                ;;
                        esac
                    done
                done < <(command grep "$pattern" "$shape_scan_cache")
                return "$matched"
                ;;
        esac
    fi
    command grep "$@"
}

type_checker_loc="$(wc -l < src/semantic/type_checker.c | tr -d '[:space:]')"
if [ "$type_checker_loc" -gt 600 ]; then
    fail "src/semantic/type_checker.c is ${type_checker_loc} LOC; expected <= 600"
fi

if [ -e src/semantic/type_checker_resolution_graph_inventory.inc ]; then
    fail "DAG inventory must stay in type_checker_resolution_graph_inventory.c, not .inc"
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
    loc="$(wc -l < "$path" | tr -d '[:space:]')"
    if [ "$loc" -gt 600 ]; then
        fail "$path is ${loc} LOC; expected <= 600"
    fi
done

if grep -q '#include "type_checker_resolution_graph_inventory.inc"' src/semantic/type_checker.c; then
    fail "type_checker.c must not include graph inventory body"
fi

if ! grep -q '^type_check_enum_decl(ASTNode \*node,' \
    src/semantic/type_checker_enum_decl.c; then
    fail "enum declaration validation must stay in type_checker_enum_decl.c"
fi

if grep -q '^type_check_enum_decl(ASTNode \*node,' \
    src/semantic/type_checker.c; then
    fail "type_checker.c must not own enum declaration validation"
fi

grep -q 'return type_check_bind_stmt(node, ctx);' src/semantic/type_checker.c \
    || fail "AST_BIND_STMT must be validated by type_check_bind_stmt"

grep -q 'type_check_bind_stmt(node, ctx);' src/semantic/type_checker_flow.c \
    || fail "CFG/body flow must consume semantic bind validation"

if grep -q 'validated at codegen level' src/semantic/type_checker.c; then
    fail "semantic bind validation must not be delegated to codegen"
fi

grep -q 'missing ability' src/semantic/type_checker_bind_stmt.c \
    || fail "bind validation must report the missing party slot ability"

grep -q 'semantic_find_party_decl_by_name(ctx, party_type_name)' \
    src/semantic/type_checker_bind_stmt.c \
    || fail "bind validation must consume the semantic party declaration owner seam"

if grep -q 'find_domain_decl_by_name' src/semantic/type_checker_bind_stmt.c; then
    fail "bind validation must not reopen direct domain declaration lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, object_type)' \
    src/semantic/type_checker_intent_control.c \
    || fail "intent control member-call checks must consume semantic host decl seam"

grep -q 'semantic_host_decl_methods(host_decl, &method_count)' \
    src/semantic/type_checker_intent_control.c \
    || fail "intent control member-call checks must consume semantic host method seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_intent_control.c; then
    fail "intent control must not reopen class-only direct type lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, slot_type)' \
    src/semantic/type_checker_domain_slots.c \
    || fail "domain slot validation must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_domain_slots.c; then
    fail "domain slot validation must not reopen direct type lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, field_type)' \
    src/semantic/type_checker_class_decl.c \
    || fail "class vessel-field validation must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_class_decl.c; then
    fail "class declaration validation must not reopen direct type lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, sym->type)' \
    src/semantic/type_checker_assignment.c \
    || fail "assignment projection immutability checks must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_assignment.c; then
    fail "assignment validation must not reopen direct type lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, type)' \
    src/semantic/type_checker_helpers_resources.c \
    || fail "value boundary nominal checks must consume semantic host decl seam"

if grep -q 'ctx->program_root == NULL' src/semantic/type_checker_helpers_resources.c; then
    fail "value boundary nominal checks must not guard on raw program root"
fi

grep -q 'semantic_host_decl_for_type(ctx, object_type)' src/semantic/type_checker_expr.c \
    || fail "member access nominal checks must consume semantic host decl seam"

if grep -q 'object_type->name != NULL && ctx->program_root != NULL' src/semantic/type_checker_expr.c; then
    fail "member access nominal checks must not guard on raw program root"
fi

grep -q 'semantic_resolve_projection_source_field_type(' \
    src/semantic/type_checker_builtins_projection.c \
    || fail "projection builtins must consume context-bearing projection field type seam"

if grep -q 'resolve_projection_source_field_type_rec(.*ctx->program_root' \
    src/semantic/type_checker_builtins_projection.c; then
    fail "projection builtins must not pass raw program-root into projection resolver"
fi

grep -q 'semantic_resolve_projection_source_field_path(' \
    src/semantic/type_checker_domain_projection_fields.c \
    || fail "domain projection field validation must consume context-bearing projection path seam"

if grep -q 'resolve_projection_source_field_path(.*ctx->program_root' \
    src/semantic/type_checker_domain_projection_fields.c; then
    fail "domain projection field validation must not pass raw program-root into projection resolver"
fi

grep -q 'semantic_resolve_projection_source_field_path(' \
    src/semantic/type_checker_resolution_graph_zone.c \
    || fail "DAG zone projection graph must consume context-bearing projection path seam"

if grep -q 'resolve_projection_source_field_path(.*ctx->program_root' \
    src/semantic/type_checker_resolution_graph_zone.c; then
    fail "DAG zone projection graph must not pass raw program-root into projection resolver"
fi

if grep -q 'return true;' src/semantic/type_checker_bind_stmt.c \
   && ! grep -q 'for (size_t i = 0; i < ability_count; i++)' src/semantic/type_checker_bind_stmt.c; then
    fail "bind validation must check every party slot ability"
fi

grep -q 'LLVM bind emission cannot resolve party variable' src/codegen/llvm_stmt.c \
    || fail "LLVM bind lowering must fail closed on missing party variable"

grep -q 'LLVM bind emission cannot resolve role vtable global' src/codegen/llvm_stmt.c \
    || fail "LLVM bind lowering must fail closed on missing role vtable global"

if [ ! -f src/semantic/type_checker_resolution_worklist.c ]; then
    fail "DAG worklist execution must stay in type_checker_resolution_worklist.c"
fi

if ! grep -q '^semantic_run_type_resolution_worklist(ASTNode \*program,' \
    src/semantic/type_checker_resolution_worklist.c; then
    fail "type_checker_resolution_worklist.c must own semantic_run_type_resolution_worklist"
fi

if grep -q '^semantic_run_type_resolution_worklist(ASTNode \*program,' \
    src/semantic/type_checker.c; then
    fail "type_checker.c must not own DAG worklist execution"
fi

if [ -e src/semantic/type_checker_resolve.c ] || [ -e src/semantic/type_checker_resolve.h ]; then
    fail "obsolete type_checker_resolve compatibility owner must not reappear"
fi

if grep -q 'semantic_assignment_path_heap_owner' src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path heap lane must not reappear"
fi

if grep -q 'semantic_assignment_target_path(ASTNode \*expr' \
    src/semantic/type_checker_ownership_support_internal.h \
    src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path heap API must not reappear"
fi

grep -q 'semantic_assignment_path_scratch_owner' src/semantic/type_checker_assignment_path.c \
    || fail "assignment target path scratch lane must stay explicit"

if grep -q 'semantic_assignment_target_path_impl(ASTNode \*expr' \
    src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must not reopen ctx + scratch bool mode"
fi

if grep -q 'scratch && ctx' src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must not compute scratch mode from a bool seam"
fi

grep -q 'Retired compatibility resolver quarantine' src/semantic/type_checker_resolution_retired.c \
    || fail "retired DAG compatibility quarantine owner lost its audit marker"

grep -q 'require_assignable(Type \*from, Type \*to' src/semantic/type_checker_type_helpers.c \
    || fail "assignability helper must stay outside the retired resolver quarantine owner"

grep -q 'semantic_role_for_type_name' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role target-type helper must live in domain role lookup owner"

grep -q 'role_lookup_find_decl_by_name' src/semantic/type_checker_domain_role_lookup.c \
    || fail "private role declaration lookup helper must live in domain role lookup owner"

grep -q 'semantic_host_index_find_decl_by_name(ctx, AST_ROLE_DECL' \
    src/semantic/type_checker_domain_role_lookup.c \
    || fail "role declaration name lookup must consume the host declaration index first"

grep -q 'case AST_ROLE_DECL:' src/semantic/type_checker_host_index.c \
    || fail "host declaration index must include role declarations"

if grep -q 'semantic_find_role_decl(ASTNode \*program' \
    src/semantic/type_checker_decls_a_helpers_internal.h; then
    fail "role declaration lookup must expose only SemanticContext-backed wrappers"
fi

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role lookup ability scan must consume AST impl-ability accessor"

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload include traversal must consume AST include accessor"

grep -q 'ast_impl_ability_method(impl, j)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload method traversal must consume AST impl-ability accessor"

grep -q 'semantic_find_next_role_decl_for_type_name(' src/semantic/type_checker_ability_match.c \
    || fail "semantic ability wrappers must consume context-bearing role-for-type seam"

grep -q 'semantic_role_decl_has_ability(ctx, stmt, ability_ref)' \
    src/semantic/type_checker_ability_match.c \
    || fail "subject ability matching must consume SemanticContext-backed role ability seam"

if grep -q 'return subject_type_has_ability(ctx->program_root' src/semantic/type_checker_ability_match.c; then
    fail "semantic ability wrapper must not delegate through raw program-root subject scan"
fi

if grep -q 'return subject_type_find_base_ability_impl(ctx->program_root' src/semantic/type_checker_ability_match.c; then
    fail "semantic base ability wrapper must not delegate through raw program-root subject scan"
fi

if grep -q 'role_decl_has_ability(stmt, ctx->program_root' src/semantic/type_checker_ability_match.c; then
    fail "semantic ability wrapper must not pass raw program-root into role ability scan"
fi

if grep -q 'ability_match_program(ctx)\|ctx->program_root' \
    src/semantic/type_checker_ability_match.c; then
    fail "ability match owner must consume SemanticContext lookup seams, not program-root scans"
fi

if grep -q 'ctx->program_root == NULL' src/semantic/type_checker_ability_where.c; then
    fail "ability where validation must rely on context-bearing semantic ability lookup"
fi

if grep -q 'ctx->program_root == NULL' src/semantic/type_checker_generic_contracts.c; then
    fail "generic contract validation must rely on context-bearing semantic ability lookup"
fi

grep -q 'semantic_find_role_decl_by_name(ctx, role_name)' \
    src/semantic/type_checker_ability_match.c \
    || fail "ability include traversal must consume the SemanticContext role lookup helper"

grep -q 'semantic_host_decl_for_type(ctx, resolved_type)' \
    src/semantic/type_checker_ability_fields.c \
    || fail "ability field visibility checks must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_ability_fields.c; then
    fail "ability field validation must not reopen direct type lookup"
fi

grep -q 'semantic_find_class_decl_by_name(ctx,' \
    src/semantic/type_checker_builtins_projection.c \
    || fail "projection builtins must consume semantic class lookup seam"

if grep -q 'find_named_class_decl(ctx->program_root' \
    src/semantic/type_checker_builtins_projection.c; then
    fail "projection builtins must not reopen query-domain class lookup"
fi

if grep -q 'find_named_class_decl' \
    src/semantic/type_checker_builtins_query_domain.h \
    src/semantic/type_checker_internal.h; then
    fail "query-domain class lookup must stay owner-local"
fi

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_ability_match.c \
    || fail "ability include traversal must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_ability_match.c \
    || fail "ability matching must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_name(role_decl)' src/semantic/type_checker_intent_role_fields.c \
    || fail "intent role-field validation must consume the semantic role target-type helper"

grep -q 'semantic_host_decl_for_type(ctx, field_type)' \
    src/semantic/type_checker_intent_role_fields.c \
    || fail "intent role-field vessel traversal must consume semantic host decl seam"

grep -q 'semantic_host_decl_for_type(ctx, bound_type)' \
    src/semantic/type_checker_intent_role_fields.c \
    || fail "intent role-field host validation must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_intent_role_fields.c; then
    fail "intent role-field validation must not reopen direct type lookup"
fi

grep -q 'intent_involves_is_subject_host(ASTNode \*involves,' \
    src/semantic/type_checker_intent_helpers.c \
    || fail "intent participant subject-host checks must use typed context seam"

grep -q 'semantic_host_decl_for_type(ctx, type)' \
    src/semantic/type_checker_intent_helpers.c \
    || fail "intent participant subject-host checks must consume semantic host decl seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_intent_helpers.c; then
    fail "intent helper subject-host checks must not reopen direct subject-host lookup"
fi

if grep -R 'intent_involves_is_subject_host(ctx->program_root' src/semantic >/dev/null; then
    fail "intent participant subject-host callers must pass SemanticContext to the typed seam"
fi

grep -q 'semantic_host_decl_for_type(ctx, subject_type)' \
    src/semantic/type_checker_intent_on_inference.c \
    || fail "intent on-inference must consume typed subject host seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_intent_on_inference.c; then
    fail "intent on-inference must not reopen direct subject-host lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx,' \
    src/semantic/type_checker_intent_participants.c \
    || fail "intent participant action checks must consume typed subject host seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_intent_participants.c; then
    fail "intent participant checks must not reopen direct subject-host lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, type)' \
    src/semantic/type_checker_intent_action_contract.c \
    || fail "intent action contract inheritance must consume typed subject host seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_intent_action_contract.c; then
    fail "intent action contract inheritance must not reopen direct subject-host lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, type)' \
    src/semantic/type_checker_intent_contract_summary.c \
    || fail "intent contract summary must consume typed subject host seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_intent_contract_summary.c; then
    fail "intent contract summary must not reopen direct subject-host lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, field_type)' \
    src/semantic/type_checker_projection_path.c \
    || fail "projection vessel-field lookup must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_projection_path.c; then
    fail "projection path validation must not reopen direct type lookup"
fi

grep -q 'semantic_find_class_decl_by_name(ctx, target_type->name)' \
    src/semantic/type_checker_domain_projection.c \
    || fail "domain projection target checks must consume semantic class lookup seam"

grep -q 'semantic_find_class_decl_by_name(ctx, source_type->name)' \
    src/semantic/type_checker_domain_projection.c \
    || fail "domain projection source fallback must consume semantic class lookup seam"

if grep -q 'find_named_class_decl_local' src/semantic/type_checker_domain_projection.c; then
    fail "domain projection validation must not reopen local class declaration scans"
fi

grep -q 'semantic_find_class_decl_by_name(ctx,' \
    src/semantic/type_checker_domain_contracts.c \
    || fail "domain relation endpoint validation must consume semantic class lookup seam"

if grep -q 'find_named_class_decl_local' src/semantic/type_checker_domain_contracts.c; then
    fail "domain relation/effect contracts must not reopen local class declaration scans"
fi

grep -q 'semantic_find_ability_decl_by_name(ctx,' \
    src/semantic/type_checker_ability_where.c \
    || fail "ability where-clause validation must consume semantic ability lookup seam"

grep -q 'semantic_find_ability_decl_by_name(ctx, bound_name)' \
    src/semantic/type_checker_generic_contracts.c \
    || fail "generic contract validation must consume semantic ability lookup seam"

grep -q 'semantic_find_ability_decl_by_name(' \
    src/semantic/type_checker_generic_validation.c \
    || fail "generic validation must consume semantic ability lookup seam"

grep -q 'semantic_find_ability_decl_by_name(ctx, ability)' \
    src/semantic/type_checker_module_contract.c \
    || fail "module ability contracts must consume semantic ability lookup seam"

grep -q 'semantic_find_ability_decl_by_name(' \
    src/semantic/type_checker_role_decl.c \
    || fail "role impl validation must consume semantic ability lookup seam"

grep -q 'semantic_find_ability_decl_by_name(ctx, provider_name)' \
    src/semantic/type_checker_resolution_stage_signature.c \
    || fail "DAG signature provider lookup must consume semantic ability lookup seam"

if grep -q 'find_ability_decl_by_name(ctx->program_root' \
    src/semantic/type_checker_ability_where.c \
    src/semantic/type_checker_generic_contracts.c \
    src/semantic/type_checker_generic_validation.c \
    src/semantic/type_checker_module_contract.c \
    src/semantic/type_checker_role_decl.c \
    src/semantic/type_checker_resolution_stage_signature.c; then
    fail "context-bearing semantic owners must not reopen raw ability declaration lookup"
fi

grep -q 'semantic_subject_type_has_ability(ctx,' \
    src/semantic/type_checker_ability_where.c \
    || fail "ability where-clause validation must consume semantic subject ability seam"

grep -q 'semantic_subject_type_has_ability(ctx,' \
    src/semantic/type_checker_generic_contracts.c \
    || fail "generic contract validation must consume semantic subject ability seam"

grep -q 'semantic_subject_type_has_ability(ctx,' \
    src/semantic/type_checker_intent_ability.c \
    || fail "intent ability validation must consume semantic subject ability seam"

grep -q 'semantic_subject_type_find_base_ability_impl(' \
    src/semantic/type_checker_intent_ability.c \
    || fail "intent ability diagnostics must consume semantic subject ability impl seam"

grep -q 'semantic_subject_type_has_ability(ctx,' \
    src/semantic/type_checker_module_contract.c \
    || fail "action ability contracts must consume semantic subject ability seam"

grep -q 'semantic_subject_type_find_base_ability_impl(' \
    src/semantic/type_checker_module_contract.c \
    || fail "action ability diagnostics must consume semantic subject ability impl seam"

grep -q 'semantic_subject_type_has_ability(ctx,' \
    src/semantic/type_checker_zone_decl_authority.c \
    || fail "zone authority ability validation must consume semantic subject ability seam"

grep -q 'semantic_subject_type_find_base_ability_impl(' \
    src/semantic/type_checker_zone_decl_authority.c \
    || fail "zone authority ability diagnostics must consume semantic subject ability impl seam"

if grep -q 'subject_type_has_ability(ctx->program_root' \
    src/semantic/type_checker_ability_where.c \
    src/semantic/type_checker_generic_contracts.c \
    src/semantic/type_checker_intent_ability.c \
    src/semantic/type_checker_module_contract.c \
    src/semantic/type_checker_zone_decl_authority.c; then
    fail "context-bearing ability consumers must not reopen raw subject ability lookup"
fi

if grep -q 'subject_type_find_base_ability_impl(ctx->program_root' \
    src/semantic/type_checker_intent_ability.c \
    src/semantic/type_checker_module_contract.c \
    src/semantic/type_checker_zone_decl_authority.c; then
    fail "context-bearing ability diagnostics must not reopen raw subject ability impl lookup"
fi

grep -q 'semantic_find_role_decl_by_name(ctx, role_name)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration validation must consume the shared semantic role lookup helper"

grep -q 'semantic_find_role_decl_by_name(ctx, role_name)' src/semantic/type_checker_bind_stmt.c \
    || fail "bind role validation must consume context-bearing semantic role lookup seam"

grep -q 'semantic_role_satisfies_party_slot(ctx, role_decl, role_slot' \
    src/semantic/type_checker_bind_stmt.c \
    || fail "bind role validation must consume context-bearing role-slot ability seam"

if grep -q 'if (!role_satisfies_party_slot(role_decl, role_slot, ctx->program_root' \
    src/semantic/type_checker_bind_stmt.c; then
    fail "bind role validation must not pass raw program-root into role-slot ability seam"
fi

if grep -q 'bind_stmt_program(ctx)\|ctx->program_root' \
    src/semantic/type_checker_bind_stmt.c; then
    fail "bind role-slot owner must consume SemanticContext ability seams, not program-root scans"
fi

grep -q 'semantic_find_next_role_decl_for_type_name(' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload role lookup must consume context-bearing role-for-type seam"

grep -q 'snapshot_resource_states_from_scope(' src/semantic/type_checker_flow_parallel.c \
    || fail "parallel flow must snapshot task deltas from a stable parent scope"

if grep -q 'task_snap = snapshot_resource_states(ctx)' \
    src/semantic/type_checker_flow_parallel.c; then
    fail "parallel flow must not retain task-local symbols after scope exit"
fi

grep -q 'semantic_find_role_decl_by_name(ctx, role_name)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload role include traversal must consume semantic role lookup seam"

if grep -q 'ctx->program_root' src/semantic/type_checker_expr_ops.c; then
    fail "operator overload validation must not consume raw program root"
fi

if grep -q 'semantic_find_role_decl(ctx->program_root' \
    src/semantic/type_checker_bind_stmt.c \
    src/semantic/type_checker_role_decl.c; then
    fail "context-bearing role consumers must not reopen raw role lookup"
fi

grep -q 'semantic_role_for_type_node(node)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration host-type validation must consume the semantic role target-type helper"

grep -q 'semantic_host_decl_for_type(ctx, bound_type)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration host-type validation must consume semantic host decl seam"

if grep -q 'find_type_decl_by_name' src/semantic/type_checker_role_decl.c; then
    fail "role declaration validation must not reopen direct type lookup"
fi

grep -q 'semantic_constructor_decl_for_symbol_kind(ctx, SYMBOL_CLASS' \
    src/semantic/type_checker_host_resource.c \
    || fail "constructor-call detection must route through the constructor host seam"

grep -q 'semantic_constructor_decl_for_symbol_kind(ctx,' \
    src/semantic/type_checker_call_constructor.c \
    || fail "constructor symbol call validation must consume semantic constructor lookup seam"

if grep -q 'constructor_decl_for_symbol_kind(ctx->program_root' \
    src/semantic/type_checker_call_constructor.c; then
    fail "context-bearing constructor consumers must not reopen raw constructor lookup"
fi

if grep -q 'ctx->program_root' src/semantic/type_checker_call_constructor.c; then
    fail "constructor symbol call validation must not guard on raw program root"
fi

grep -q 'semantic_find_callable_decl_by_name(ctx, display_name)' \
    src/semantic/type_checker_helpers_late.c \
    || fail "late callable validation must consume semantic callable lookup seam"

if grep -q 'find_callable_decl_by_name(ctx->program_root' \
    src/semantic/type_checker_helpers_late.c; then
    fail "late callable validation must not reopen raw callable lookup"
fi

if grep -q 'ctx->program_root != NULL' \
    src/semantic/type_checker_helpers_late.c; then
    fail "late callable validation must not guard on raw program root"
fi

grep -q 'semantic_find_function_decl_by_name(ctx, display_name)' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call parameter contract lookup must consume semantic function lookup seam"

grep -q 'semantic_find_function_decl_by_name(ctx, display_name)' \
    src/semantic/type_checker_call_generic_where.c \
    || fail "call generic where validation must consume semantic function lookup seam"

if grep -q 'ASTNode \*prog = ctx->program_root' \
    src/semantic/type_checker_call_contract_helpers.c; then
    fail "call parameter contract lookup must not scan raw program root"
fi

if grep -q 'ctx->program_root' \
    src/semantic/type_checker_call_generic_where.c; then
    fail "call generic where validation must not scan raw program root"
fi

grep -q 'semantic_find_callable_decl_by_name(ctx, callee_name)' \
    src/semantic/type_checker_async_channel.c \
    || fail "spawn boundary validation must consume context-bearing callable lookup seam"

if grep -q 'spawn_find_callable_decl' \
    src/semantic/type_checker_async_channel.c; then
    fail "spawn boundary validation must not carry a local callable lookup"
fi

grep -q 'semantic_host_decl_for_type(ctx, type)' \
    src/semantic/type_checker_host_helpers.c \
    || fail "subject type classification must consume semantic host decl seam"

grep -q 'semantic_find_enum_decl_by_name(ctx, type->name)' \
    src/semantic/type_checker_host_helpers.c \
    || fail "semantic host type classification must use enum owner lookup seam"

grep -q 'semantic_find_enum_decl_by_name(ctx, enum_name)' \
    src/semantic/type_checker_expr_enum.c \
    || fail "enum expression projection must consume semantic enum lookup seam"

grep -q 'semantic_find_enum_decl_by_name(ctx, type->name)' \
    src/semantic/type_checker_flow_match.c \
    || fail "match enum analysis must consume semantic enum lookup seam"

if grep -q 'ast_program_statement_count(ctx->program_root)' \
    src/semantic/type_checker_expr_enum.c \
    src/semantic/type_checker_flow_match.c; then
    fail "enum expression/match consumers must not scan raw program root"
fi

for host_lookup_term in \
    'semantic_find_class_decl_by_name(ctx, type->name)' \
    'semantic_find_party_decl_by_name(ctx, type->name)' \
    'semantic_find_roster_decl_by_name(ctx, type->name)' \
    'semantic_find_world_decl_by_name(ctx, type->name)' \
    'semantic_find_zone_decl_by_name(ctx, type->name)' \
    'semantic_find_relation_decl_by_name(ctx, type->name)' \
    'semantic_find_effect_decl_by_name(ctx, type->name)'; do
    grep -q "$host_lookup_term" src/semantic/type_checker_host_helpers.c \
        || fail "semantic host type classification must use owner lookup seam: $host_lookup_term"
done

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration include validation must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration impl validation must consume AST impl-ability accessor"

grep -q 'ast_impl_ability_method(impl, j)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration impl method validation must consume AST impl-ability accessor"

grep -q 'any_subject_role_has_ability(SemanticContext \*ctx' \
    src/semantic/type_checker_domain_role_lookup.c \
    || fail "subject-bound role lookup must consume SemanticContext"

grep -q 'semantic_host_decl_for_type(ctx, resolved_type)' \
    src/semantic/type_checker_domain_role_lookup.c \
    || fail "subject-bound role lookup must consume semantic host decl seam"

grep -q 'role_lookup_program(ctx)' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role lookup owner must centralize context program access"

grep -q 'semantic_find_type_alias_decl_by_name(SemanticContext \*ctx' \
    src/semantic/type_checker_host_lookup.c \
    || fail "type-alias lookup must live behind the semantic host lookup seam"

grep -q 'stage_lookup_program(ctx)' src/semantic/type_checker_resolution_stage_lookup.c \
    || fail "DAG stage lookup owner must centralize context program access"

grep -q 'semantic_host_index_find_top_level_decl_by_label(ctx, label, kind)' \
    src/semantic/type_checker_resolution_stage_lookup.c \
    || fail "DAG stage context lookup must consume the host declaration index first"

grep -q 'semantic_host_index_find_top_level_decl_by_label(' \
    src/semantic/type_checker_host_index.c \
    || fail "host declaration index must expose DAG top-level label lookup"

grep -q 'legacy_ast_param_summary_program(ctx)' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "legacy AST param-summary owner must name its program-root seam explicitly"

grep -q 'semantic_callable_param_escape_summary' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call contract summary owner must expose the canonical escape-summary seam"

grep -q 'semantic_callable_summary_proves_no_ref_escape' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call contract summary owner must check typed body-summary facts before legacy AST analysis"

grep -q 'type_function_body_summary' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call contract summary owner must consume checked function body summaries"

grep -q 'type_function_has_body_summary' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call contract summary owner must distinguish absent summary facts from checked empty summaries"

grep -q 'BODY_SUMMARY_MAY_ESCAPE_REF' \
    src/semantic/type_checker_call_contract_helpers.c \
    || fail "call contract summary owner must preserve the ref-escape body-summary bit"

if grep -RIn 'semantic_legacy_ast_callable_param_escape_summary' src/semantic >/dev/null; then
    fail "call contract escape summary must not reintroduce the legacy AST public seam name"
fi

if grep -q 'constructor_decl_for_symbol_kind(ASTNode \*program' \
    src/semantic/type_checker_internal.h; then
    fail "constructor declaration lookup must expose only the SemanticContext-backed host seam"
fi

if grep -q 'ASTNode \*.*(ASTNode \*program' \
    src/semantic/type_checker_internal.h; then
    fail "semantic internal header must not expose raw program-root lookup helpers"
fi

if grep -RInE '(^|[^A-Za-z0-9_])(find_type_decl_by_name|find_ability_decl_by_name|find_callable_decl_by_name)\(' \
    src/semantic \
    | grep -v 'src/semantic/type_checker_host_lookup.c' >/dev/null; then
    fail "raw class/ability/callable program-root lookup helpers must stay private to host helper owner names"
fi

if grep -RInE '(^|[^A-Za-z0-9_])find_domain_decl_by_name\(' src/semantic \
    | grep -v 'src/semantic/type_checker_host_lookup.c' >/dev/null; then
    fail "raw domain declaration lookup helper must stay private to host helper owner names"
fi

if grep -q 'find_type_alias_decl(ASTNode \*program' \
    src/semantic/type_checker_internal.h; then
    fail "type-alias declaration lookup must expose only the SemanticContext-backed resolver seam"
fi

grep -q 'semantic_build_host_decl_index(ctx, program)' \
    src/semantic/type_checker_program.c \
    || fail "semantic program owner must build the host declaration index before DAG precollect"

grep -q 'semantic_host_index_find_decl_by_name(ctx, AST_TYPE_ALIAS' \
    src/semantic/type_checker_host_lookup.c \
    || fail "type-alias lookup must consume the host declaration index"

if grep -q 'ast_program_statement_count(program)' \
    src/semantic/type_checker_resolution_helpers.c; then
    fail "resolution helpers must not rescan program root for type-alias declarations"
fi

for host_lookup in \
    host_find_type_decl_by_name \
    host_find_ability_decl_by_name \
    host_find_callable_decl_by_name \
    host_find_enum_decl_by_name \
    host_find_function_decl_by_name; do
    grep -q "$host_lookup" src/semantic/type_checker_host_lookup.c \
        || fail "host lookup owner missing private lookup seam: $host_lookup"
done

if grep -RIn 'find_subject_host_decl_by_name' src/semantic >/dev/null; then
    fail "dead raw subject-host declaration lookup helper must not reappear"
fi

if grep -q 'stdlib_use_program(ctx)\|ctx->program_root' \
    src/semantic/type_checker_stdlib_use.c; then
    fail "stdlib use validation must use context-local use inventory, not program-root scans"
fi

grep -q 'semantic_resolve_projection_source_field_path(SemanticContext \*ctx' \
    src/semantic/type_checker_projection_path.c \
    || fail "projection path owner must expose only the SemanticContext-backed seam"

if grep -q 'builtin_query_domain_program(ctx)\|ctx->program_root' \
    src/semantic/type_checker_builtins_query_domain.c; then
    fail "domain builtin query owner must consume SemanticContext lookup seams, not program-root scans"
fi

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_domain_role_lookup.c; then
    fail "subject-bound role lookup must not reopen direct subject-host lookup"
fi

grep -q 'any_subject_role_has_ability(ctx, ab_type)' src/semantic/type_checker_party_decl.c \
    || fail "party role-slot validation must pass SemanticContext to subject role lookup"

if grep -q 'ctx->program_root != NULL' src/semantic/type_checker_party_decl.c; then
    fail "party role-slot validation must rely on context-bearing ability seams"
fi

if grep -q 'ctx == NULL || ctx->program_root == NULL || zone_name == NULL' \
    src/semantic/type_checker_resolution_stage_domain.c; then
    fail "DAG domain staging must rely on context-bearing zone lookup seam"
fi

if grep -q 'ctx == NULL || ctx->program_root == NULL || label == NULL' \
    src/semantic/type_checker_resolution_stage_lookup.c; then
    fail "DAG graph host lookup must rely on context-bearing host lookup seams"
fi

grep -q 'semantic_find_class_decl_by_name(' \
    src/semantic/type_checker_ownership_let.c \
    || fail "let ownership annotation where-check must consume semantic class lookup seam"

grep -q 'Void expression cannot initialize local binding' \
    src/semantic/type_checker_ownership_let.c \
    || fail "let binding must reject Void expressions as value sources"

grep -q 'Void function return must not carry a Void expression value' \
    src/semantic/type_checker_ownership_return.c \
    || fail "return checking must reject Void expression values in Void functions"

grep -q 'Void expression cannot be assigned as a value' \
    src/semantic/type_checker_assignment.c \
    || fail "assignment checking must reject Void expressions as value sources"

grep -q 'Void expression cannot be passed as a call argument' \
    src/semantic/type_checker_helpers_late.c \
    || fail "call checking must reject Void expressions before generic argument inference"

grep -q 'Void expression cannot be stored as an array literal element' \
    src/semantic/type_checker_expr_ops.c \
    || fail "array literal checking must reject Void element values"

grep -q 'Void expression cannot be stored as a tuple literal element' \
    src/semantic/type_checker_expr.c \
    || fail "tuple literal checking must reject Void element values"

grep -q 'Void expression cannot initialize constructor field' \
    src/semantic/type_checker_call_constructor.c \
    || fail "constructor checking must reject Void field initializer values"

grep -q 'type_name_or_unknown(elem_type)' \
    src/semantic/type_checker_expr_ops.c \
    || fail "array literal diagnostics must not dereference unresolved element types"

if grep -q 'find_type_decl_by_name(ctx->program_root' \
    src/semantic/type_checker_ownership_let.c; then
    fail "let ownership annotation where-check must not reopen raw type-decl lookup"
fi

if grep -q 'ctx->program_root == NULL' \
    src/semantic/type_checker_resolution_metadata_constructed.c; then
    fail "constructed metadata materialization must rely on semantic class lookup seam"
fi

grep -q 'semantic_find_callable_decl_by_name(ctx, callee_name)' \
    src/semantic/type_checker_async_channel.c \
    || fail "spawn token-boundary validation must consume semantic callable lookup seam"

grep -q 'ast_async_func_name(stmt)' src/semantic/type_checker_host_lookup.c \
    || fail "semantic callable lookup must include async function declarations"

grep -q 'host_lookup_program(ctx)' src/semantic/type_checker_host_lookup.c \
    || fail "semantic host lookup owner must centralize context program access"

if grep -q 'program = ctx->program_root' src/semantic/type_checker_async_channel.c; then
    fail "spawn token-boundary validation must not reopen raw program-root scan"
fi

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_resolution_graph_decl_participants.c \
    || fail "DAG role include precollect must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_resolution_graph_decl_participants.c \
    || fail "DAG role impl precollect must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_node(role_decl)' src/semantic/type_checker_resolution_graph_decl_participants.c \
    || fail "DAG role host-type precollect must consume the semantic role target-type helper"

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_resolution_stage_nominal.c \
    || fail "DAG stage role include resolution must consume AST include accessor"

grep -q 'semantic_role_for_type_node(decl)' src/semantic/type_checker_resolution_stage_nominal.c \
    || fail "DAG stage role host-type resolution must consume the semantic role target-type helper"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_resolution_stage_nominal.c \
    || fail "DAG stage role impl resolution must consume AST impl-ability accessor"

if grep -R "data\.include_stmt" src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser include payload consumers must use AST include accessors"
fi

if grep -R "data\.array_access" src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser array-access payload consumers must use AST array-access accessors"
fi

if grep -q "data\.identifier\.name" src/semantic/type_checker_lambda_capture.c; then
    fail "lambda capture validation must use ast_identifier_name for identifier payloads"
fi

for path in \
    src/compiler/air_boundary.c \
    src/compiler/hir_analysis.c \
    src/compiler/hir_cfg_phi.c \
    src/compiler/mir_call_fact.c
do
    if grep -q "data\.identifier\.name" "$path"; then
        fail "$path must use ast_identifier_name for identifier payloads"
    fi
done

for path in \
    src/compiler/dir_collect_intent.c \
    src/compiler/mir.c \
    src/compiler/mir_intent.c \
    src/compiler/mir_ssa_rename.c \
    src/compiler/mir_type_helpers.c \
    src/compiler/module_normalizer_refs.c \
    src/compiler/rir_facts.c \
    src/compiler/rir_validation.c \
    src/compiler/mir_source_shape.c \
    src/compiler/mir_stmt_source.c \
    src/semantic/slot_analyzer_escape.c \
    src/semantic/slot_analyzer_summary.c \
    src/semantic/type_checker_assignment.c \
    src/semantic/type_checker_async_channel.c \
    src/semantic/type_checker_async_decl.c \
    src/semantic/type_checker.c \
    src/semantic/type_checker_builtins_device_slot.c \
    src/semantic/type_checker_builtins_nominal.c \
    src/semantic/type_checker_builtins_projection.c \
    src/semantic/type_checker_builtins_query.c \
    src/semantic/type_checker_builtins_query_world.c \
    src/semantic/type_checker_builtins_secure_token.c \
    src/semantic/type_checker_builtins_slotops.c \
    src/semantic/type_checker_builtins_slotops_view.c \
    src/semantic/type_checker_builtins_stdlib_body.c \
    src/semantic/type_checker_call_constructor.c \
    src/semantic/type_checker_event.c \
    src/semantic/type_checker_expr_names.c \
    src/semantic/type_checker_flow_match.c \
    src/semantic/type_checker_generic_support.c \
    src/semantic/type_checker_helpers_late.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_intent_contract_summary.c \
    src/semantic/type_checker_intent_control.c \
    src/semantic/type_checker_intent_decl.c \
    src/semantic/type_checker_intent_on_inference.c \
    src/semantic/type_checker_intent_role_fields.c \
    src/semantic/type_checker_intent_transfer.c \
    src/semantic/type_checker_ownership_destructure.c \
    src/semantic/type_checker_ownership_let.c \
    src/semantic/type_checker_ownership_let_helpers.c \
    src/semantic/type_checker_ownership_let_slot_claim.c \
    src/semantic/type_checker_qubit.c \
    src/semantic/type_checker_resolution_helpers.c \
    src/semantic/type_checker_slot_view_active.c \
    src/semantic/type_checker_world_embedding.c \
    src/semantic/type_infer.c
do
    if grep -q "data\.identifier\.name" "$path"; then
        fail "$path must use ast_identifier_name for identifier payloads"
    fi
done

for path in \
    src/codegen/llvm_expr_array_calls.c \
    src/codegen/llvm_expr_aggregate_utils.c \
    src/codegen/llvm_expr_call_collections_require.c \
    src/codegen/llvm_expr_call_collections_extended.c \
    src/codegen/llvm_expr_call_methods_domain_slice.c \
    src/codegen/llvm_expr_call_methods_vtable_dispatch.c \
    src/codegen/llvm_expr_assignment_member_projection.c \
    src/codegen/llvm_expr_boundary_projection_helpers.c \
    src/codegen/llvm_expr_call_dispatch.c \
    src/codegen/llvm_expr_channel.c \
    src/codegen/llvm_expr_call_projection_sync.c \
    src/codegen/llvm_expr_collection_base_calls.c \
    src/codegen/llvm_expr.c \
    src/codegen/llvm_expr_common.c \
    src/codegen/llvm_expr_domain_query_utils.c \
    src/codegen/llvm_expr_event_calls.c \
    src/codegen/llvm_expr_assignment_projection.c \
    src/codegen/llvm_expr_identifier_slot_helpers.c \
    src/codegen/llvm_expr_member_access.c \
    src/codegen/llvm_expr_member_lvalue.c \
    src/codegen/llvm_expr_projection_path_helpers.c \
    src/codegen/llvm_expr_rc_calls.c \
    src/codegen/llvm_expr_slot_device_calls.c \
    src/codegen/llvm_expr_spawn_call_helpers.c \
    src/codegen/llvm_expr_call_queue_extended.c \
    src/codegen/llvm_expr_task_channel_calls.c \
    src/codegen/llvm_intent_emit_support.c \
    src/codegen/llvm_mir_cfg_control.c \
    src/codegen/llvm_mir_for_in_control.c \
    src/codegen/llvm_mir_local_emit.c \
    src/codegen/llvm_member_call_emit.c \
    src/codegen/llvm_stmt_let_helpers.c \
    src/codegen/llvm_stmt_let_slots.c \
    src/codegen/llvm_stmt_loop_match.c \
    src/codegen/llvm_stmt_parallel_async.c \
    src/codegen/llvm_stmt_type_infer_await.c \
    src/codegen/llvm_stmt_type_infer_nominal.c \
    src/codegen/transpiler_call_constructor_result_emit.c \
    src/codegen/transpiler_call_result_option_builtin_emit.c \
    src/codegen/transpiler_channel_type_query.c \
    src/codegen/transpiler_destructure_emit.c \
    src/codegen/transpiler_domain_receiver_query.c \
    src/codegen/transpiler_event_builtin_emit.c \
    src/codegen/transpiler_expr_call_user_emit.h \
    src/codegen/transpiler_expr_core_builtins_emit.c \
    src/codegen/transpiler_expr_core_builtins_emit.h \
    src/codegen/transpiler_expr_projection_builtin.c \
    src/codegen/transpiler_expr_dispatch_emit.c \
    src/codegen/transpiler_expr_stdlib_builtin.c \
    src/codegen/transpiler_expr_stdlib_builtin.h \
    src/codegen/transpiler_func_class_flow_emit.c \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_generic_binding_query.c \
    src/codegen/transpiler_generic_param_query.c \
    src/codegen/transpiler_helpers_core_b.h \
    src/codegen/transpiler_intent_context.c \
    src/codegen/transpiler_lambda_emit.c \
    src/codegen/transpiler_let_box_emit.h \
    src/codegen/transpiler_let_slot_emit.c \
    src/codegen/transpiler_match_emit.c \
    src/codegen/transpiler_mir_assignment_emit.h \
    src/codegen/transpiler_mir_block_emit.c \
    src/codegen/transpiler_mir_destructure_emit.c \
    src/codegen/transpiler_mir_local_binding.c \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_local_type_lookup.c \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_pending_uses.c \
    src/codegen/transpiler_nominal.c \
    src/codegen/transpiler_parallel_capture.c \
    src/codegen/transpiler_projection_field_path.c \
    src/codegen/transpiler_projection_method_invalidation.c \
    src/codegen/transpiler_projection_sync.h \
    src/codegen/transpiler_select.c \
    src/codegen/transpiler_slot_builtin_emit.c \
    src/codegen/transpiler_slot_target.c \
    src/codegen/transpiler_spawn_channel_emit.c \
    src/codegen/llvm_stmt_let_callable.c \
    src/codegen/llvm_stmt_let_collections.c \
    src/codegen/llvm_stmt_let_resources.c \
    src/codegen/llvm_stmt_let_with.c \
    src/codegen/llvm_stmt_zone_action.c
do
    if grep -q "data\.identifier\.name" "$path"; then
        fail "$path must use ast_identifier_name for identifier payloads"
    fi
done

if grep -R "data\.identifier\.name" src/codegen >/dev/null; then
    fail "codegen must use ast_identifier_name for identifier payloads"
fi

if grep -R "data\.program\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser program payload consumers must use AST program accessors/mutators"
fi

if grep -R "data\.member\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser member-access payload consumers must use AST member accessors"
fi

if grep -R "data\.assignment\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser assignment payload consumers must use AST assignment accessors"
fi

if grep -R "data\.let_destructure\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser let-destructure payload consumers must use AST destructure accessors"
fi

if grep -R "data\.party_instance\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser party-instance payload consumers must use AST party-instance accessors"
fi

if grep -R "data\.use_decl\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser use-declaration payload consumers must use AST use accessors"
fi

if grep -R "data\.import_decl\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser import-declaration payload consumers must use AST import accessors"
fi

if grep -R "data\.namespace_decl\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser namespace-declaration payload consumers must use AST namespace accessors"
fi

if grep -R "data\.bind_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser bind-statement payload consumers must use AST bind accessors"
fi

if grep -R "data\.context_access\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser context-access payload consumers must use AST context accessors"
fi

if grep -R "data\.override_func\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser override-function payload consumers must use AST override accessors"
fi

if grep -R "data\.await_expr\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser await-expression payload consumers must use AST await accessors"
fi

if grep -R "data\.channel_\(send\|recv\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser channel payload consumers must use AST channel accessors"
fi

if grep -R "data\.\(channel_type\|future_type\)\.\(element_type\|capacity\|value_type\)" src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser channel/future type consumers must use AST async type accessors"
fi

if grep -R "data\.unary\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser unary payload consumers must use AST unary accessors"
fi

if grep -R "data\.binary\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser binary payload consumers must use AST binary accessors"
fi

if grep -R "data\.\(array_literal\|tuple_literal\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser literal payload consumers must use AST literal accessors"
fi

if grep -R "data\.type\.tuple_\(elements\|element_count\)" src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser tuple-type consumers must use AST type tuple accessors"
fi

if grep -R "data\.defer_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser defer payload consumers must use AST defer accessors"
fi

if grep -R "data\.return_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser return payload consumers must use AST return accessors"
fi

if grep -R "data\.unsafe_block\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser unsafe-block payload consumers must use AST unsafe-block accessors"
fi

if grep -R "data\.\(break_stmt\|continue_stmt\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser loop-control payload consumers must use AST loop-control accessors"
fi

if grep -R "data\.while_loop\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser while-loop payload consumers must use AST while-loop accessors"
fi

if grep -R "data\.for_loop\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser for-loop payload consumers must use AST for-loop accessors"
fi

if grep -R "data\.task_group\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser task-group payload consumers must use AST task-group accessors"
fi

if grep -R "data\.spawn_expr\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser spawn-expression payload consumers must use AST spawn accessors"
fi

if grep -R "data\.async_block\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser async-block payload consumers must use AST async-block accessors"
fi

if grep -R "data\.select_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser select-statement payload consumers must use AST select accessors"
fi

if grep -R "data\.parallel\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser parallel-block payload consumers must use AST parallel accessors"
fi

if grep -R "data\.with_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser with-statement payload consumers must use AST with-statement accessors"
fi

if grep -R "data\.block\." src/semantic src/compiler src/codegen \
    | grep -v "src/compiler/hir_destroy.c" >/dev/null; then
    fail "non-parser block payload consumers must use AST block accessors outside parser-owned HIR teardown"
fi

if grep -R "data\.\(match_stmt\|match_case\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser match payload consumers must use AST match accessors"
fi

if grep -R "data\.lambda_expr\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser lambda payload consumers must use AST lambda accessors"
fi

if grep -R "data\.\(event_op\|event_invoke\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser event operation/invoke payload consumers must use AST event accessors"
fi

if grep -R "data\.let_decl\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser let-declaration payload consumers must use AST let accessors"
fi

if grep -R "data\.\(number\|string\|boolean\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser scalar literal payload consumers must use AST literal accessors"
fi

if grep -R "data\.if_stmt\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser if-statement payload consumers must use AST if-statement accessors"
fi

grep -q 'ast_impl_ability_method(impl, i)' src/compiler/dir_collect.c \
    || fail "DIR role ability method scan must consume AST impl-ability accessor"

grep -q 'ast_impl_ability_method(impl, j)' src/compiler/hir_routines.c \
    || fail "HIR role impl routine collection must consume AST impl-ability accessor"

if grep -R "data\.impl_ability" \
    src/semantic/type_checker_expr_ops.c \
    src/semantic/type_checker_ability_match.c \
    src/compiler/dir_collect.c \
    src/compiler/hir_routines.c >/dev/null; then
    fail "semantic/compiler role impl compatibility paths must use AST impl-ability accessors"
fi

if grep -R "data\.impl_ability" src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser impl ability payload consumers must use AST impl-ability accessors"
fi

if grep -R "data\.role_decl\.\(for_type\|includes\|include_count\|impl_abilities\|impl_count\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser role child-list consumers must use AST role accessors"
fi

if grep -R "data\.ability_decl\.\(name\|methods\|method_count\)" \
    src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen ability consumers must use AST ability accessors"
fi

if grep -R "data\.ability_decl\.\(generic_params\|where_clause\|require_fields\|require_count\|methods\|method_count\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen ability metadata consumers must use AST ability accessors"
fi

if grep -R "data\.\(ability_decl\|role_decl\)\.name" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen ability/role-name consumers must use AST accessors"
fi

if grep -R "data\.\(ability_decl\|role_decl\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen ability/role payload consumers must use AST accessors"
fi

if grep -R "data\.type_alias\.\(name\|target_type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen type-alias consumers must use AST type-alias accessors"
fi

if grep -R "data\.event_decl\.\(name\|params\|param_count\|return_type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen event consumers must use AST event accessors"
fi

if grep -R "data\.extern_block\.\(abi\|declarations\|count\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen extern block consumers must use AST extern accessors"
fi

if grep -R "data\.func_decl\.name" \
    src/compiler/hir.c \
    src/compiler/hir_routines.c \
    src/compiler/mir_decl_headers.c \
    src/compiler/mir_decl_header_validate.c \
    src/codegen/llvm_inventory_decl_lookup.c \
    src/semantic/type_checker_resolution_graph_collect.c \
    src/semantic/type_checker_resolution_graph_inventory.c \
    src/semantic/type_checker_resolution_stage_lookup.c >/dev/null; then
    fail "closed HIR/MIR/DAG/LLVM declaration-name consumers must use AST declaration-name accessors"
fi

if grep -R "data\.func_decl\.name" src/semantic src/codegen >/dev/null; then
    fail "semantic/codegen function-name consumers must use AST declaration-name accessors"
fi

if grep -R "data\.func_decl\.name" src/compiler >/dev/null; then
    fail "compiler function-name consumers must use AST declaration-name accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\|arg_names\|generic_args\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser call payload consumers must use AST call accessors"
fi

grep -Fq "semantic_assignment_path_release" \
    src/semantic/type_checker_assignment_path.c
if grep -Fq "free(base)" src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path owner must release intermediate paths through semantic_assignment_path_release"
fi

grep -Fq "if (!slot_analyze_block(body, sa))" \
    src/semantic/slot_analyzer.c \
    || fail "slot analyzer function bodies must propagate child analysis failure"
grep -Fq "&& !slot_analyze_func_body(stmt, sa)" \
    src/semantic/slot_analyzer.c \
    || fail "slot analyzer program pass must propagate failed function-body analysis"
grep -Fq "if (!slot_analyze_program(ast, sa) && !ctx->has_error)" \
    src/semantic/semantic.c \
    || fail "semantic entry must convert slot analyzer internal failure into a diagnostic"
grep -Fq "semantic_run_legacy_slot_resource_analysis(ASTNode *ast, SemanticContext *ctx)" \
    src/semantic/semantic.c \
    || fail "semantic entry must keep slot analyzer behind an explicit legacy compatibility seam"
grep -Fq "Slot resource-boundary analysis could not allocate state" \
    src/semantic/semantic.c \
    || fail "semantic entry must fail closed when slot analyzer allocation fails"

if grep -R "data\.func_decl\.\(param_count\|params\|return_type\|body\)" \
    src/semantic/slot_analyzer.c \
    src/semantic/slot_analyzer_escape.c \
    src/semantic/slot_analyzer_summary.c \
    src/semantic/type_checker_ability_decl.c \
    src/semantic/type_checker_call_contract_helpers.c \
    src/semantic/type_checker_call_generic_where.c \
    src/semantic/type_checker_async_channel.c \
    src/semantic/type_checker_expr_host.c \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_ops.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_generic_support.c \
    src/semantic/type_checker_helpers_effects.c \
    src/semantic/type_checker_helpers_late.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_intent_action_contract.c \
    src/semantic/type_checker_intent_on_inference.c \
    src/semantic/type_checker_ownership_param_summary.c \
    src/semantic/type_checker_program.c \
    src/semantic/type_checker_resolution_graph_decl.c \
    src/semantic/type_checker_resolution_stage_signature.c \
    src/compiler/debugger.c \
    src/compiler/hir_analysis.c \
    src/compiler/hir_routines.c \
    src/compiler/mir_decl_header_validate.c \
    src/compiler/mir_decl_headers.c \
    src/compiler/mir_non_cfg_stmt_population.c \
    src/compiler/mir_type_helpers.c \
    src/compiler/module_normalizer_refs.c \
    src/compiler/rir_builder.c \
    src/compiler/runtime_none_contract.c \
    src/codegen/llvm_backend_forward_declare.c \
    src/codegen/llvm_decl.c \
    src/codegen/llvm_domain_forward.c \
    src/codegen/llvm_expr_boundary_projection_helpers.c \
    src/codegen/llvm_expr_call_dispatch.c \
    src/codegen/llvm_expr_call_variable.c \
    src/codegen/llvm_expr_identifier_slot_helpers.c \
    src/codegen/llvm_expr_spawn_call_helpers.c \
    src/codegen/llvm_expr_unary_core.c \
    src/codegen/llvm_member_call_emit.c \
    src/codegen/llvm_mir_block_emit.c \
    src/codegen/llvm_mir_cfg_control.c \
    src/codegen/llvm_mir_emit.c \
    src/codegen/llvm_mir_local_emit.c \
    src/codegen/llvm_register.c \
    src/codegen/llvm_stmt.c \
    src/codegen/llvm_stmt_let_helpers.c \
    src/codegen/llvm_stmt_let_callable.c \
    src/codegen/llvm_stmt_type_infer_helpers.c \
    src/codegen/llvm_stmt_type_infer.c \
    src/codegen/llvm_type.c \
    src/codegen/transpiler_async_parallel_emit.c \
    src/codegen/transpiler_extern.c \
    src/codegen/transpiler_class_decl_emit.c \
    src/codegen/transpiler_domain_nominal_emit.c \
    src/codegen/transpiler_domain_nominal_emit.h \
    src/codegen/transpiler_domain_role_ability_emit.c \
    src/codegen/transpiler_domain_role_ability_emit.h \
    src/codegen/transpiler_domain_role_methods_emit.c \
    src/codegen/transpiler_enum_decl_emit.c \
    src/codegen/transpiler_expr_call_member_emit.c \
    src/codegen/transpiler_expr_call_spawn_emit.c \
    src/codegen/transpiler_expr_call_user_emit.h \
    src/codegen/transpiler_expr_type_infer.c \
    src/codegen/transpiler_expr_type_infer.h \
    src/codegen/transpiler_func_class_flow_emit.c \
    src/codegen/transpiler_func_forward_emit.c \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_func_forward_metadata.c \
    src/codegen/transpiler_func_forward_policy.c \
    src/codegen/transpiler_generic_class_specialization_emit.c \
    src/codegen/transpiler_let_emit.c \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_mir_emission_mapping_contract.c \
    src/codegen/transpiler_mir_emit_state.c \
    src/codegen/transpiler_mir_func_emit.c \
    src/codegen/transpiler_mir_func_ssa_locals_emit.h \
    src/codegen/transpiler_mir_local_binding.c \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_local_type_lookup.c \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_signature.c \
    src/codegen/transpiler_projection_method_invalidation.c \
    src/codegen/transpiler_spawn_channel_emit.c \
    src/codegen/transpiler_specialization_registry.h \
    src/codegen/transpiler_type_declarator.c >/dev/null; then
    fail "closed function signature/body slice must use AST function accessors"
fi

if grep -R "data\.func_decl\.\(param_count\|params\|return_type\|body\)" \
    src/semantic src/codegen >/dev/null; then
    fail "semantic/codegen function signature/body consumers must use AST function accessors"
fi

if grep -R "data\.func_decl\.\(param_count\|params\|return_type\|body\)" src/compiler >/dev/null; then
    fail "compiler function signature/body consumers must use AST function accessors"
fi

if grep -R "data\.block\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen block payload consumers must use AST block accessors/mutators"
fi

if grep -R "data\.func_decl\.\(generic_params\|where_clause\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen function generic/where consumers must use AST function accessors"
fi

if grep -R "data\.async_func_decl\.\(name\|param_count\|params\|return_type\|body\|generic_params\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen async function metadata consumers must use AST async function accessors"
fi

if grep -R "data\.event_handler_type\.\(param_count\|param_types\|return_type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen event-handler type consumers must use AST event-handler type accessors"
fi

if grep -R "data\.type\.\(name\|generic_args\)" \
    src/semantic/type_checker_ability_ref.c \
    src/semantic/type_checker_ability_match.c \
    src/semantic/type_checker_ability_where.c >/dev/null; then
    fail "ability contract helpers must use AST type accessors for type-ref names and generic args"
fi

if grep -R "data\.type\.\(name\|generic_args\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen AST_TYPE consumers must use AST type accessors"
fi

if grep -R "data\.call\.generic_args" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen call generic-argument consumers must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/compiler/air_boundary.c \
    src/compiler/air_boundary_walk.c \
    src/compiler/air_evidence_ast.c >/dev/null; then
    fail "AIR call traversal owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/compiler/hir_analysis.c >/dev/null; then
    fail "HIR analysis call traversal owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/compiler/mir_stmt_source.c \
    src/compiler/mir.c \
    src/compiler/mir_call_fact.c \
    src/compiler/mir_source_shape.c \
    src/compiler/mir_ssa_rename.c \
    src/compiler/mir_type_helpers.c \
    src/compiler/module_normalizer_refs.c \
    src/compiler/rir_builder_walk.c \
    src/compiler/rir_facts.c \
    src/compiler/rir_validation.c \
    src/compiler/runtime_none_contract.c >/dev/null; then
    fail "compiler source/runtime scanning call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/slot_analyzer_escape.c \
    src/semantic/slot_analyzer_summary.c >/dev/null; then
    fail "slot analyzer call traversal owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_async_channel.c \
    src/semantic/type_checker_builtins_channel_state.c \
    src/semantic/type_checker_builtins_intent_observability.c >/dev/null; then
    fail "semantic async/channel/observability call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_projection.c \
    src/semantic/type_checker_builtins_query.c >/dev/null; then
    fail "semantic projection/query builtin call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_query_channel.c >/dev/null; then
    fail "semantic channel query builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_query_world.c >/dev/null; then
    fail "semantic world query builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_nominal.c >/dev/null; then
    fail "semantic nominal builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_slotops.c \
    src/semantic/type_checker_builtins_slotops_view.c >/dev/null; then
    fail "semantic slotops builtin call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_state_tools.c >/dev/null; then
    fail "semantic state-tool builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_stdlib_scalar.c >/dev/null; then
    fail "semantic stdlib scalar builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_builtins_stdlib_map.c >/dev/null; then
    fail "semantic stdlib map builtin call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_lambda_capture.c >/dev/null; then
    fail "semantic lambda capture call owner must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_host.c \
    src/semantic/type_checker_builtins_stdlib_variant.c \
    src/semantic/type_checker_call_constructor.c \
    src/semantic/type_checker_flow_match.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_intent_decl.c \
    src/semantic/type_checker_intent_control.c \
    src/semantic/type_checker_intent_on_inference.c \
    src/semantic/type_checker_ownership_destructure.c \
    src/semantic/type_checker_ownership_let.c \
    src/semantic/type_checker_ownership_let_helpers.c \
    src/semantic/type_checker_ownership_let_slot_claim.c \
    src/semantic/type_checker_resolution_graph_body.c \
    src/semantic/type_checker_world_embedding.c \
    src/semantic/type_infer.c >/dev/null; then
    fail "semantic intent/ownership/DAG call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/llvm_stmt_let_callable.c \
    src/codegen/transpiler_call_constructor_result_emit.c \
    src/codegen/transpiler_expr_call_member_emit.c \
    src/codegen/transpiler_expr_call_spawn_emit.c \
    src/codegen/transpiler_expr_call_user_emit.h >/dev/null; then
    fail "codegen callable call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/llvm_stmt_zone_action.c \
    src/codegen/transpiler_projection_sync.c \
    src/codegen/transpiler_projection_sync.h >/dev/null; then
    fail "codegen zone/projection sync call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/llvm_expr_event_calls.c \
    src/codegen/llvm_expr_intent_observability_calls.c \
    src/codegen/llvm_expr_rc_calls.c \
    src/codegen/transpiler_allocator_builtin_emit.c >/dev/null; then
    fail "codegen event/intent-observability/Rc/allocator call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/llvm_expr_call_methods_domain_slice.c \
    src/codegen/llvm_expr_call_dispatch.c \
    src/codegen/llvm_expr_call_collections_extended.c \
    src/codegen/llvm_expr_call_queue_extended.c \
    src/codegen/llvm_expr_call_methods_vtable_dispatch.c \
    src/codegen/llvm_expr_call_variable.c \
    src/codegen/llvm_expr_array_calls.c \
    src/codegen/llvm_expr_collection_base_calls.c \
    src/codegen/llvm_expr_common.c \
    src/codegen/llvm_expr_constructor_calls.c \
    src/codegen/llvm_expr_domain_query_calls.c \
    src/codegen/llvm_expr_domain_query_utils.c \
    src/codegen/llvm_expr_identifier_slot_helpers.c \
    src/codegen/llvm_expr_log_calls.c \
    src/codegen/llvm_expr_math_calls.c \
    src/codegen/llvm_expr_result_option_calls.c \
    src/codegen/llvm_expr_stdlib_scalar_io_calls.c \
    src/codegen/llvm_expr_slot_device_calls.c \
    src/codegen/llvm_expr_task_channel_calls.c \
    src/codegen/llvm_expr_projection_path_helpers.c \
    src/codegen/llvm_expr_spawn_call_helpers.c \
    src/codegen/llvm_member_call_emit.c \
    src/codegen/llvm_mir_cfg_control.c \
    src/codegen/llvm_stmt_let_collections.c \
    src/codegen/llvm_stmt_let_helpers.c \
    src/codegen/llvm_stmt_let_resources.c \
    src/codegen/llvm_stmt_let_slots.c \
    src/codegen/llvm_stmt_let_with.c \
    src/codegen/llvm_stmt_loop_match.c \
    src/codegen/llvm_stmt_type_infer_nominal.c \
    src/codegen/llvm_mir_local_emit.c \
    src/codegen/transpiler_event_builtin_emit.c \
    src/codegen/transpiler_call_result_option_builtin_emit.c \
    src/codegen/transpiler_channel_type_query.c \
    src/codegen/transpiler_domain_receiver_query.c \
    src/codegen/transpiler_expr_core_builtins_emit.c \
    src/codegen/transpiler_expr_core_builtins_emit.h \
    src/codegen/transpiler_expr_projection_builtin.c \
    src/codegen/transpiler_expr_builtin_dispatch.h \
    src/codegen/transpiler_expr_stdlib_builtin.c \
    src/codegen/transpiler_expr_stdlib_builtin.h \
    src/codegen/transpiler_func_class_flow_emit.c \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_helpers_core_b.h \
    src/codegen/transpiler_intent_observability_builtin_emit.c \
    src/codegen/transpiler_generic_binding_query.c \
    src/codegen/transpiler_expr_stdlib_collection_builtin.h \
    src/codegen/transpiler_expr_stdlib_channel_builtin.h \
    src/codegen/transpiler_expr_stdlib_map_builtin.c \
    src/codegen/transpiler_expr_stdlib_misc_builtin.c \
    src/codegen/transpiler_expr_stdlib_queue_builtin.c \
    src/codegen/transpiler_expr_stdlib_scalar_builtin.h \
    src/codegen/transpiler_log_builtin_emit.c \
    src/codegen/transpiler_generic_param_query.c \
    src/codegen/transpiler_let_channel_emit.c \
    src/codegen/transpiler_let_box_emit.h \
    src/codegen/transpiler_let_slot_emit.c \
    src/codegen/transpiler_match_emit.c \
    src/codegen/transpiler_mir_destructure_emit.c \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_local_type_lookup.c \
    src/codegen/transpiler_mir_ssa_contract.h \
    src/codegen/transpiler_parallel_capture.c \
    src/codegen/transpiler_projection_method_invalidation.c \
    src/codegen/transpiler_slot_builtin_emit.c \
    src/codegen/transpiler_slot_target.c >/dev/null; then
    fail "codegen domain/vtable/event/channel/slot call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_pending_uses.c \
    src/codegen/transpiler_spawn_channel_emit.c >/dev/null; then
    fail "C backend MIR/spawn call owners must use AST call accessors"
fi

if grep -R "data\.type\.\(name\|generic_args\)" \
    src/codegen/llvm_internal_api.h \
    src/codegen/llvm_backend_ast_type.c \
    src/codegen/llvm_backend_forward_declare.c \
    src/codegen/llvm_backend_type_registry.c \
    src/codegen/llvm_backend_type_render.c \
    src/codegen/llvm_boundary_slot_param.c \
    src/codegen/llvm_decl.c \
    src/codegen/llvm_domain_forward.c \
    src/codegen/llvm_domain_lookup.c \
    src/codegen/llvm_domain_projection_value_helpers.c \
    src/codegen/llvm_domain_projection_sync_body_helpers.c \
    src/codegen/llvm_domain_role_lookup.c \
    src/codegen/llvm_expr_call_dispatch.c \
    src/codegen/llvm_expr_identifier_slot_helpers.c \
    src/codegen/llvm_expr_projection_path_helpers.c \
    src/codegen/llvm_expr_spawn_call_helpers.c \
    src/codegen/llvm_intent_flow.c \
    src/codegen/llvm_intent_cleanup.c \
    src/codegen/llvm_intent_setup.c \
    src/codegen/llvm_intent_step_context.c \
    src/codegen/llvm_member_call_emit.c \
    src/codegen/llvm_mir_local_emit.c \
    src/codegen/llvm_mir_type_helpers.c \
    src/codegen/llvm_stmt_let_collections.c \
    src/codegen/llvm_stmt_let_helpers.c \
    src/codegen/llvm_stmt_let_resources.c \
    src/codegen/llvm_stmt_let_slots.c \
    src/codegen/llvm_stmt_let_with.c \
    src/codegen/llvm_stmt_type_render.c \
    src/codegen/llvm_stmt_type_infer.c \
    src/codegen/llvm_stmt_with.c \
    src/codegen/llvm_stmt_zone_action.c \
    src/codegen/llvm_type.c \
    src/compiler/dir.c \
    src/compiler/dir_collect_domain.c \
    src/compiler/dir_collect_intent.c \
    src/compiler/hir_analysis.c \
    src/compiler/mir_intent.c \
    src/compiler/mir_type_helpers.c \
    src/compiler/rir_builder.c \
    src/compiler/rir_builder_intent.c \
    src/compiler/rir_facts.c \
    src/codegen/transpiler_block_intent_helpers.c \
    src/codegen/transpiler_block_intent_rebind_helpers.c \
    src/codegen/transpiler_decl_host_lookup.c \
    src/codegen/transpiler_decl_lookup.c \
    src/codegen/transpiler_domain_nominal_emit.c \
    src/codegen/transpiler_domain_nominal_emit.h \
    src/codegen/transpiler_domain_role_ability_emit.c \
    src/codegen/transpiler_domain_role_ability_emit.h \
    src/codegen/transpiler_expr_call_member_emit.c \
    src/codegen/transpiler_expr_call_spawn_emit.c \
    src/codegen/transpiler_func_class_flow_emit.c \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_func_forward_policy.c \
    src/codegen/transpiler_generic_class_specialization_emit.c \
    src/codegen/transpiler_intent_context.c \
    src/codegen/transpiler_intent_emit.c \
    src/codegen/transpiler_intent_participant.c \
    src/codegen/transpiler_domain_provenance_emit.h \
    src/codegen/transpiler_intent_zone_slot.c \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_let_emit.c \
    src/codegen/transpiler_let_box_emit.h \
    src/codegen/transpiler_let_slot_emit.c \
    src/codegen/transpiler_overlay_projection.c \
    src/codegen/transpiler_projection.c \
    src/codegen/transpiler_projection_method_invalidation.c \
    src/codegen/transpiler_projection_sync.h \
    src/codegen/transpiler_specialization_registry.h \
    src/codegen/transpiler_type_render.c \
    src/semantic/type_checker_async_channel.c \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_call_generic_where.c \
    src/semantic/type_checker_domain_role_lookup.c \
    src/semantic/type_checker_generic_contracts.c \
    src/semantic/type_checker_generic_validation.c \
    src/semantic/type_checker_helpers_late.c \
    src/semantic/type_checker_intent_helpers.c \
    src/semantic/type_checker_intent_action_contract.c \
    src/semantic/type_checker_intent_contract_summary.c \
    src/semantic/type_checker_intent_participants.c \
    src/semantic/type_checker_intent_role_fields.c \
    src/semantic/type_checker_module_contract.c \
    src/semantic/type_checker_ownership_let.c \
    src/semantic/type_checker_party_decl.c \
    src/semantic/type_checker_projection_path.c \
    src/semantic/type_checker_role_decl.c \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_metadata_constructed.c \
    src/semantic/type_checker_resolution_metadata_dead_end.c \
    src/semantic/type_checker_resolution_metadata_diagnostics.c \
    src/semantic/type_checker_resolution_graph_collect.c \
    src/semantic/type_checker_resolution_graph_core.c \
    src/semantic/type_checker_resolution_graph_domain.c \
    src/semantic/type_checker_resolution_stage_signature.c \
    src/semantic/type_checker_resolution_stage_stats.c \
    src/semantic/type_checker_type_constraint.c \
    src/semantic/type_infer.c >/dev/null; then
    fail "LLVM/DIR/MIR type rendering and registry helpers must use AST type accessors"
fi

if grep -R "data\.require_field\.\(name\|type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen require-field consumers must use AST require-field accessors"
fi

if grep -R "data\.role_decl\.\(generic_params\|where_clause\|parallel_block\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen role metadata consumers must use AST role accessors"
fi

if grep -R "data\.role_decl\.name" \
    src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen role-name consumers must use AST role accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.name" \
    src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen party/roster-name consumers must use AST domain name accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.name" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen party/roster-name consumers must use AST domain name accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.\(role_slots\|role_count\|party_slots\|party_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen party/roster child-list consumers must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.name" \
    src/compiler/dir_collect.c \
    src/compiler/dir_collect_domain.c \
    src/compiler/hir.c \
    src/compiler/hir_routines.c \
    src/compiler/mir_decl_headers.c \
    src/compiler/mir_decl_header_validate.c \
    src/compiler/rir_builder.c \
    src/compiler/rir_builder_intent.c \
    src/compiler/rir_facts.c \
    src/codegen/llvm_domain_decl_parts_helpers.c \
    src/codegen/llvm_inventory_decl_lookup.c \
    src/codegen/transpiler_decl_lookup.c \
    src/codegen/transpiler_relation_effect_emit.c \
    src/codegen/transpiler_world_select_event_emit.c \
    src/codegen/transpiler_zone_decl_emit.c >/dev/null; then
    fail "closed world/relation/effect/zone name consumers must use AST domain name accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(shared_fields\|shared_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/llvm_domain_decl_parts_helpers.c >/dev/null; then
    fail "LLVM domain decl parts helper must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.name" \
    src/semantic/type_checker_builtins_query.c \
    src/semantic/type_checker_builtins_query_world.c \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_domain_contracts.c \
    src/semantic/type_checker_domain_projection.c \
    src/semantic/type_checker_decls_domain_helpers.c \
    src/semantic/type_checker_effect_decl.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_relation_decl.c \
    src/semantic/type_checker_resolution_graph_collect.c \
    src/semantic/type_checker_resolution_graph_domain.c \
    src/semantic/type_checker_resolution_graph_labels.c \
    src/semantic/type_checker_resolution_graph_zone_commands.c \
    src/semantic/type_checker_resolution_graph_zone_tail.c \
    src/semantic/type_checker_resolution_graph_world.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_resolution_stage_lookup.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_world_decl.c \
    src/semantic/type_checker_program.c \
    src/semantic/type_checker_intent_decl.c \
    src/semantic/type_checker_zone_decl.c \
    src/semantic/type_checker_zone_decl_authority.c \
    src/semantic/type_checker_zone_projection_rules.c \
    src/semantic/type_checker_zone_state.c \
    src/semantic/type_checker_zone_shape.c >/dev/null; then
    fail "closed semantic domain-name lookup consumers must use AST domain name accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\)\.\(slots\|slot_count\|refreshes\|refresh_count\)" \
    src/semantic/type_checker_domain_contracts.c >/dev/null; then
    fail "domain contract relation/effect slot scans must use AST domain child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\|zone_decl\)\.\(slots\|slot_count\|shared_fields\|shared_count\|methods\|method_count\|authorities\|authority_count\|layer_slots\|layer_slot_count\)" \
    src/semantic/type_checker_resolution_stage_domain_decl.c >/dev/null; then
    fail "DAG domain stage child-list scans must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(zones\|zone_count\|slots\|slot_count\|shared_fields\|shared_count\|methods\|method_count\|authorities\|authority_count\|layer_slots\|layer_slot_count\)" \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_host_resource.c >/dev/null; then
    fail "semantic host overlay helpers must use AST domain child accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|methods\|method_count\|is_struct\)" \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_host_resource.c >/dev/null; then
    fail "semantic host helpers must use AST nominal accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(zones\|zone_count\|states\|state_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_effects\|maintained_effect_count\|maintained_relations\|maintained_relation_count\|maintained_states\|maintained_state_count\|activations\|activate_count\|deactivations\|deactivate_count\|maintained_zones\|maintained_zone_count\)" \
    src/semantic/type_checker_resolution_stage_domain.c >/dev/null; then
    fail "DAG domain local-contract staging must use AST domain child accessors"
fi

if grep -R "data\.world_decl\.\(rosters\|roster_count\|zones\|zone_count\|activations\|activate_count\|deactivations\|deactivate_count\|maintained_zones\|maintained_zone_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_world_decl.c >/dev/null; then
    fail "world declaration validator must use AST world child accessors"
fi

if grep -R "data\.world_decl\.\(rosters\|roster_count\|zones\|zone_count\|shared_fields\|shared_count\|states\|state_count\|activations\|activate_count\|maintained_zones\|maintained_zone_count\|deactivations\|deactivate_count\)" \
    src/codegen/transpiler_world_select_event_emit.c >/dev/null; then
    fail "C world emission must use AST world child accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.name" \
    src/compiler/dir_collect.c >/dev/null; then
    fail "DIR nominal collection must use AST nominal name accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|methods\|method_count\)" \
    src/compiler/hir_routines.c src/compiler/mir_decl_headers.c >/dev/null; then
    fail "HIR/MIR nominal method collection must use AST nominal accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|method_count\|nominal_kind\)" \
    src/compiler/mir_decl_header_validate.c >/dev/null; then
    fail "MIR decl header nominal validation must use AST nominal accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.name" \
    src/codegen/llvm_inventory_decl_lookup.c src/codegen/transpiler_decl_lookup.c >/dev/null; then
    fail "C/LLVM declaration lookup must use AST nominal name accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(methods\|method_count\)" \
    src/codegen/llvm_inventory_host_methods.c \
    src/codegen/transpiler_decl_method_view.c >/dev/null; then
    fail "C/LLVM hosted method views must use AST nominal method accessors"
fi

if grep -R "data\.class_decl\.\(fields\|field_count\)" \
    src/semantic/type_checker_projection_path.c \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_call_constructor.c >/dev/null; then
    fail "semantic projection/constructor helpers must use AST class field accessors"
fi

if grep -R "data\.class_decl\.\(name\|fields\|field_count\|nominal_kind\|is_struct\)" \
    src/semantic/type_checker_domain_projection.c \
    src/semantic/type_checker_builtins_projection.c \
    src/codegen/llvm_domain_projection_value_helpers.c \
    src/codegen/llvm_expr_projection_path_helpers.c >/dev/null; then
    fail "projection semantic/LLVM helpers must use AST class accessors"
fi

if grep -R "data\.class_decl\.\(fields\|field_count\|nominal_kind\|is_struct\)" \
    src/codegen/transpiler_projection.c \
    src/codegen/transpiler_projection_field_path.c \
    src/codegen/transpiler_overlay_projection.c >/dev/null; then
    fail "C projection helpers must use AST class accessors"
fi

if grep -R "data\.enum_decl\.\(variants\|variant_count\)" \
    src/semantic/type_checker_flow_match_coverage.c >/dev/null; then
    fail "match coverage must use AST enum variant accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|fields\|field_count\|methods\|method_count\|variants\|variant_count\)" \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_graph_decl.c \
    src/semantic/type_checker_resolution_stage_lookup.c \
    src/semantic/type_checker_resolution_graph_collect.c >/dev/null; then
    fail "DAG nominal resolution owners must use AST nominal accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|fields\|field_count\|methods\|method_count\|variants\|variant_count\|variant_params\|variant_param_counts\|nominal_kind\|is_struct\)" \
    src/compiler src/codegen src/semantic >/dev/null; then
    fail "semantic/compiler/codegen nominal payload readers must use AST nominal accessors"
fi

grep -q 'ast_replace_declaration_name_copy(stmt, final_name)' src/compiler/module_normalizer.c \
    || fail "module_normalizer.c namespace rewrite must use the AST declaration-name mutator"

grep -q 'ast_replace_identifier_name_copy' src/compiler/module_normalizer_refs.c \
    || fail "module_normalizer_refs.c identifier rewrite must use the AST identifier-name mutator"

if grep -R -E "ast_(func|class|ability|role|party|roster)_generic_params[(]" \
    src/compiler/module_normalizer_refs.c >/dev/null; then
    fail "module normalizer must consume declaration-level generic metadata"
fi

grep -q 'Intentional AST mutation seam' src/compiler/module_normalizer_refs.c \
    || fail "module_normalizer_refs.c world-roster rewrite must stay documented as an AST mutation seam"

grep -q 'ast_world_roster_replace_type_name' src/compiler/module_normalizer_refs.c \
    || fail "module_normalizer_refs.c world-roster rewrite must use the AST world-roster mutator"

grep -q 'ast_roster_slot_replace_party_type' src/compiler/module_normalizer_refs.c \
    || fail "module_normalizer_refs.c roster-slot rewrite must use the AST roster-slot mutator"

if grep -R "data\.roster_slot\.\(slot_name\|party_type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen roster-slot consumers must use AST roster-slot accessors"
fi

if grep -R "data\.role_slot\.\(slot_name\|is_dynamic\|required_abilities\|ability_count\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen role-slot consumers must use AST role-slot accessors"
fi

if grep -R "data\.party_shared\.\(name\|type\|initializer\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen shared-field consumers must use AST party-shared accessors"
fi

if grep -R "data\.domain_slot\.\(slot_name\|type\|is_subject\|is_vessel\|is_tobject\|is_binding\|initializer\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen domain-slot consumers must use AST domain-slot accessors"
fi

if grep -R "data\.zone_layer_slot\.\(slot_name\|layer_type\|is_relation\|is_pool\|pool_capacity\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen zone-layer-slot consumers must use AST zone-layer-slot accessors"
fi

if grep -R "data\.zone_state\.\(state_name\|is_relation\|layer_slot_name\|left_or_target_slot_name\|right_slot_name\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen zone-state consumers must use AST zone-state accessors"
fi

if grep -R "data\.intent_step\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen intent-step consumers must use AST intent-step accessors/mutators"
fi

if grep -R "data\.intent_\(decl\|involves\|value\)\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen intent declaration consumers must use AST intent declaration accessors"
fi

if grep -R "data\.intent_" src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen intent payload consumers must use AST intent accessors/mutators"
fi

if grep -R "data\.intent_step\." \
    src/semantic/type_checker_intent_contract_summary.c \
    src/semantic/type_checker_intent_participants.c \
    src/semantic/type_checker_intent_ability.c \
    src/semantic/type_checker_resolution_graph_intent.c \
    src/semantic/type_checker_resolution_stage_systemic.c >/dev/null; then
    fail "read-only semantic intent-step consumers must use AST intent-step accessors"
fi

if grep -R "data\.intent_step\.\(where_type\|using_expr\|causes_effect\|inherited_where_from_action\|inherited_causes_from_action\|derived_where_from_using\|derived_where_from_transfer\|derived_using_from_transfer\|derived_using_from_where\)[[:space:]]*=[^=]" \
    src/semantic >/dev/null; then
    fail "semantic intent-step simple inferred field writes must use AST intent-step mutators"
fi

if grep -R "data\.intent_step\.\(authorized_by\|authorized_by_count\|authorized_by_capacity\|inherited_authorized_by_from_action\)[[:space:]]*=[^=]" \
    src/semantic >/dev/null; then
    fail "semantic intent-step authorized-by writes must use AST intent-step mutators"
fi

if grep -R "data\.intent_step\.derived_authorized_by_from_zone[[:space:]]*=[^=]" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "legacy zone-derived authorized-by field must not be set by active compiler phases"
fi

if grep -R "data\.intent_step\.\(who_names\|who_count\|who_capacity\|inherited_who_from_action\|derived_who_from_on_receiver\|derived_who_from_single_participant\)[[:space:]]*=[^=]" \
    src/semantic >/dev/null; then
    fail "semantic intent-step who writes must use AST intent-step mutators"
fi

if grep -R "data\.intent_step\.\(required_abilities\|required_ability_count\|required_ability_capacity\|inherited_requires_from_action\)[[:space:]]*=[^=]" \
    src/semantic >/dev/null; then
    fail "semantic intent-step required-ability writes must use AST intent-step mutators"
fi

if grep -R "data\.intent_step\." \
    src/semantic/type_checker_intent_action_contract.c \
    src/semantic/type_checker_intent_on_inference.c \
    src/semantic/type_checker_intent_authority.c \
    src/semantic/type_checker_intent_role_fields.c >/dev/null; then
    fail "compact intent semantic owners must use AST intent-step accessors/mutators"
fi

if grep -R "data\.func_decl\.\(within_zone\|causes_effect\|authorized_by\|authorized_by_count\|required_abilities\|required_ability_count\)" \
    src/semantic/type_checker_intent_on_inference.c >/dev/null; then
    fail "compact intent on-inference must use AST function contract accessors"
fi

for path in \
    src/compiler/hir_analysis.c \
    src/compiler/hir_routines.c \
    src/compiler/mir_decl_headers.c \
    src/compiler/runtime_none_contract.c \
    src/compiler/rir_builder.c \
    src/codegen/llvm_decl.c \
    src/codegen/llvm_inventory_decl_lookup.c \
    src/codegen/llvm_expr_call_methods_world_effect_sync.c \
    src/codegen/llvm_stmt_zone_action.c \
    src/codegen/transpiler_intent_context.c \
    src/codegen/transpiler_projection_sync.h \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_helpers_effects.c \
    src/semantic/type_checker_intent_contract_summary.c \
    src/semantic/type_checker_intent_control.c \
    src/semantic/type_checker_intent_action_contract.c \
    src/semantic/type_checker_intent_helpers.c \
    src/semantic/type_checker_module_contract.c \
    src/semantic/type_checker_resolution_graph_decl.c \
    src/semantic/type_checker_resolution_stage_signature.c
do
    if grep -q "data\.func_decl\.\(is_action\|within_zone\|causes_effect\|authorized_by\|authorized_by_count\|required_abilities\|required_ability_count\|access\|has_explicit_access\|doc_comment\|has_effects_clause\|declared_effects\)" "$path"; then
        fail "$path must use AST function contract accessors"
    fi
done

if grep -R "data\.func_decl\.\(is_action\|within_zone\|causes_effect\|authorized_by\|authorized_by_count\|required_abilities\|required_ability_count\|access\|has_explicit_access\|doc_comment\|has_effects_clause\|declared_effects\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "non-parser function contract metadata consumers must use AST function contract accessors"
fi

if grep -R "data\.intent_step\." \
    src/semantic/type_checker_intent_bindings.c \
    src/semantic/type_checker_intent_helpers.c \
    src/semantic/type_checker_intent_transfer.c \
    src/semantic/type_checker_intent_types.c >/dev/null; then
    fail "intent semantic helper/transfer/type owners must use AST intent-step accessors/mutators"
fi

if grep -R "data\.world_decl\.\(states\|state_count\)" \
    src/semantic/type_checker_world_state.c >/dev/null; then
    fail "world state validator must use AST world child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\)" \
    src/semantic/type_checker_world_embedding.c >/dev/null; then
    fail "world embedding validation must use AST world child accessors"
fi

if grep -R "data\.world_\(roster\|zone\)\.\(slot_name\|roster_type\|zone_type\|initializer\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen embedded world slot facts must use AST world embedded accessors/mutators"
fi

if grep -R "data\.world_decl\.\(rosters\|roster_count\|zones\|zone_count\|states\|state_count\|activations\|activate_count\|deactivations\|deactivate_count\|maintained_zones\|maintained_zone_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_resolution_graph_world.c >/dev/null; then
    fail "DAG world precollect must use AST world child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|shared_fields\|shared_count\|methods\|method_count\|authorities\|authority_count\|applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_effects\|maintained_effect_count\|maintained_relations\|maintained_relation_count\)" \
    src/semantic/type_checker_zone_decl.c >/dev/null; then
    fail "zone declaration validator must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(states\|state_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_states\|maintained_state_count\|authorities\|authority_count\)" \
    src/semantic/type_checker_zone_state.c >/dev/null; then
    fail "zone state validator must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(authorities\|authority_count\|layer_slots\|layer_slot_count\)" \
    src/semantic/type_checker_zone_decl_authority.c >/dev/null; then
    fail "zone authority validator must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|refreshes\|refresh_count\|applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_effects\|maintained_effect_count\|maintained_relations\|maintained_relation_count\|maintained_states\|maintained_state_count\|authorities\|authority_count\)" \
    src/semantic/type_checker_zone_shape.c >/dev/null; then
    fail "zone shape warnings must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(refreshes\|refresh_count\|authorities\|authority_count\)" \
    src/semantic/type_checker_zone_projection_rules.c >/dev/null; then
    fail "zone projection rules must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|states\|state_count\|authorities\|authority_count\)" \
    src/semantic/type_checker_decls_domain_helpers.c >/dev/null; then
    fail "domain declaration helpers must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(refreshes\|refresh_count\|applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_effects\|maintained_effect_count\|maintained_relations\|maintained_relation_count\)" \
    src/semantic/type_checker_resolution_graph_zone_commands.c >/dev/null; then
    fail "DAG zone command precollect must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(states\|state_count\|maintained_states\|maintained_state_count\|authorities\|authority_count\|methods\|method_count\)" \
    src/semantic/type_checker_resolution_graph_zone_tail.c >/dev/null; then
    fail "DAG zone state/authority precollect must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|shared_fields\|shared_count\|layer_slots\|layer_slot_count\)" \
    src/semantic/type_checker_resolution_graph_zone_inventory.c >/dev/null; then
    fail "DAG zone inventory precollect must use AST zone child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(zones\|zone_count\|slots\|slot_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|states\|state_count\)" \
    src/semantic/type_checker_builtins_query_domain.c >/dev/null; then
    fail "domain builtin query helpers must use AST domain child accessors"
fi

if grep -q "find_program_domain_decl_local" \
    src/semantic/type_checker_builtins_query_domain.c; then
    fail "domain builtin query helpers must consume semantic domain lookup seams"
fi

grep -q "semantic_find_zone_decl_by_name(ctx, zone_type_name)" \
    src/semantic/type_checker_builtins_query_domain.c \
    || fail "world zone builtin lookup must consume semantic zone lookup seam"

if grep -R "data\.relation_decl\.\(slots\|slot_count\|refreshes\|refresh_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_relation_decl.c >/dev/null; then
    fail "relation declaration validator must use AST relation child accessors"
fi

if grep -R "data\.relation_decl\.between_\(left\|right\)_\(kind\|type\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "relation endpoint consumers must use AST relation endpoint accessors"
fi

if grep -R "data\.party_decl\.\(generic_params\|extends\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "party generic/extends consumers must use AST party metadata accessors"
fi

if grep -R "data\.roster_decl\.generic_params" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "roster generic consumers must use AST roster metadata accessors"
fi

if grep -R "data\.class_decl\.\(generic_params\|where_clause\)" \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "class generic/where consumers must use AST class metadata accessors"
fi

if grep -R "data\.effect_decl\.\(slots\|slot_count\|refreshes\|refresh_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_effect_decl.c >/dev/null; then
    fail "effect declaration validator must use AST effect child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\)\.\(slots\|slot_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_resolution_graph_domain.c >/dev/null; then
    fail "DAG relation/effect precollect must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\)" \
    src/semantic/type_checker_domain_projection.c >/dev/null; then
    fail "domain projection contract checks must use AST zone child accessors"
fi

grep -q 'semantic_host_decl_for_type(ctx, source_type)' \
    src/semantic/type_checker_domain_projection.c \
    || fail "domain projection source checks must consume semantic host decl seam"

if grep -q 'find_subject_host_decl_by_name' src/semantic/type_checker_domain_projection.c; then
    fail "domain projection source checks must not reopen direct subject-host lookup"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(rosters\|roster_count\|zones\|zone_count\|methods\|method_count\)" \
    src/semantic/type_checker_expr_host.c >/dev/null; then
    fail "host expression lookup must use AST domain child accessors"
fi

if grep -R "data\.world_decl\.\(rosters\|roster_count\|zones\|zone_count\)" \
    src/semantic/type_checker_expr.c >/dev/null; then
    fail "world member expression lookup must use AST world child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\)" \
    src/semantic/type_checker_call_constructor.c >/dev/null; then
    fail "world constructor checks must use AST world child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\)" \
    src/codegen/transpiler_intent_zone_slot.c \
    src/codegen/transpiler_overlay_projection.c >/dev/null; then
    fail "C intent/overlay zone-slot helpers must use AST zone child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\|zone_decl\)\.\(slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_overlay_projection.c >/dev/null; then
    fail "C overlay projection must use AST domain child accessors"
fi

if grep -R "data\.\(zone_decl\|relation_decl\|effect_decl\)\.\(layer_slots\|layer_slot_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_overlay_zone_bind.c \
    src/codegen/transpiler_overlay_zone_relation_bind.c >/dev/null; then
    fail "C overlay zone bind helpers must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(rosters\|roster_count\|zones\|zone_count\|slots\|slot_count\|shared_fields\|shared_count\|states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/transpiler_projection.c >/dev/null; then
    fail "C projection lookup helpers must use AST domain child accessors"
fi

if grep -R "data\.\(zone_decl\|relation_decl\|effect_decl\)\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|shared_fields\|shared_count\)" \
    src/codegen/transpiler_overlay_host_fields.h >/dev/null; then
    fail "C overlay host field lookup must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(layer_slots\|layer_slot_count\)" \
    src/compiler/rir_builder_intent.c >/dev/null; then
    fail "RIR intent effect-slot lookup must use AST zone child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\)" \
    src/codegen/llvm_domain_world_sync.c >/dev/null; then
    fail "LLVM world sync must use AST world child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\|states\|state_count\)" \
    src/codegen/llvm_domain_world_frontier.c >/dev/null; then
    fail "LLVM world frontier must use AST world child accessors"
fi

if grep -R "data\.zone_decl\.\(states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/llvm_domain_zone_frontier_state.c >/dev/null; then
    fail "LLVM zone frontier state tracking must use AST zone child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(zones\|zone_count\|slots\|slot_count\|layer_slots\|layer_slot_count\|states\|state_count\)" \
    src/codegen/llvm_domain_lookup.c >/dev/null; then
    fail "LLVM domain lookup must use AST domain child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\|states\|state_count\|activations\|activate_count\|deactivations\|deactivate_count\|maintained_zones\|maintained_zone_count\)" \
    src/codegen/llvm_domain_world_sync_directives.c >/dev/null; then
    fail "LLVM world sync directives must use AST world child accessors"
fi

if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    src/codegen/llvm_decl.c \
    src/codegen/llvm_decl_authority.c >/dev/null; then
    fail "LLVM zone authority checks must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/llvm_expr_call_projection_sync.c >/dev/null; then
    fail "LLVM projection sync calls must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(layer_slots\|layer_slot_count\|states\|state_count\)" \
    src/codegen/llvm_intent_effect.c >/dev/null; then
    fail "LLVM intent effect provenance must use AST zone child accessors"
fi

if grep -R "data\.\(zone_decl\|relation_decl\|effect_decl\)\.\(slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/llvm_expr_assignment_projection.c >/dev/null; then
    fail "LLVM assignment projection invalidation must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(layer_slots\|layer_slot_count\|states\|state_count\)" \
    src/codegen/transpiler_block_intent_helpers.c >/dev/null; then
    fail "C intent effect provenance must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    src/codegen/transpiler_mir_func_emit.c >/dev/null; then
    fail "C MIR function authority checks must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|shared_fields\|shared_count\|states\|state_count\)" \
    src/codegen/transpiler_zone_struct_emit.c >/dev/null; then
    fail "C zone struct emission must use AST zone child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\)\.\(slots\|slot_count\|shared_fields\|shared_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_relation_effect_emit.c >/dev/null; then
    fail "C relation/effect emission must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|shared_fields\|shared_count\)" \
    src/codegen/transpiler_mir_ssa_names.c >/dev/null; then
    fail "C MIR SSA name rendering must use AST zone child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(methods\|method_count\)" \
    src/compiler/mir_decl_header_validate.c >/dev/null; then
    fail "MIR decl header validation must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(methods\|method_count\)" \
    src/compiler/hir_routines.c \
    src/compiler/mir_decl_headers.c >/dev/null; then
    fail "HIR/MIR domain method collection must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.\(methods\|method_count\)" \
    src/codegen/llvm_inventory_host_methods.c \
    src/codegen/transpiler_decl_method_view.c >/dev/null; then
    fail "C/LLVM hosted method views must use AST domain child accessors"
fi

if grep -R "data\.\(zone_decl\|effect_decl\)\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|states\|state_count\)" \
    src/codegen/llvm_expr_call_methods_world_effect_sync.c \
    src/codegen/llvm_stmt_zone_action.c >/dev/null; then
    fail "LLVM world/zone effect sync must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|effect_decl\)\.\(layer_slots\|layer_slot_count\|states\|state_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_projection_sync.h >/dev/null; then
    fail "C projection sync helpers must use AST domain child accessors"
fi

if grep -R "data\.\(zone_decl\|relation_decl\|effect_decl\)\.\(layer_slots\|layer_slot_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/llvm_domain_zone_bind_helpers.c >/dev/null; then
    fail "LLVM zone bind helpers must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(zones\|zone_count\|slots\|slot_count\|refreshes\|refresh_count\|shared_fields\|shared_count\)" \
    src/codegen/llvm_expr_constructor_calls.c >/dev/null; then
    fail "LLVM constructor calls must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(layer_slots\|layer_slot_count\|states\|state_count\|detaches\|detach_count\)" \
    src/codegen/llvm_domain_zone_sync_clauses.c >/dev/null; then
    fail "LLVM zone sync clauses must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(applies\|apply_count\|states\|state_count\|maintained_effects\|maintained_effect_count\|maintained_states\|maintained_state_count\)" \
    src/codegen/llvm_domain_zone_sync.c >/dev/null; then
    fail "LLVM zone sync must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(links\|link_count\|states\|state_count\|maintained_relations\|maintained_relation_count\|unlinks\|unlink_count\)" \
    src/codegen/llvm_domain_zone_sync_relations.c >/dev/null; then
    fail "LLVM zone relation sync must use AST zone child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(zones\|zone_count\|states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/domain_frontier_policy.c >/dev/null; then
    fail "domain frontier policy must use AST domain child accessors"
fi

if grep -R "data\.class_decl\.\(fields\|field_count\|nominal_kind\)" \
    src/codegen/llvm_domain_lookup.c >/dev/null; then
    fail "LLVM domain lookup must use AST nominal accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(shared_fields\|shared_count\|zones\|zone_count\|rosters\|roster_count\|slots\|slot_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/transpiler_nominal.c >/dev/null; then
    fail "C nominal member lookup must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(shared_fields\|shared_count\|zones\|zone_count\|rosters\|roster_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_call_constructor_result_emit.c >/dev/null; then
    fail "C constructor emit must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\|maintained_effects\|maintained_effect_count\|maintained_relations\|maintained_relation_count\|maintained_states\|maintained_state_count\)" \
    src/codegen/transpiler_zone_decl_emit.c >/dev/null; then
    fail "C zone declaration sync must use AST zone child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(shared_fields\|shared_count\|zones\|zone_count\|rosters\|roster_count\|slots\|slot_count\|refreshes\|refresh_count\|states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/llvm_domain_struct_register_fields.c >/dev/null; then
    fail "LLVM domain struct field registration must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(shared_fields\|shared_count\|zones\|zone_count\|rosters\|roster_count\|slots\|slot_count\|refreshes\|refresh_count\|states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/codegen/llvm_domain_struct_register.c >/dev/null; then
    fail "LLVM domain struct type registration must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|zones\|zone_count\|methods\|method_count\)" \
    src/compiler/rir_facts.c >/dev/null; then
    fail "RIR domain slot lookup must use AST domain child accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|zones\|zone_count\|methods\|method_count\|authorities\|authority_count\|applies\|apply_count\|links\|link_count\|detaches\|detach_count\|unlinks\|unlink_count\)" \
    src/compiler/rir_builder.c >/dev/null; then
    fail "RIR builder must use AST domain child accessors"
fi

if grep -R "data\.class_decl\.\(name\|methods\|method_count\)" \
    src/compiler/rir_builder.c >/dev/null; then
    fail "RIR class method collection must use AST nominal accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\|relation_decl\|effect_decl\)\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|refreshes\|refresh_count\|zones\|zone_count\|methods\|method_count\|shared_fields\|shared_count\|rosters\|roster_count\|states\|state_count\)" \
    src/compiler/module_normalizer_refs.c >/dev/null; then
    fail "module normalizer domain traversal must use AST domain child accessors"
fi

if grep -R "data\.class_decl\.\(fields\|field_count\|methods\|method_count\)" \
    src/compiler/module_normalizer_refs.c \
    src/compiler/runtime_none_contract.c >/dev/null; then
    fail "module normalizer/runtime-none class scans must use AST class accessors"
fi

if grep -R "data\.\(world_decl\|zone_decl\)\.\(zones\|zone_count\|states\|state_count\|layer_slots\|layer_slot_count\)" \
    src/semantic/type_checker_world_helpers.c >/dev/null; then
    fail "world helper lookup must use AST domain child accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\|world_decl\)\.\(role_slots\|role_count\|party_slots\|party_count\|shared_fields\|shared_count\|methods\|method_count\|rosters\|roster_count\|zones\|zone_count\)" \
    src/semantic/type_checker_resolution_stage_systemic.c >/dev/null; then
    fail "DAG systemic stage replay must use AST systemic child accessors"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\)\.\(rosters\|roster_count\|zones\|zone_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/compiler/dir_collect.c >/dev/null; then
    fail "DIR collection must use AST domain child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\)\.\(slots\|slot_count\|refreshes\|refresh_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/compiler/runtime_none_contract.c >/dev/null; then
    fail "runtime-none contract scanner must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|authorities\|authority_count\|refreshes\|refresh_count\|states\|state_count\)" \
    src/compiler/dir_collect_domain.c >/dev/null; then
    fail "DIR zone collection must use AST zone child accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.\(role_slots\|role_count\|party_slots\|party_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_resolution_graph_decl.c >/dev/null; then
    fail "DAG systemic precollect must use AST systemic child accessors"
fi

if grep -R "data\.\(zone_decl\|effect_decl\)\.\(slots\|slot_count\)" \
    src/semantic/type_checker_func_action_contract.c >/dev/null; then
    fail "action contract validation must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|authorities\|authority_count\)" \
    src/semantic/type_checker_intent_authority.c >/dev/null; then
    fail "intent authority validation must use AST zone child accessors"
fi

if grep -R "data\.zone_authority\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen zone authority consumers must use AST zone-authority accessors"
fi

if grep -R "data\.\(zone_decl\|world_decl\)\.\(slots\|slot_count\|zones\|zone_count\)" \
    src/semantic/type_checker_overlay_common.c >/dev/null; then
    fail "overlay common hosted scope registration must use AST domain child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\)" \
    src/semantic/type_checker_intent_participants.c >/dev/null; then
    fail "intent participant validation must use AST zone child accessors"
fi

if grep -R "data\.party_decl\.\(role_slots\|role_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_party_decl.c >/dev/null; then
    fail "party declaration validator must use AST party child accessors"
fi

if grep -R "data\.roster_decl\.\(party_slots\|party_count\|shared_fields\|shared_count\|methods\|method_count\)" \
    src/semantic/type_checker_roster_decl.c >/dev/null; then
    fail "roster declaration validator must use AST roster child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\|zone_decl\)\.\(refreshes\|refresh_count\|states\|state_count\)" \
    src/semantic/type_checker_builtins_query.c >/dev/null; then
    fail "builtin domain query predicates must use AST domain child accessors"
fi

if grep -R "data\.world_decl\.\(zones\|zone_count\|states\|state_count\)" \
    src/semantic/type_checker_builtins_query_world.c >/dev/null; then
    fail "world builtin query predicates must use AST world child accessors"
fi

if grep -R "data\.world_\(activate\|maintain\|deactivate\)\." \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen world lifecycle directive consumers must use AST world directive accessors"
fi

if grep -R "data\.world_state\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen world state consumers must use AST world-state accessors"
fi

if grep -R "data\.zone_\(apply\|detach\|link\|unlink\|maintain_effect\|maintain_relation\|maintain_state\)\." \
    src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen zone lifecycle directive consumers must use AST zone directive accessors"
fi

if grep -R "data\.zone_refresh\." src/semantic src/compiler src/codegen >/dev/null; then
    fail "semantic/compiler/codegen zone refresh/projection consumers must use AST zone-refresh accessors"
fi

for path in \
    src/semantic/symbol_table.c \
    src/semantic/type_checker_async_channel.c \
    src/semantic/type_checker_event.c \
    src/semantic/type_checker_expr.c \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_ops.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_builtins_secure_token.c \
    src/semantic/type_checker_builtins_slotops.c \
    src/semantic/type_checker_builtins_slotops_view.c \
    src/semantic/type_checker_builtins_stdlib_collections.c \
    src/semantic/type_checker_builtins_stdlib_map.c \
    src/semantic/type_checker_helpers_effects.c \
    src/semantic/type_checker_helpers_late.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_ownership_let.c \
    src/semantic/type_checker_ownership_let_helpers.c \
    src/semantic/type_checker_ownership_let_slot_claim.c \
    src/semantic/type_checker_program.c \
    src/semantic/type_checker_resolution_helpers.c \
    src/semantic/type_checker_assignment.c \
    src/semantic/type_checker_type_helpers.c \
    src/semantic/type_infer.c
do
    if grep -q "data\.\(constructed\|slot\|function\)\." "$path"; then
        fail "$path must use type-system accessors for constructed/slot/function payloads"
    fi
done

if grep -R "data\.\(constructed\|slot\|function\|tuple\|generic\|primitive\)\." \
    src/semantic src/compiler src/codegen \
    | grep -v "src/semantic/type_system.c" \
    | grep -v "src/semantic/type_system_slot.c" \
    | grep -v "src/semantic/type_system_tuple.c" \
    | grep -v "src/semantic/type_system_compat.c" \
    | grep -v "src/semantic/type_effects.c" \
    | grep -v "src/semantic/type_checker_resolution_metadata_storage.c" >/dev/null; then
    fail "non-type-system owners must use type-system accessors for Type payloads"
fi

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.name" \
    src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen world/relation/effect/zone-name consumers must use AST domain name accessors"
fi

if grep -R "resolve_type_node(" src/semantic/type_checker_resolution_graph_*.c >/dev/null; then
    fail "DAG graph core/precollect layer must not call resolve_type_node directly"
fi

if ! grep -Fq "semantic_assignment_path_release(owner, base)" \
    src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must release through owner-aware wrapper"
fi

if grep -Fq "free(base)" src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must not directly free base path strings"
fi

echo "[semantic-core-shape] semantic owner boundaries ok"
