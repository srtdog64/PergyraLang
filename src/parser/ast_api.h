/*
 * Copyright (c) 2025 Pergyra Language Project
 * Public AST construction, manipulation, and utility API.
 */

#ifndef PERGYRA_AST_API_H
#define PERGYRA_AST_API_H

#include "ast_types.h"
#include "ast_domain_api.h"

/* AST creation functions */
ASTNode* ast_create_program(void);
bool ast_assign_stable_ids(ASTNode* root);
uint32_t ast_node_stable_id(const ASTNode* node);
size_t ast_program_statement_count(const ASTNode* node);
ASTNode** ast_program_statements(const ASTNode* node, size_t* count_out);
ASTNode* ast_program_statement(const ASTNode* node, size_t index);
bool ast_program_append_statement(ASTNode* node, ASTNode* statement);
ASTNode* ast_program_detach_statement(ASTNode* node, size_t index);
bool ast_program_replace_statements(ASTNode* node,
                                    ASTNode** statements,
                                    size_t count,
                                    size_t capacity);
const char* ast_declaration_name(const ASTNode* node);
GenericParams* ast_declaration_generic_params(const ASTNode* node);
bool ast_replace_declaration_name_copy(ASTNode* node, const char* name);
ASTNode* ast_create_function(const char* name);
size_t ast_func_param_count(const ASTNode* node);
FuncParam** ast_func_params(const ASTNode* node, size_t* count_out);
FuncParam* ast_func_param(const ASTNode* node, size_t index);
GenericParams* ast_func_generic_params(const ASTNode* node);
WhereClause* ast_func_where_clause(const ASTNode* node);
ASTNode* ast_func_return_type(const ASTNode* node);
bool ast_func_set_return_type(ASTNode* node, ASTNode* return_type);
const char* ast_func_semantic_return_type_name(const ASTNode* node);
bool ast_func_set_semantic_return_type_name_copy(ASTNode* node,
                                                 const char* type_name);
ASTNode* ast_func_body(const ASTNode* node);
bool ast_func_attach_body(ASTNode* node, ASTNode* body);
ASTNode* ast_func_detach_body(ASTNode* node);
bool ast_func_is_action(const ASTNode* node);
AccessModifier ast_func_access(const ASTNode* node);
bool ast_func_has_explicit_access(const ASTNode* node);
bool ast_func_has_effects_clause(const ASTNode* node);
uint32_t ast_func_declared_effects(const ASTNode* node);
bool ast_func_has_caps_clause(const ASTNode* node);
uint32_t ast_func_declared_capabilities(const ASTNode* node);
StructuredComment* ast_func_doc_comment(const ASTNode* node);
size_t ast_func_required_ability_count(const ASTNode* node);
ASTNode** ast_func_required_abilities(const ASTNode* node, size_t* count_out);
ASTNode* ast_func_required_ability(const ASTNode* node, size_t index);
const char* ast_func_within_zone(const ASTNode* node);
bool ast_func_set_within_zone_copy(ASTNode* node, const char* within_zone);
const char* ast_func_causes_effect(const ASTNode* node);
size_t ast_func_authorized_by_count(const ASTNode* node);
const char* ast_func_authorized_by(const ASTNode* node, size_t index);
ASTNode* ast_create_class(const char* name);
const char* ast_class_name(const ASTNode* node);
NominalDeclKind ast_class_nominal_kind(const ASTNode* node);
bool ast_class_is_struct(const ASTNode* node);
GenericParams* ast_class_generic_params(const ASTNode* node);
WhereClause* ast_class_where_clause(const ASTNode* node);
ClassField** ast_class_fields(const ASTNode* node, size_t* count_out);
size_t ast_class_field_destructure_count(const ASTNode* node);
ASTNode* ast_class_field_destructure_at(const ASTNode* node, size_t index);
bool ast_class_append_field_destructure(ASTNode* node, ASTNode* destructure);
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
const char* ast_extern_block_abi(const ASTNode* node);
ASTNode** ast_extern_block_declarations(const ASTNode* node, size_t* count_out);
ASTNode* ast_extern_block_declaration(const ASTNode* node, size_t index);
const char* ast_use_module_name(const ASTNode* node);
ASTNode* ast_create_let_declaration(const char* name);
const char* ast_let_name(const ASTNode* node);
ASTNode* ast_let_type(const ASTNode* node);
ASTNode* ast_let_initializer(const ASTNode* node);
bool ast_let_attach_initializer(ASTNode* node, ASTNode* initializer);
bool ast_let_is_mutable(const ASTNode* node);
bool ast_let_is_alias(const ASTNode* node);
size_t ast_let_destructure_name_count(const ASTNode* node);
const char* ast_let_destructure_name(const ASTNode* node, size_t index);
ASTNode* ast_let_destructure_initializer(const ASTNode* node);
ASTNode* ast_create_type_alias(const char* name, ASTNode* target_type);
const char* ast_type_alias_name(const ASTNode* node);
ASTNode* ast_type_alias_target_type(const ASTNode* node);
ASTNode* ast_create_lifecycle_declaration(const char* subject);
bool ast_lifecycle_add_transition(ASTNode* node, const char* op,
                                  const char* from_state,
                                  const char* to_state);
