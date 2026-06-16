/*
 * Copyright (c) 2025 Pergyra Language Project
 * Domain-oriented AST construction and accessor API.
 */

#ifndef PERGYRA_AST_DOMAIN_API_H
#define PERGYRA_AST_DOMAIN_API_H

#include "ast_types.h"

/* Role/Ability system AST creation functions */
ASTNode* ast_create_ability_declaration(const char* name);
const char* ast_ability_name(const ASTNode* node);
AccessModifier ast_ability_access(const ASTNode* node);
bool ast_ability_has_explicit_access(const ASTNode* node);
bool ast_ability_is_innate(const ASTNode* node);
GenericParams* ast_ability_generic_params(const ASTNode* node);
WhereClause* ast_ability_where_clause(const ASTNode* node);
size_t ast_ability_require_field_count(const ASTNode* node);
ASTNode* ast_ability_require_field(const ASTNode* node, size_t index);
size_t ast_ability_method_count(const ASTNode* node);
ASTNode* ast_ability_method(const ASTNode* node, size_t index);
ASTNode** ast_ability_methods(const ASTNode* node, size_t* method_count);
ASTNode* ast_create_role_declaration(const char* name);
const char* ast_role_name(const ASTNode* node);
ASTNode* ast_create_include_statement(const char* role_name);
ASTNode* ast_role_for_type(const ASTNode* node);
GenericParams* ast_role_generic_params(const ASTNode* node);
WhereClause* ast_role_where_clause(const ASTNode* node);
ASTNode* ast_role_parallel_block(const ASTNode* node);
size_t ast_role_include_count(const ASTNode* node);
ASTNode* ast_role_include(const ASTNode* node, size_t index);
size_t ast_role_impl_count(const ASTNode* node);
ASTNode* ast_role_impl(const ASTNode* node, size_t index);
bool ast_role_impl_method_total_count(const ASTNode* node, size_t* count_out);
const char* ast_include_role_name(const ASTNode* node);
GenericParams* ast_include_type_args(const ASTNode* node);
ASTNode* ast_create_require_field(const char* name);
const char* ast_require_field_name(const ASTNode* node);
ASTNode* ast_require_field_type(const ASTNode* node);
ASTNode* ast_create_impl_ability(ASTNode* ability_ref);
ASTNode* ast_impl_ability_ref(const ASTNode* node);
const char* ast_impl_ability_name(const ASTNode* node);
size_t ast_impl_ability_method_count(const ASTNode* node);
ASTNode* ast_impl_ability_method(const ASTNode* node, size_t index);
ASTNode* ast_create_override_func(ASTNode* func_decl);
ASTNode* ast_override_func_decl(const ASTNode* node);
bool ast_override_calls_super(const ASTNode* node);

