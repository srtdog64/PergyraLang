#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    echo "[semantic-core-shape] $*" >&2
    exit 1
}

shape_scan_cache=""
cleanup_shape_scan_cache() {
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
    src/semantic/type_checker_generic_contracts.h \
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

grep -q 'semantic_assignment_path_heap_owner' src/semantic/type_checker_assignment_path.c \
    || fail "assignment target path heap lane must stay explicit"

grep -q 'semantic_assignment_path_scratch_owner' src/semantic/type_checker_assignment_path.c \
    || fail "assignment target path scratch lane must stay explicit"

if grep -q 'semantic_assignment_target_path_impl(ASTNode \*expr' \
    src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must not reopen ctx + scratch bool mode"
fi

if grep -q 'scratch && ctx' src/semantic/type_checker_assignment_path.c; then
    fail "assignment target path must not compute scratch mode from a bool seam"
fi

grep -q 'Retired compatibility resolver audit counters' src/semantic/type_checker_resolution_retired.c \
    || fail "retired DAG compatibility counter owner lost its audit marker"

grep -q 'require_assignable(Type \*from, Type \*to' src/semantic/type_checker_type_helpers.c \
    || fail "assignability helper must stay outside the retired resolver counter owner"

grep -q 'semantic_role_for_type_name' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role target-type helper must live in domain role lookup owner"

grep -q 'semantic_find_role_decl' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role declaration lookup helper must live in domain role lookup owner"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_domain_role_lookup.c \
    || fail "semantic role lookup ability scan must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_name(stmt)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload checks must consume the semantic role target-type helper"

grep -q 'semantic_find_role_decl(program' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload include traversal must consume the shared semantic role lookup helper"

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload include traversal must consume AST include accessor"

grep -q 'ast_impl_ability_method(impl, j)' src/semantic/type_checker_expr_ops.c \
    || fail "operator overload method traversal must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_name(stmt)' src/semantic/type_checker_ability_match.c \
    || fail "ability role matching must consume the semantic role target-type helper"

grep -q 'semantic_find_role_decl(program' src/semantic/type_checker_ability_match.c \
    || fail "ability include traversal must consume the shared semantic role lookup helper"

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_ability_match.c \
    || fail "ability include traversal must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_ability_match.c \
    || fail "ability matching must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_name(role_decl)' src/semantic/type_checker_intent_role_fields.c \
    || fail "intent role-field validation must consume the semantic role target-type helper"

grep -q 'semantic_find_role_decl(ctx->program_root, role_name)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration validation must consume the shared semantic role lookup helper"

grep -q 'semantic_role_for_type_node(node)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration host-type validation must consume the semantic role target-type helper"

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration include validation must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration impl validation must consume AST impl-ability accessor"

grep -q 'ast_impl_ability_method(impl, j)' src/semantic/type_checker_role_decl.c \
    || fail "role declaration impl method validation must consume AST impl-ability accessor"

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
    src/semantic/type_checker_assignment.h \
    src/semantic/type_checker_async_channel.h \
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
    src/semantic/type_checker_generic_support.h \
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
    src/codegen/transpiler_call_constructor_result_emit.h \
    src/codegen/transpiler_call_result_option_builtin_emit.c \
    src/codegen/transpiler_channel_type_query.c \
    src/codegen/transpiler_destructure_emit.h \
    src/codegen/transpiler_domain_receiver_query.c \
    src/codegen/transpiler_event_builtin_emit.c \
    src/codegen/transpiler_expr_call_user_emit.h \
    src/codegen/transpiler_expr_core_builtins_emit.c \
    src/codegen/transpiler_expr_core_builtins_emit.h \
    src/codegen/transpiler_expr_projection_builtin.c \
    src/codegen/transpiler_expr_dispatch_emit.h \
    src/codegen/transpiler_expr_stdlib_builtin.c \
    src/codegen/transpiler_expr_stdlib_builtin.h \
    src/codegen/transpiler_func_class_flow_emit.h \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_generic_binding_query.c \
    src/codegen/transpiler_generic_param_query.c \
    src/codegen/transpiler_helpers_core_b.h \
    src/codegen/transpiler_intent_context.c \
    src/codegen/transpiler_lambda_emit.h \
    src/codegen/transpiler_let_box_emit.h \
    src/codegen/transpiler_let_slot_emit.c \
    src/codegen/transpiler_match_emit.c \
    src/codegen/transpiler_mir_assignment_emit.h \
    src/codegen/transpiler_mir_block_emit.h \
    src/codegen/transpiler_mir_destructure_emit.h \
    src/codegen/transpiler_mir_local_binding.c \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_local_type_lookup.h \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_pending_uses.h \
    src/codegen/transpiler_mir_ssa_emit.h \
    src/codegen/transpiler_nominal.c \
    src/codegen/transpiler_parallel_capture.h \
    src/codegen/transpiler_projection_field_path.c \
    src/codegen/transpiler_projection_method_invalidation.h \
    src/codegen/transpiler_projection_sync_helpers.h \
    src/codegen/transpiler_select.c \
    src/codegen/transpiler_slot_builtin_emit.c \
    src/codegen/transpiler_slot_target.c \
    src/codegen/transpiler_spawn_channel_emit.h \
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
    src/semantic/type_checker_async_channel.h \
    src/semantic/type_checker_expr_host.c \
    src/semantic/type_checker_expr_call.c \
    src/semantic/type_checker_expr_ops.c \
    src/semantic/type_checker_func_action_contract.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_generic_support.h \
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
    src/codegen/transpiler_async_parallel_emit.h \
    src/codegen/transpiler_extern.c \
    src/codegen/transpiler_class_decl_emit.h \
    src/codegen/transpiler_domain_nominal_emit.h \
    src/codegen/transpiler_domain_role_ability_emit.h \
    src/codegen/transpiler_domain_role_methods_emit.h \
    src/codegen/transpiler_enum_decl_emit.h \
    src/codegen/transpiler_expr_call_spawn_emit.h \
    src/codegen/transpiler_expr_call_user_emit.h \
    src/codegen/transpiler_expr_type_infer.c \
    src/codegen/transpiler_expr_type_infer.h \
    src/codegen/transpiler_func_class_flow_emit.h \
    src/codegen/transpiler_func_forward_emit.c \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_func_forward_metadata.c \
    src/codegen/transpiler_func_forward_policy.c \
    src/codegen/transpiler_generic_class_specialization_emit.h \
    src/codegen/transpiler_let_emit.h \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_mir_emission_mapping_contract.h \
    src/codegen/transpiler_mir_emit_state.c \
    src/codegen/transpiler_mir_func_emit.h \
    src/codegen/transpiler_mir_func_ssa_locals_emit.h \
    src/codegen/transpiler_mir_local_binding.c \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_local_type_lookup.h \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_signature.c \
    src/codegen/transpiler_mir_ssa_emit.h \
    src/codegen/transpiler_projection_method_invalidation.h \
    src/codegen/transpiler_spawn_channel_emit.h \
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
    src/semantic/type_checker_async_channel.h \
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
    src/codegen/transpiler_call_constructor_result_emit.h \
    src/codegen/transpiler_expr_call_spawn_emit.h \
    src/codegen/transpiler_expr_call_user_emit.h >/dev/null; then
    fail "codegen callable call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/llvm_stmt_zone_action.c \
    src/codegen/transpiler_projection_sync_helpers.h >/dev/null; then
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
    src/codegen/transpiler_func_class_flow_emit.h \
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
    src/codegen/transpiler_mir_destructure_emit.h \
    src/codegen/transpiler_mir_match_condition_emit.c \
    src/codegen/transpiler_mir_local_type_lookup.h \
    src/codegen/transpiler_mir_ssa_emit.h \
    src/codegen/transpiler_mir_ssa_contract.h \
    src/codegen/transpiler_parallel_capture.h \
    src/codegen/transpiler_projection_method_invalidation.h \
    src/codegen/transpiler_slot_builtin_emit.c \
    src/codegen/transpiler_slot_target.c >/dev/null; then
    fail "codegen domain/vtable/event/channel/slot call owners must use AST call accessors"
fi

if grep -R "data\.call\.\(callee\|arguments\|arg_count\)" \
    src/codegen/transpiler_mir_local_type_ast_lookup.c \
    src/codegen/transpiler_mir_pending_uses.h \
    src/codegen/transpiler_spawn_channel_emit.h >/dev/null; then
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
    src/codegen/transpiler_domain_nominal_emit.h \
    src/codegen/transpiler_domain_role_ability_emit.h \
    src/codegen/transpiler_expr_call_spawn_emit.h \
    src/codegen/transpiler_func_class_flow_emit.h \
    src/codegen/transpiler_func_forward_helpers.h \
    src/codegen/transpiler_func_forward_policy.c \
    src/codegen/transpiler_generic_class_specialization_emit.h \
    src/codegen/transpiler_intent_context.c \
    src/codegen/transpiler_intent_emit.c \
    src/codegen/transpiler_intent_participant.c \
    src/codegen/transpiler_domain_provenance_emit.h \
    src/codegen/transpiler_intent_zone_slot.c \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_let_emit.h \
    src/codegen/transpiler_let_box_emit.h \
    src/codegen/transpiler_let_slot_emit.c \
    src/codegen/transpiler_mir_ssa_emit.h \
    src/codegen/transpiler_overlay_projection.h \
    src/codegen/transpiler_projection.c \
    src/codegen/transpiler_projection_method_invalidation.h \
    src/codegen/transpiler_projection_sync_helpers.h \
    src/codegen/transpiler_specialization_registry.h \
    src/codegen/transpiler_type_render.c \
    src/semantic/type_checker_async_channel.h \
    src/semantic/type_checker_builtins_query_domain.c \
    src/semantic/type_checker_call_generic_where.c \
    src/semantic/type_checker_domain_role_lookup.c \
    src/semantic/type_checker_generic_contracts.h \
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
    src/codegen/transpiler_relation_effect_emit.h \
    src/codegen/transpiler_world_select_event_emit.h \
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
    src/semantic/type_checker_host_helpers.c >/dev/null; then
    fail "semantic host overlay helpers must use AST domain child accessors"
fi

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|methods\|method_count\|is_struct\)" \
    src/semantic/type_checker_host_helpers.c >/dev/null; then
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
    src/codegen/transpiler_world_select_event_emit.h >/dev/null; then
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
    src/codegen/transpiler_overlay_projection.h >/dev/null; then
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
    src/codegen/transpiler_projection_sync_helpers.h \
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
    src/codegen/transpiler_overlay_world_projection.h >/dev/null; then
    fail "C intent/overlay zone-slot helpers must use AST zone child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\|zone_decl\)\.\(slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_overlay_projection.h >/dev/null; then
    fail "C overlay projection must use AST domain child accessors"
fi

if grep -R "data\.\(zone_decl\|relation_decl\|effect_decl\)\.\(layer_slots\|layer_slot_count\|slots\|slot_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_overlay_zone_bind.h \
    src/codegen/transpiler_overlay_zone_relation_bind.h >/dev/null; then
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
    src/codegen/transpiler_mir_func_emit.h >/dev/null; then
    fail "C MIR function authority checks must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|shared_fields\|shared_count\|states\|state_count\)" \
    src/codegen/transpiler_zone_struct_emit.c >/dev/null; then
    fail "C zone struct emission must use AST zone child accessors"
fi

if grep -R "data\.\(relation_decl\|effect_decl\)\.\(slots\|slot_count\|shared_fields\|shared_count\|refreshes\|refresh_count\)" \
    src/codegen/transpiler_relation_effect_emit.h >/dev/null; then
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
    src/codegen/transpiler_projection_sync_helpers.h >/dev/null; then
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
    src/codegen/transpiler_call_constructor_result_emit.h >/dev/null; then
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
    src/semantic/type_checker_async_channel.h \
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
    src/semantic/type_checker_assignment.h \
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

echo "[semantic-core-shape] semantic owner boundaries ok"
