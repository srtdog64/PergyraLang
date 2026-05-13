/*
 * Copyright (c) 2025 Pergyra Language Project
 * Public AST construction, manipulation, and utility API.
 */

#ifndef PERGYRA_AST_API_H
#define PERGYRA_AST_API_H

#include "ast_types.h"

/* AST creation functions */
ASTNode* ast_create_program(void);
ASTNode* ast_create_function(const char* name);
ASTNode* ast_create_class(const char* name);
const char* ast_class_name(const ASTNode* node);
NominalDeclKind ast_class_nominal_kind(const ASTNode* node);
bool ast_class_is_struct(const ASTNode* node);
ClassField** ast_class_fields(const ASTNode* node, size_t* count_out);
ASTNode** ast_class_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_subject(const char* name);
ASTNode* ast_create_vessel(const char* name);
ASTNode* ast_create_struct(const char* name);
ASTNode* ast_create_object(const char* name);
ASTNode* ast_create_tobject(const char* name);
const char* ast_enum_name(const ASTNode* node);
char** ast_enum_variants(const ASTNode* node, size_t* count_out);
size_t ast_enum_variant_param_count(const ASTNode* node, size_t index);
ASTNode* ast_enum_variant_param(const ASTNode* node, size_t variant_index,
                                size_t param_index);
ASTNode** ast_enum_methods(const ASTNode* node, size_t* count_out);
ASTNode* ast_create_extern_block(const char* abi);
ASTNode* ast_create_let_declaration(const char* name);
ASTNode* ast_create_type_alias(const char* name, ASTNode* target_type);
ASTNode* ast_create_with_statement(void);
ASTNode* ast_create_parallel_block(void);
ASTNode* ast_create_block(void);
ASTNode* ast_create_for_loop(void);
ASTNode* ast_create_while_loop(void);
ASTNode* ast_create_match_statement(void);
ASTNode* ast_create_match_case(void);
ASTNode* ast_create_if_statement(void);
ASTNode* ast_create_return_statement(void);
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right);
ASTNode* ast_create_unary(Token op, ASTNode* operand);
ASTNode* ast_create_call(ASTNode* callee);
ASTNode* ast_create_member_access(ASTNode* object, const char* member);
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index);
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value);
ASTNode* ast_create_number(const char* value);
ASTNode* ast_create_string(const char* value);
ASTNode* ast_create_boolean(bool value);
ASTNode* ast_create_identifier(const char* name);
ASTNode* ast_create_type(const char* name);

/* Async AST creation functions */
ASTNode* ast_create_async_function(const char* name, bool is_async);
ASTNode* ast_create_await_expression(ASTNode* expression);
ASTNode* ast_create_channel_send(ASTNode* channel, ASTNode* value);
ASTNode* ast_create_channel_recv(ASTNode* channel);
ASTNode* ast_create_select_statement(void);
ASTNode* ast_create_async_block(void);
ASTNode* ast_create_spawn_expression(ASTNode* function);
ASTNode* ast_create_channel_type(ASTNode* element_type);
ASTNode* ast_create_future_type(ASTNode* value_type);
ASTNode* ast_create_task_group(bool wait_all);

/* AST manipulation functions */
void ast_add_statement(ASTNode* parent, ASTNode* statement);
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task);
void ast_add_argument(ASTNode* call, ASTNode* arg);

/* AST utility functions */
void ast_destroy(ASTNode* node);
void ast_destroy_structured_comment(StructuredComment* comment);
void ast_print(ASTNode* node, int indent);
const char* token_type_to_string(PgyTokenType type);

/* Role/Ability system AST creation functions */
ASTNode* ast_create_ability_declaration(const char* name);
const char* ast_ability_name(const ASTNode* node);
size_t ast_ability_method_count(const ASTNode* node);
ASTNode* ast_ability_method(const ASTNode* node, size_t index);
ASTNode* ast_create_role_declaration(const char* name);
const char* ast_role_name(const ASTNode* node);
ASTNode* ast_create_include_statement(const char* role_name);
ASTNode* ast_role_for_type(const ASTNode* node);
size_t ast_role_include_count(const ASTNode* node);
ASTNode* ast_role_include(const ASTNode* node, size_t index);
size_t ast_role_impl_count(const ASTNode* node);
ASTNode* ast_role_impl(const ASTNode* node, size_t index);
const char* ast_include_role_name(const ASTNode* node);
GenericParams* ast_include_type_args(const ASTNode* node);
ASTNode* ast_create_require_field(const char* name);
ASTNode* ast_create_impl_ability(ASTNode* ability_ref);
ASTNode* ast_impl_ability_ref(const ASTNode* node);
const char* ast_impl_ability_name(const ASTNode* node);
size_t ast_impl_ability_method_count(const ASTNode* node);
ASTNode* ast_impl_ability_method(const ASTNode* node, size_t index);
ASTNode* ast_create_override_func(ASTNode* func_decl);

/* Roster/World system AST creation functions */
ASTNode* ast_create_roster_declaration(const char* name);
const char* ast_roster_name(const ASTNode* node);
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
ASTNode* ast_create_world_state(const char* state_name, const char* zone_slot_name,
                                WorldStateSourceKind source_kind,
                                const char* detail_name);
ASTNode* ast_create_world_state_compose(const char* state_name,
                                        WorldStateSourceKind source_kind,
                                        const char** input_names,
                                        size_t input_count);
ASTNode* ast_create_intent_declaration(const char* name);
ASTNode* ast_create_intent_involves(const char* alias);
ASTNode* ast_create_intent_value(const char* alias);
ASTNode* ast_create_intent_step(const char* name);
const char* ast_intent_step_name(const ASTNode* node);
ASTNode* ast_intent_step_where_type(const ASTNode* node);
ASTNode* ast_intent_step_using_expr(const ASTNode* node);
ASTNode* ast_intent_step_intent_expr(const ASTNode* node);
const char* ast_intent_step_transfer_from_alias(const ASTNode* node);
const char* ast_intent_step_transfer_to_alias(const ASTNode* node);
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
ASTNode* ast_create_relation_declaration(const char* name);
const char* ast_relation_name(const ASTNode* node);
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
ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name);
ASTNode* ast_create_zone_maintain_relation(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name);
ASTNode* ast_create_zone_maintain_state(const char* state_name);
ASTNode* ast_create_zone_authority(const char* subject_slot_name);
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
ASTNode* ast_create_party_instance(const char* party_type);

/* Event system AST creation functions */
ASTNode* ast_create_event_declaration(const char* name);
ASTNode* ast_create_event_subscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_unsubscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_invoke(ASTNode* event);
ASTNode* ast_create_event_handler_type(void);
ASTNode* ast_clone(ASTNode* node);
ASTNode* ast_create_lambda_expression(void);

/* Module system AST creation */
ASTNode* ast_create_import_declaration(const char* path);
ASTNode* ast_create_namespace_declaration(const char* name);
ASTNode* ast_create_unsafe_block(ASTNode* body);
ASTNode* ast_create_defer_statement(ASTNode* body);
ASTNode* ast_create_bind_statement(const char* party_var, const char* slot_name, const char* role_name);

#endif /* PERGYRA_AST_API_H */