/* Roster/World system AST creation functions */
ASTNode* ast_create_roster_declaration(const char* name);
const char* ast_roster_name(const ASTNode* node);
GenericParams* ast_roster_generic_params(const ASTNode* node);
size_t ast_roster_party_count(const ASTNode* node);
ASTNode* ast_roster_party(const ASTNode* node, size_t index);
size_t ast_roster_shared_count(const ASTNode* node);
ASTNode* ast_roster_shared(const ASTNode* node, size_t index);
ASTNode** ast_roster_shared_fields(const ASTNode* node, size_t* count_out);
size_t ast_roster_method_count(const ASTNode* node);
ASTNode* ast_roster_method(const ASTNode* node, size_t index);
ASTNode** ast_roster_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_roster_slot(const char* slot_name, const char* party_type);
const char* ast_roster_slot_name(const ASTNode* node);
const char* ast_roster_slot_party_type(const ASTNode* node);
bool ast_roster_slot_replace_party_type(ASTNode* node, const char* party_type);
ASTNode* ast_create_world_declaration(const char* name);
const char* ast_world_name(const ASTNode* node);
ASTNode** ast_world_rosters(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_zones(const ASTNode* node, size_t* count_out);
const char* ast_world_roster_slot_name(const ASTNode* node);
const char* ast_world_roster_type_name(const ASTNode* node);
ASTNode* ast_world_roster_initializer(const ASTNode* node);
bool ast_world_roster_replace_type_name(ASTNode* node, const char* type_name);
const char* ast_world_zone_slot_name(const ASTNode* node);
const char* ast_world_zone_type_name(const ASTNode* node);
ASTNode* ast_world_zone_initializer(const ASTNode* node);
ASTNode** ast_world_shared_fields(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_states(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_activations(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_deactivations(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_maintained_zones(const ASTNode* node, size_t* count_out);
ASTNode** ast_world_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_world_roster(const char* slot_name, const char* roster_type);
ASTNode* ast_create_world_zone(const char* slot_name, const char* zone_type);
ASTNode* ast_create_world_activate(const char* zone_slot_name);
ASTNode* ast_create_world_deactivate(const char* zone_slot_name);
ASTNode* ast_create_world_maintain(const char* zone_slot_name);
const char* ast_world_directive_zone_slot_name(const ASTNode* node);
const char* ast_world_directive_state_name(const ASTNode* node);
ASTNode* ast_create_world_state(const char* state_name, const char* zone_slot_name,
                                WorldStateSourceKind source_kind,
                                const char* detail_name);
ASTNode* ast_create_world_state_compose(const char* state_name,
                                        WorldStateSourceKind source_kind,
                                        const char** input_names,
                                        size_t input_count);
const char* ast_world_state_name(const ASTNode* node);
const char* ast_world_state_zone_slot_name(const ASTNode* node);
WorldStateSourceKind ast_world_state_source_kind(const ASTNode* node);
const char* ast_world_state_detail_name(const ASTNode* node);
size_t ast_world_state_input_count(const ASTNode* node);
const char* ast_world_state_input_name(const ASTNode* node, size_t index);
ASTNode* ast_create_intent_declaration(const char* name);
ASTNode* ast_create_intent_involves(const char* alias);
ASTNode* ast_create_intent_value(const char* alias);
ASTNode* ast_create_intent_step(const char* name);
const char* ast_intent_decl_name(const ASTNode* node);
ASTNode** ast_intent_decl_involves(const ASTNode* node, size_t* count_out);
size_t ast_intent_decl_involve_count(const ASTNode* node);
ASTNode** ast_intent_decl_values(const ASTNode* node, size_t* count_out);
size_t ast_intent_decl_value_count(const ASTNode* node);
ASTNode** ast_intent_decl_bindings(const ASTNode* node, size_t* count_out);
size_t ast_intent_decl_binding_count(const ASTNode* node);
ASTNode** ast_intent_decl_steps(const ASTNode* node, size_t* count_out);
size_t ast_intent_decl_step_count(const ASTNode* node);
bool ast_intent_decl_is_concurrent(const ASTNode* node);
IntentRollbackPolicy ast_intent_decl_rollback_policy(const ASTNode* node);
ASTNode* ast_intent_decl_priority_expr(const ASTNode* node);
ASTNode* ast_intent_decl_success_expr(const ASTNode* node);
ASTNode* ast_intent_decl_failure_expr(const ASTNode* node);
char** ast_intent_decl_default_who_names(const ASTNode* node, size_t* count_out);
size_t ast_intent_decl_default_who_count(const ASTNode* node);
ASTNode* ast_intent_decl_default_where_type(const ASTNode* node);
int ast_intent_decl_retry_count(const ASTNode* node);
const char* ast_intent_involves_alias(const ASTNode* node);
ASTNode* ast_intent_involves_subject_type(const ASTNode* node);
const char* ast_intent_value_alias(const ASTNode* node);
ASTNode* ast_intent_value_type(const ASTNode* node);
const char* ast_intent_step_name(const ASTNode* node);
ASTNode* ast_intent_step_where_type(const ASTNode* node);
ASTNode* ast_intent_step_using_expr(const ASTNode* node);
ASTNode* ast_intent_step_intent_expr(const ASTNode* node);
const char* ast_intent_step_transfer_from_alias(const ASTNode* node);
const char* ast_intent_step_transfer_to_alias(const ASTNode* node);
bool ast_intent_step_replace_transfer_to_alias_copy(ASTNode* node, const char* alias);
char** ast_intent_step_who_names(const ASTNode* node, size_t* count_out);
size_t ast_intent_step_who_count(const ASTNode* node);
ASTNode** ast_intent_step_on_exprs(const ASTNode* node, size_t* count_out);
size_t ast_intent_step_on_expr_count(const ASTNode* node);
ASTNode** ast_intent_step_compensate_exprs(const ASTNode* node, size_t* count_out);
size_t ast_intent_step_compensate_expr_count(const ASTNode* node);
ASTNode* ast_intent_step_pre_expr(const ASTNode* node);
ASTNode* ast_intent_step_guard_expr(const ASTNode* node);
ASTNode* ast_intent_step_post_expr(const ASTNode* node);
ASTNode* ast_intent_step_invariant_expr(const ASTNode* node);
ASTNode** ast_intent_step_required_abilities(const ASTNode* node, size_t* count_out);
size_t ast_intent_step_required_ability_count(const ASTNode* node);
const char* ast_intent_step_causes_effect(const ASTNode* node);
char** ast_intent_step_authorized_by(const ASTNode* node, size_t* count_out);
size_t ast_intent_step_authorized_by_count(const ASTNode* node);
ASTNode* ast_intent_step_expect_expr(const ASTNode* node);
bool ast_intent_step_inherited_who_from_intent(const ASTNode* node);
bool ast_intent_step_derived_who_from_on_receiver(const ASTNode* node);
bool ast_intent_step_derived_who_from_single_participant(const ASTNode* node);
bool ast_intent_step_inherited_where_from_intent(const ASTNode* node);
bool ast_intent_step_inherited_who_from_action(const ASTNode* node);
bool ast_intent_step_inherited_where_from_action(const ASTNode* node);
bool ast_intent_step_inherited_requires_from_action(const ASTNode* node);
bool ast_intent_step_inherited_causes_from_action(const ASTNode* node);
bool ast_intent_step_inherited_authorized_by_from_action(const ASTNode* node);
bool ast_intent_step_derived_authorized_by_from_zone(const ASTNode* node);
bool ast_intent_step_derived_where_from_using(const ASTNode* node);
bool ast_intent_step_derived_where_from_transfer(const ASTNode* node);
bool ast_intent_step_derived_using_from_transfer(const ASTNode* node);
bool ast_intent_step_derived_using_from_where(const ASTNode* node);
bool ast_intent_step_set_where_type(ASTNode* node, ASTNode* where_type);
bool ast_intent_step_set_using_expr(ASTNode* node, ASTNode* using_expr);
bool ast_intent_step_set_causes_effect_copy(ASTNode* node, const char* causes_effect);
bool ast_intent_step_append_who_name_copy(ASTNode* node, const char* alias);
bool ast_intent_step_append_authorized_by_copy(ASTNode* node, const char* alias);
bool ast_intent_step_append_required_ability_clone(ASTNode* node, ASTNode* ability);
void ast_intent_step_clear_authorized_by(ASTNode* node);
void ast_intent_step_mark_inherited_who_from_action(ASTNode* node);
void ast_intent_step_mark_derived_who_from_on_receiver(ASTNode* node);
void ast_intent_step_mark_derived_who_from_single_participant(ASTNode* node);
void ast_intent_step_mark_inherited_where_from_action(ASTNode* node);
void ast_intent_step_mark_inherited_requires_from_action(ASTNode* node);
void ast_intent_step_mark_inherited_causes_from_action(ASTNode* node);
void ast_intent_step_mark_inherited_authorized_by_from_action(ASTNode* node);
void ast_intent_step_mark_derived_where_from_using(ASTNode* node);
void ast_intent_step_mark_derived_where_from_transfer(ASTNode* node);
void ast_intent_step_mark_derived_using_from_transfer(ASTNode* node);
void ast_intent_step_mark_derived_using_from_where(ASTNode* node);
ASTNode* ast_create_relation_declaration(const char* name);
const char* ast_relation_name(const ASTNode* node);
RelationEndpointKind ast_relation_between_left_kind(const ASTNode* node);
RelationEndpointKind ast_relation_between_right_kind(const ASTNode* node);
ASTNode* ast_relation_between_left_type(const ASTNode* node);
ASTNode* ast_relation_between_right_type(const ASTNode* node);
ASTNode** ast_relation_slots(const ASTNode* node, size_t* count_out);
ASTNode** ast_relation_refreshes(const ASTNode* node, size_t* count_out);
ASTNode** ast_relation_shared_fields(const ASTNode* node, size_t* count_out);
ASTNode** ast_relation_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_effect_declaration(const char* name);
const char* ast_effect_name(const ASTNode* node);
ASTNode** ast_effect_slots(const ASTNode* node, size_t* count_out);
ASTNode** ast_effect_refreshes(const ASTNode* node, size_t* count_out);
ASTNode** ast_effect_shared_fields(const ASTNode* node, size_t* count_out);
ASTNode** ast_effect_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_zone_declaration(const char* name);
const char* ast_zone_name(const ASTNode* node);
bool ast_zone_forbids_unsafe(const ASTNode* node);
ASTNode** ast_zone_slots(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_layer_slots(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_shared_fields(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_authorities(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_methods(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_refreshes(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_states(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_applies(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_links(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_detaches(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_unlinks(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_maintained_effects(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_maintained_relations(const ASTNode* node, size_t* count_out);
ASTNode** ast_zone_maintained_states(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_domain_slot(const char* slot_name, bool is_subject);
const char* ast_domain_slot_name(const ASTNode* node);
ASTNode* ast_domain_slot_type(const ASTNode* node);
bool ast_domain_slot_is_subject(const ASTNode* node);
bool ast_domain_slot_is_vessel(const ASTNode* node);
bool ast_domain_slot_is_tobject(const ASTNode* node);
bool ast_domain_slot_is_binding(const ASTNode* node);
ASTNode* ast_domain_slot_initializer(const ASTNode* node);
ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation);
const char* ast_zone_layer_slot_name(const ASTNode* node);
const char* ast_zone_layer_slot_layer_type(const ASTNode* node);
bool ast_zone_layer_slot_is_relation(const ASTNode* node);
bool ast_zone_layer_slot_is_pool(const ASTNode* node);
int ast_zone_layer_slot_pool_capacity(const ASTNode* node);
ASTNode* ast_create_zone_apply(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_link(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_detach(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_unlink(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_refresh(const char* object_slot_name, const char* source_slot_name);
const char* ast_zone_refresh_object_slot_name(const ASTNode* node);
const char* ast_zone_refresh_source_slot_name(const ASTNode* node);
const char* ast_zone_refresh_participant_slot_name(const ASTNode* node);
bool ast_zone_refresh_requires_dto(const ASTNode* node);
bool ast_zone_refresh_derives_target_kind(const ASTNode* node);
size_t ast_zone_refresh_field_map_count(const ASTNode* node);
const char* ast_zone_refresh_mapped_target_field(const ASTNode* node,
                                                 size_t index);
const char* ast_zone_refresh_mapped_source_field(const ASTNode* node,
                                                 size_t index);
ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_maintain_relation(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_maintain_state(const char* state_name);
const char* ast_zone_effect_slot_name(const ASTNode* node);
const char* ast_zone_effect_target_slot_name(const ASTNode* node);
const char* ast_zone_relation_slot_name(const ASTNode* node);
const char* ast_zone_relation_left_slot_name(const ASTNode* node);
const char* ast_zone_relation_right_slot_name(const ASTNode* node);
const char* ast_zone_directive_state_name(const ASTNode* node);
const char* ast_zone_directive_participant_slot_name(const ASTNode* node);
ASTNode* ast_create_zone_authority(const char* subject_slot_name);
const char* ast_zone_authority_subject_slot_name(const ASTNode* node);
size_t ast_zone_authority_ability_count(const ASTNode* node);
ASTNode** ast_zone_authority_required_abilities(const ASTNode* node,
                                                size_t* count_out);
ASTNode* ast_zone_authority_required_ability(const ASTNode* node,
                                             size_t index);
ASTNode* ast_create_zone_state(const char* state_name, bool is_relation,
                               const char* layer_slot_name,
                               const char* left_or_target_slot_name,
                               const char* right_slot_name);
const char* ast_zone_state_name(const ASTNode* node);
bool ast_zone_state_is_relation(const ASTNode* node);
const char* ast_zone_state_layer_slot_name(const ASTNode* node);
const char* ast_zone_state_left_or_target_slot_name(const ASTNode* node);
const char* ast_zone_state_right_slot_name(const ASTNode* node);

/* Party system AST creation functions */
ASTNode* ast_create_party_declaration(const char* name);
const char* ast_party_name(const ASTNode* node);
GenericParams* ast_party_generic_params(const ASTNode* node);
ASTNode* ast_party_extends(const ASTNode* node);
size_t ast_party_role_count(const ASTNode* node);
ASTNode* ast_party_role(const ASTNode* node, size_t index);
size_t ast_party_shared_count(const ASTNode* node);
ASTNode* ast_party_shared(const ASTNode* node, size_t index);
ASTNode** ast_party_shared_fields(const ASTNode* node, size_t* count_out);
size_t ast_party_method_count(const ASTNode* node);
ASTNode* ast_party_method(const ASTNode* node, size_t index);
ASTNode** ast_party_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_role_slot(const char* slot_name);
const char* ast_role_slot_name(const ASTNode* node);
bool ast_role_slot_is_dynamic(const ASTNode* node);
size_t ast_role_slot_required_ability_count(const ASTNode* node);
ASTNode* ast_role_slot_required_ability(const ASTNode* node, size_t index);
ASTNode** ast_role_slot_required_abilities(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_party_shared(const char* name);
const char* ast_party_shared_name(const ASTNode* node);
ASTNode* ast_party_shared_type(const ASTNode* node);
ASTNode* ast_party_shared_initializer(const ASTNode* node);
ASTNode* ast_create_context_access(const char* method_name, const char* slot_name);
const char* ast_context_access_method_name(const ASTNode* node);
const char* ast_context_access_role_slot_name(const ASTNode* node);
ASTNode* ast_context_access_ability_type(const ASTNode* node);
ASTNode* ast_create_party_instance(const char* party_type);
const char* ast_party_instance_party_type(const ASTNode* node);
size_t ast_party_instance_assignment_count(const ASTNode* node);
const char* ast_party_instance_assignment_slot_name(const ASTNode* node,
                                                   size_t index);
ASTNode* ast_party_instance_assignment_value(const ASTNode* node, size_t index);

#endif /* PERGYRA_AST_DOMAIN_API_H */
