/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST party, event, and lambda constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

// Party declaration
ASTNode* ast_create_party_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_PARTY_DECL);
    node->data.party_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.party_decl.role_slots = NULL;
    node->data.party_decl.role_count = 0;
    node->data.party_decl.shared_fields = NULL;
    node->data.party_decl.shared_count = 0;
    node->data.party_decl.methods = NULL;
    node->data.party_decl.method_count = 0;
    node->data.party_decl.extends = NULL;
    node->data.party_decl.generic_params = NULL;
    node->data.party_decl.doc_comment = NULL;
    return node;
}

const char*
ast_party_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return NULL;
    return node->data.party_decl.name;
}

GenericParams*
ast_party_generic_params(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return NULL;
    return node->data.party_decl.generic_params;
}

ASTNode*
ast_party_extends(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return NULL;
    return node->data.party_decl.extends;
}

size_t
ast_party_role_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return 0;
    return node->data.party_decl.role_count;
}

ASTNode*
ast_party_role(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARTY_DECL
        || index >= node->data.party_decl.role_count) {
        return NULL;
    }
    return node->data.party_decl.role_slots[index];
}

size_t
ast_party_shared_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return 0;
    return node->data.party_decl.shared_count;
}

ASTNode*
ast_party_shared(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARTY_DECL
        || index >= node->data.party_decl.shared_count) {
        return NULL;
    }
    return node->data.party_decl.shared_fields[index];
}

ASTNode**
ast_party_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_party_shared_count(node);
    if (node == NULL || node->type != AST_PARTY_DECL)
        return NULL;
    return node->data.party_decl.shared_fields;
}

size_t
ast_party_method_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_DECL)
        return 0;
    return node->data.party_decl.method_count;
}

ASTNode*
ast_party_method(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARTY_DECL
        || index >= node->data.party_decl.method_count) {
        return NULL;
    }
    return node->data.party_decl.methods[index];
}

ASTNode**
ast_party_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_party_method_count(node);
    if (node == NULL || node->type != AST_PARTY_DECL)
        return NULL;
    return node->data.party_decl.methods;
}

// Role slot in party
ASTNode* ast_create_role_slot(const char* slot_name) {
    ASTNode* node = ast_create_node(AST_ROLE_SLOT);
    node->data.role_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.role_slot.required_abilities = NULL;
    node->data.role_slot.ability_count = 0;
    node->data.role_slot.is_array = false;
    return node;
}

// Party shared field
ASTNode* ast_create_party_shared(const char* name) {
    ASTNode* node = ast_create_node(AST_PARTY_SHARED);
    node->data.party_shared.name = name ? pergyra_strdup(name) : NULL;
    node->data.party_shared.type = NULL;
    node->data.party_shared.initializer = NULL;
    node->data.party_shared.access = ACCESS_PUBLIC;
    return node;
}

// Context access
ASTNode* ast_create_context_access(const char* method_name, const char* slot_name) {
    ASTNode* node = ast_create_node(AST_CONTEXT_ACCESS);
    node->data.context_access.method_name = method_name ? pergyra_strdup(method_name) : NULL;
    node->data.context_access.role_slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.context_access.ability_type = NULL;
    return node;
}

const char*
ast_context_access_method_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CONTEXT_ACCESS)
        return NULL;
    return node->data.context_access.method_name;
}

const char*
ast_context_access_role_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CONTEXT_ACCESS)
        return NULL;
    return node->data.context_access.role_slot_name;
}

ASTNode*
ast_context_access_ability_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CONTEXT_ACCESS)
        return NULL;
    return node->data.context_access.ability_type;
}

// Party instance creation
ASTNode* ast_create_party_instance(const char* party_type) {
    ASTNode* node = ast_create_node(AST_PARTY_INSTANCE);
    node->data.party_instance.party_type = party_type ? pergyra_strdup(party_type) : NULL;
    node->data.party_instance.assignments = NULL;
    node->data.party_instance.assignment_count = 0;
    return node;
}

const char*
ast_party_instance_party_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_INSTANCE)
        return NULL;
    return node->data.party_instance.party_type;
}

size_t
ast_party_instance_assignment_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_INSTANCE)
        return 0;
    return node->data.party_instance.assignment_count;
}

const char*
ast_party_instance_assignment_slot_name(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARTY_INSTANCE
        || index >= node->data.party_instance.assignment_count)
        return NULL;
    return node->data.party_instance.assignments[index].slot_name;
}

ASTNode*
ast_party_instance_assignment_value(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARTY_INSTANCE
        || index >= node->data.party_instance.assignment_count)
        return NULL;
    return node->data.party_instance.assignments[index].value;
}

// Event declaration
ASTNode* ast_create_event_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_EVENT_DECL);
    node->data.event_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.event_decl.params = NULL;
    node->data.event_decl.param_count = 0;
    node->data.event_decl.return_type = NULL;
    node->data.event_decl.access = ACCESS_PUBLIC;
    return node;
}

// Event subscribe
ASTNode* ast_create_event_subscribe(ASTNode* event, ASTNode* handler) {
    ASTNode* node = ast_create_node(AST_EVENT_SUBSCRIBE);
    node->data.event_op.event = event;
    node->data.event_op.handler = handler;
    if (event != NULL) {
        node->line = event->line;
        node->column = event->column;
    }
    return node;
}

// Event unsubscribe
ASTNode* ast_create_event_unsubscribe(ASTNode* event, ASTNode* handler) {
    ASTNode* node = ast_create_node(AST_EVENT_UNSUBSCRIBE);
    node->data.event_op.event = event;
    node->data.event_op.handler = handler;
    if (event != NULL) {
        node->line = event->line;
        node->column = event->column;
    }
    return node;
}

// Event invoke
ASTNode* ast_create_event_invoke(ASTNode* event) {
    ASTNode* node = ast_create_node(AST_EVENT_INVOKE);
    node->data.event_invoke.event = event;
    node->data.event_invoke.arguments = NULL;
    node->data.event_invoke.arg_count = 0;
    return node;
}

// Event handler type
ASTNode* ast_create_event_handler_type(void) {
    ASTNode* node = ast_create_node(AST_EVENT_HANDLER_TYPE);
    node->data.event_handler_type.param_types = NULL;
    node->data.event_handler_type.param_count = 0;
    node->data.event_handler_type.param_capacity = 0;
    node->data.event_handler_type.return_type = NULL;
    return node;
}

// Lambda expression
ASTNode* ast_create_lambda_expression(void) {
    ASTNode* node = ast_create_node(AST_LAMBDA_EXPR);
    node->data.lambda_expr.params = NULL;
    node->data.lambda_expr.param_count = 0;
    node->data.lambda_expr.param_capacity = 0;
    node->data.lambda_expr.body = NULL;
    node->data.lambda_expr.return_type = NULL;
    node->data.lambda_expr.is_async = false;
    node->data.lambda_expr.captures = NULL;
    node->data.lambda_expr.capture_count = 0;
    node->data.lambda_expr.capture_capacity = 0;
    return node;
}