const char* ast_lifecycle_subject(const ASTNode* node);
size_t ast_lifecycle_transition_count(const ASTNode* node);
const LifecycleTransitionDecl* ast_lifecycle_transition(const ASTNode* node,
                                                        size_t index);
ASTNode* ast_create_with_statement(void);
ASTNode* ast_create_parallel_block(void);
ASTNode* ast_create_block(void);
ASTNode** ast_block_statements(const ASTNode* node, size_t* count_out);
size_t ast_block_statement_count(const ASTNode* node);
ASTNode* ast_block_statement(const ASTNode* node, size_t index);
ASTNode** ast_block_detach_statements(ASTNode* node, size_t* count_out);
bool ast_block_is_pin_block(const ASTNode* node);
bool ast_block_pin_view_is_write(const ASTNode* node);
const char* ast_block_pin_source_name(const ASTNode* node);
const char* ast_block_pin_view_name(const ASTNode* node);
ASTNode* ast_create_for_loop(void);
const char* ast_for_label(const ASTNode* node);
const char* ast_for_variable(const ASTNode* node);
ASTNode* ast_for_range_start(const ASTNode* node);
ASTNode* ast_for_range_end(const ASTNode* node);
ASTNode* ast_for_iterable(const ASTNode* node);
ASTNode* ast_for_detach_iterable(ASTNode* node);
bool ast_for_attach_iterable(ASTNode* node, ASTNode* iterable);
ASTNode* ast_for_body(const ASTNode* node);
ASTNode* ast_create_while_loop(void);
ASTNode* ast_create_match_statement(void);
ASTNode* ast_create_match_case(void);
ASTNode* ast_match_subject(const ASTNode* node);
ASTNode** ast_match_cases(const ASTNode* node, size_t* count_out);
size_t ast_match_case_count(const ASTNode* node);
ASTNode* ast_match_case_at(const ASTNode* node, size_t index);
ASTNode* ast_match_default_body(const ASTNode* node);
ASTNode* ast_match_case_pattern(const ASTNode* node);
ASTNode** ast_match_case_patterns(const ASTNode* node, size_t* count_out);
size_t ast_match_case_pattern_count(const ASTNode* node);
ASTNode* ast_match_case_pattern_at(const ASTNode* node, size_t index);
ASTNode* ast_match_case_guard(const ASTNode* node);
ASTNode* ast_match_case_body(const ASTNode* node);
ASTNode* ast_find_match_subject_for_case(const ASTNode* root,
                                         const ASTNode* case_node);
