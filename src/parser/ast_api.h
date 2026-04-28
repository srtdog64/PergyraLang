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
ASTNode* ast_create_subject(const char* name);
ASTNode* ast_create_vessel(const char* name);
ASTNode* ast_create_struct(const char* name);
ASTNode* ast_create_object(const char* name);
ASTNode* ast_create_tobject(const char* name);
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
ASTNode* ast_create_role_declaration(const char* name);
ASTNode* ast_create_include_statement(const char* role_name);
ASTNode* ast_create_require_field(const char* name);
ASTNode* ast_create_impl_ability(ASTNode* ability_ref);
ASTNode* ast_create_override_func(ASTNode* func_decl);

/* Roster/World system AST creation functions */
ASTNode* ast_create_roster_declaration(const char* name);
ASTNode* ast_create_roster_slot(const char* slot_name, const char* party_type);
ASTNode* ast_create_world_declaration(const char* name);
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
ASTNode* ast_create_relation_declaration(const char* name);
ASTNode* ast_create_effect_declaration(const char* name);
ASTNode* ast_create_zone_declaration(const char* name);
ASTNode* ast_create_domain_slot(const char* slot_name, bool is_subject);
ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation);
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

/* Party system AST creation functions */
ASTNode* ast_create_party_declaration(const char* name);
ASTNode* ast_create_role_slot(const char* slot_name);
ASTNode* ast_create_party_shared(const char* name);
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
