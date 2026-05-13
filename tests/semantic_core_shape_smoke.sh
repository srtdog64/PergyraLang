#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    echo "[semantic-core-shape] $*" >&2
    exit 1
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
    src/semantic/type_checker_builtins_stdlib_collections.c \
    src/semantic/type_checker_resolution_stage_alias.c \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_resolution_stage_lookup.c \
    src/semantic/type_checker_resolution_stage_stats.c \
    src/semantic/type_checker_resolution_stage_domain.c \
    src/semantic/type_checker_host_helpers.c \
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_func_action_contract.c \
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
    src/semantic/type_checker_func_decl.c \
    src/semantic/type_checker_func_action_contract.c \
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

if [ -e src/semantic/type_checker_resolve.c ] || [ -e src/semantic/type_checker_resolve.h ]; then
    fail "obsolete type_checker_resolve compatibility owner must not reappear"
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

grep -q 'ast_include_role_name(inc)' src/semantic/type_checker_resolution_graph_decl.c \
    || fail "DAG role include precollect must consume AST include accessor"

grep -q 'ast_impl_ability_ref(impl)' src/semantic/type_checker_resolution_graph_decl.c \
    || fail "DAG role impl precollect must consume AST impl-ability accessor"

grep -q 'semantic_role_for_type_node(role_decl)' src/semantic/type_checker_resolution_graph_decl.c \
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
    src/compiler src/codegen \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
    fail "compiler/codegen ability consumers must use AST ability accessors"
fi

if grep -R "data\.role_decl\.name" \
    src/compiler src/codegen \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
    fail "compiler/codegen role-name consumers must use AST role accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.name" \
    src/compiler src/codegen \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
    fail "compiler/codegen party/roster-name consumers must use AST domain name accessors"
fi

if grep -R "data\.\(party_decl\|roster_decl\)\.name" \
    src/semantic src/compiler src/codegen \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
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
    src/codegen/transpiler_zone_decl_emit.h >/dev/null; then
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

if grep -R "data\.\(class_decl\|enum_decl\)\.\(name\|fields\|field_count\|methods\|method_count\|variants\|variant_count\|nominal_kind\|is_struct\)" \
    src/compiler src/codegen src/semantic \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
    fail "semantic/compiler/codegen nominal payload readers must use AST nominal accessors"
fi

grep -q 'Intentional AST payload mutation seam' src/compiler/module_normalizer.c \
    || fail "module_normalizer.c nominal payload exception must stay documented as the mutable-name rewrite seam"

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

if grep -R "data\.intent_step\." src/compiler src/codegen >/dev/null; then
    fail "compiler/codegen intent-step read consumers must use AST intent-step accessors"
fi

if grep -R "data\.intent_step\." \
    src/semantic/type_checker_intent_contract_summary.c \
    src/semantic/type_checker_intent_participants.c \
    src/semantic/type_checker_intent_ability.c \
    src/semantic/type_checker_resolution_graph_intent.c \
    src/semantic/type_checker_resolution_stage_systemic.c >/dev/null; then
    fail "read-only semantic intent-step consumers must use AST intent-step accessors"
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
    src/codegen/transpiler_overlay_zone_bind.h >/dev/null; then
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
    src/codegen/llvm_decl.c >/dev/null; then
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
    src/codegen/transpiler_block_intent_helpers.h >/dev/null; then
    fail "C intent effect provenance must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(authorities\|authority_count\)" \
    src/codegen/transpiler_mir_func_emit.h >/dev/null; then
    fail "C MIR function authority checks must use AST zone child accessors"
fi

if grep -R "data\.zone_decl\.\(slots\|slot_count\|layer_slots\|layer_slot_count\|shared_fields\|shared_count\|states\|state_count\)" \
    src/codegen/transpiler_zone_struct_emit.h >/dev/null; then
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
    src/codegen/transpiler_zone_decl_emit.h >/dev/null; then
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

if grep -R "data\.\(world_decl\|relation_decl\|effect_decl\|zone_decl\)\.name" \
    src/compiler src/codegen \
    | grep -v "src/compiler/module_normalizer.c" >/dev/null; then
    fail "compiler/codegen world/relation/effect/zone-name consumers must use AST domain name accessors"
fi

if grep -R "resolve_type_node(" src/semantic/type_checker_resolution_graph_*.c >/dev/null; then
    fail "DAG graph core/precollect layer must not call resolve_type_node directly"
fi

echo "[semantic-core-shape] semantic owner boundaries ok"