ASTNode* ast_create_if_statement(void);
ASTNode* ast_create_return_statement(void);
ASTNode* ast_create_give_statement(ASTNode* value);
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right);
ASTNode* ast_create_unary(Token op, ASTNode* operand);
ASTNode* ast_create_call(ASTNode* callee);
ASTNode* ast_create_member_access(ASTNode* object, const char* member);
ASTNode* ast_member_object(const ASTNode* node);
const char* ast_member_name(const ASTNode* node);
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index);
ASTNode* ast_array_access_array(const ASTNode* node);
ASTNode* ast_array_access_index(const ASTNode* node);
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value);
ASTNode* ast_assignment_target(const ASTNode* node);
ASTNode* ast_assignment_value(const ASTNode* node);
double ast_number_value(const ASTNode* node);
bool ast_number_is_long(const ASTNode* node);
bool ast_number_is_float(const ASTNode* node);
/* Duration literal (docs/181 SS2.3): value = normalized nanoseconds. */
bool ast_number_is_duration(const ASTNode* node);
bool ast_number_make_duration(ASTNode* node, double ns_value);
const char* ast_string_value(const ASTNode* node);
void ast_morph_to_string(ASTNode* node, const char* value);
bool ast_boolean_value(const ASTNode* node);
ASTNode* ast_await_expression(const ASTNode* node);
ASTNode* ast_channel_send_channel(const ASTNode* node);
ASTNode* ast_channel_send_value(const ASTNode* node);
ASTNode* ast_channel_recv_channel(const ASTNode* node);
ASTNode* ast_unary_operand(const ASTNode* node);
Token ast_unary_operator(const ASTNode* node);
ASTNode* ast_binary_left(const ASTNode* node);
ASTNode* ast_binary_right(const ASTNode* node);
Token ast_binary_operator(const ASTNode* node);
size_t ast_array_literal_count(const ASTNode* node);
ASTNode* ast_array_literal_element(const ASTNode* node, size_t index);
size_t ast_tuple_literal_count(const ASTNode* node);
ASTNode* ast_tuple_literal_element(const ASTNode* node, size_t index);
size_t ast_map_literal_count(const ASTNode* node);
ASTNode* ast_map_literal_key(const ASTNode* node, size_t index);
ASTNode* ast_map_literal_value(const ASTNode* node, size_t index);
size_t ast_set_literal_count(const ASTNode* node);
ASTNode* ast_set_literal_element(const ASTNode* node, size_t index);
ASTNode* ast_cast_operand(const ASTNode* node);
const char* ast_cast_target_type(const ASTNode* node);
ASTNode* ast_type_test_operand(const ASTNode* node);
const char* ast_type_test_target_type(const ASTNode* node);
const char* ast_type_name_canonical_scalar(const char* name);
const char* ast_break_label(const ASTNode* node);
const char* ast_continue_label(const ASTNode* node);
ASTNode* ast_if_condition(const ASTNode* node);
ASTNode* ast_if_then_branch(const ASTNode* node);
ASTNode* ast_if_else_branch(const ASTNode* node);
const char* ast_while_label(const ASTNode* node);
ASTNode* ast_while_condition(const ASTNode* node);
ASTNode* ast_while_body(const ASTNode* node);
ASTNode* ast_unsafe_block_body(const ASTNode* node);
ASTNode* ast_transaction_block_body(const ASTNode* node);
ASTNode* ast_fail_stmt_reason(const ASTNode* node);
ASTNode* ast_defer_body(const ASTNode* node);
ASTNode* ast_return_value(const ASTNode* node);
ASTNode* ast_give_value(const ASTNode* node);
ASTNode* ast_create_number(const char* value);
ASTNode* ast_create_string(const char* value);
ASTNode* ast_create_boolean(bool value);
ASTNode* ast_create_identifier(const char* name);
const char* ast_identifier_name(const ASTNode* node);
bool ast_replace_identifier_name_copy(ASTNode* node, const char* name);
ASTNode* ast_create_type(const char* name);
ASTNode* ast_create_generic_type(const char* name, ASTNode* inner_type);
const char* ast_type_name(const ASTNode* node);
GenericParams* ast_type_generic_args(const ASTNode* node);
bool ast_type_append_generic_arg_owned(ASTNode* node,
                                       const char* name,
                                       ASTNode* constraint,
                                       ASTNode* default_type);
size_t ast_generic_param_count(const GenericParams* params);
GenericParam* ast_generic_param_at(const GenericParams* params, size_t index);
const char* ast_generic_param_name(const GenericParam* param);
ASTNode* ast_generic_param_constraint(const GenericParam* param);
ASTNode* ast_generic_param_default_type(const GenericParam* param);
size_t ast_type_tuple_element_count(const ASTNode* node);
ASTNode* ast_type_tuple_element(const ASTNode* node, size_t index);
bool ast_replace_type_name_copy(ASTNode* node, const char* type_name);
GenericParams* ast_call_generic_args(const ASTNode* node);
size_t ast_call_generic_arg_count(const ASTNode* node);
GenericParam* ast_call_generic_arg(const ASTNode* node, size_t index);
ASTNode* ast_call_callee(const ASTNode* node);
uint32_t ast_call_semantic_callee_decl_id(const ASTNode* node);
bool ast_call_set_semantic_callee_decl_id(ASTNode* node, uint32_t decl_id);
size_t ast_call_arg_count(const ASTNode* node);
ASTNode** ast_call_arguments(const ASTNode* node, size_t* count_out);
ASTNode* ast_call_argument(const ASTNode* node, size_t index);
const char* ast_call_argument_name(const ASTNode* node, size_t index);
bool ast_call_has_named_arguments(const ASTNode* node);
ASTNode* ast_call_find_named_argument(const ASTNode* node, const char* field_name);
void ast_init_call_borrowed_view(ASTNode* node, ASTNode* callee,
                                 ASTNode** arguments, size_t arg_count);

/* Async AST creation functions */
ASTNode* ast_create_async_function(const char* name, bool is_async);
const char* ast_async_func_name(const ASTNode* node);
size_t ast_async_func_param_count(const ASTNode* node);
FuncParam** ast_async_func_params(const ASTNode* node, size_t* count_out);
FuncParam* ast_async_func_param(const ASTNode* node, size_t index);
ASTNode* ast_async_func_body(const ASTNode* node);
ASTNode* ast_create_await_expression(ASTNode* expression);
ASTNode* ast_create_channel_send(ASTNode* channel, ASTNode* value);
ASTNode* ast_create_channel_recv(ASTNode* channel);
ASTNode* ast_create_select_statement(void);
ASTNode* ast_create_async_block(void);
ASTNode** ast_async_block_statements(const ASTNode* node, size_t* count_out);
size_t ast_async_block_statement_count(const ASTNode* node);
ASTNode* ast_async_block_statement(const ASTNode* node, size_t index);
ASTNode** ast_parallel_tasks(const ASTNode* node, size_t* count_out);
size_t ast_parallel_task_count(const ASTNode* node);
ASTNode* ast_parallel_task(const ASTNode* node, size_t index);
/* Join form (docs/181 SS1 rung 0): `parallel (x in xs) [join with all]`. */
bool ast_parallel_set_join_form(ASTNode* node, const char* element_name,
                                ASTNode* collection);
bool ast_parallel_is_join_form(const ASTNode* node);
const char* ast_parallel_join_element(const ASTNode* node);
ASTNode* ast_parallel_join_collection(const ASTNode* node);
/* Index form (docs/181 R1): `parallel (i in lo..hi)`; join_collection is
 * the range start. */
void ast_parallel_set_join_range_end(ASTNode* node, ASTNode* range_end);
ASTNode* ast_parallel_join_range_end(const ASTNode* node);
bool ast_parallel_is_index_join(const ASTNode* node);
/* Expression form (docs/181 R2): checker-sealed give result type name. */
bool ast_parallel_set_join_give_type(ASTNode* node, const char* type_name);
const char* ast_parallel_join_give_type(const ASTNode* node);
/* Reduce combinator (docs/181 R4): parse-time "sum"/"product"/"min"/"max". */
bool ast_parallel_set_join_reduce_op(ASTNode* node, const char* op);
const char* ast_parallel_join_reduce_op(const ASTNode* node);
/* any-join (docs/181 R3): first give wins; element mode + expr form only. */
bool ast_parallel_set_join_any(ASTNode* node);
bool ast_parallel_join_is_any(const ASTNode* node);
ASTNode* ast_with_slot_type(const ASTNode* node);
const char* ast_with_alias(const ASTNode* node);
ASTNode* ast_with_body(const ASTNode* node);
bool ast_with_is_secure(const ASTNode* node);
const char* ast_with_security_level(const ASTNode* node);
ASTNode* ast_create_spawn_expression(ASTNode* function);
ASTNode* ast_spawn_function(const ASTNode* node);
ASTNode** ast_spawn_arguments(const ASTNode* node, size_t* count_out);
size_t ast_spawn_arg_count(const ASTNode* node);
ASTNode* ast_spawn_argument(const ASTNode* node, size_t index);
bool ast_spawn_is_blocking(const ASTNode* node);
ASTNode* ast_create_channel_type(ASTNode* element_type);
ASTNode* ast_channel_type_element_type(const ASTNode* node);
ASTNode* ast_channel_type_capacity(const ASTNode* node);
ASTNode* ast_create_future_type(ASTNode* value_type);
ASTNode* ast_future_type_value_type(const ASTNode* node);
ASTNode** ast_select_cases(const ASTNode* node, size_t* count_out);
size_t ast_select_case_count(const ASTNode* node);
ASTNode* ast_select_case(const ASTNode* node, size_t index);
ASTNode* ast_select_default_case(const ASTNode* node);

/* AST manipulation functions */
void ast_add_statement(ASTNode* parent, ASTNode* statement);
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task);
void ast_add_argument(ASTNode* call, ASTNode* arg);

/* AST utility functions */
void ast_destroy(ASTNode* node);
void ast_destroy_structured_comment(StructuredComment* comment);
void ast_print(ASTNode* node, int indent);
/* Capture an inline/compact rendering of `node` into a malloc'd string (caller
 * frees). Used by the MIR source-shape owner to preserve transitional MIR JSON
 * expression text without reopening AST payloads during serialization. */
char* ast_capture_inline(ASTNode* node);
const char* token_type_to_string(PgyTokenType type);

/* Event system AST creation functions */
ASTNode* ast_create_event_declaration(const char* name);
const char* ast_event_name(const ASTNode* node);
size_t ast_event_param_count(const ASTNode* node);
ASTNode* ast_event_param(const ASTNode* node, size_t index);
ASTNode* ast_event_return_type(const ASTNode* node);
ASTNode* ast_create_event_subscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_unsubscribe(ASTNode* event, ASTNode* handler);
ASTNode* ast_create_event_invoke(ASTNode* event);
ASTNode* ast_event_op_event(const ASTNode* node);
ASTNode* ast_event_op_handler(const ASTNode* node);
ASTNode* ast_event_invoke_event(const ASTNode* node);
ASTNode** ast_event_invoke_arguments(const ASTNode* node, size_t* count_out);
size_t ast_event_invoke_arg_count(const ASTNode* node);
ASTNode* ast_event_invoke_argument(const ASTNode* node, size_t index);
ASTNode* ast_create_event_handler_type(void);
size_t ast_event_handler_param_count(const ASTNode* node);
ASTNode** ast_event_handler_param_types(const ASTNode* node,
                                        size_t* count_out);
ASTNode* ast_event_handler_param_type(const ASTNode* node, size_t index);
ASTNode* ast_event_handler_return_type(const ASTNode* node);
ASTNode* ast_clone(ASTNode* node);
ASTNode* ast_create_lambda_expression(void);
ASTNode** ast_lambda_params(const ASTNode* node, size_t* count_out);
size_t ast_lambda_param_count(const ASTNode* node);
ASTNode* ast_lambda_param(const ASTNode* node, size_t index);
ASTNode* ast_lambda_body(const ASTNode* node);
ASTNode* ast_lambda_return_type(const ASTNode* node);
bool ast_lambda_is_async(const ASTNode* node);
size_t ast_lambda_capture_count(const ASTNode* node);
const char* ast_lambda_capture_name(const ASTNode* node, size_t index);
const char* ast_lambda_capture_type_name(const ASTNode* node, size_t index);
LambdaCaptureMode ast_lambda_capture_mode(const ASTNode* node, size_t index);
bool ast_lambda_add_capture(ASTNode* node, const char* name,
                            const char* type_name, LambdaCaptureMode mode);

/* Module system AST creation */
ASTNode* ast_create_import_declaration(const char* path);
const char* ast_import_path(const ASTNode* node);
ASTNode* ast_create_namespace_declaration(const char* name);
const char* ast_namespace_name(const ASTNode* node);
ASTNode** ast_namespace_statements(const ASTNode* node, size_t* count_out);
size_t ast_namespace_statement_count(const ASTNode* node);
ASTNode* ast_namespace_statement(const ASTNode* node, size_t index);
void ast_destroy_namespace_shell_only(ASTNode* node);
ASTNode* ast_create_unsafe_block(ASTNode* body);
ASTNode* ast_create_transaction_block(ASTNode* body);
ASTNode* ast_create_fail_statement(ASTNode* reason);
ASTNode* ast_create_cast(ASTNode* operand, const char* target_type);
ASTNode* ast_create_type_test(ASTNode* operand, const char* target_type);
ASTNode* ast_create_defer_statement(ASTNode* body);
ASTNode* ast_create_bind_statement(const char* party_var, const char* slot_name, const char* role_name);
const char* ast_bind_statement_party_var(const ASTNode* node);
const char* ast_bind_statement_slot_name(const ASTNode* node);
const char* ast_bind_statement_role_name(const ASTNode* node);

#endif /* PERGYRA_AST_API_H */
