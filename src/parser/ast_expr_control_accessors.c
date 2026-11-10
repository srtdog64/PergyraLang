/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner: member/index/assignment/control-flow accessors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ASTNode*
ast_member_object(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;
    return node->data.member.object;
}

const char*
ast_member_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;
    return node->data.member.name;
}

ASTNode*
ast_array_access_array(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_ACCESS)
        return NULL;
    return node->data.array_access.array;
}

ASTNode*
ast_array_access_index(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_ACCESS)
        return NULL;
    return node->data.array_access.index;
}

ASTNode*
ast_assignment_target(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return node->data.assignment.target;
}

ASTNode*
ast_assignment_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return node->data.assignment.value;
}

const char*
ast_assignment_semantic_binding_type_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return node->data.assignment.semantic_binding_type_name;
}

AstAssignmentOperation
ast_assignment_operation(const ASTNode *node)
{
    return node != NULL && node->type == AST_ASSIGNMENT
        ? node->data.assignment.operation : AST_ASSIGN_SET;
}

const char *
ast_assignment_operation_text(AstAssignmentOperation operation)
{
    switch (operation) {
    case AST_ASSIGN_SET: return "=";
    case AST_ASSIGN_ADD: return "+=";
    case AST_ASSIGN_SUBTRACT: return "-=";
    case AST_ASSIGN_MULTIPLY: return "*=";
    case AST_ASSIGN_DIVIDE: return "/=";
    case AST_ASSIGN_REMAINDER: return "%=";
    }
    return NULL;
}

PgyTokenType
ast_assignment_binary_operator(AstAssignmentOperation operation)
{
    switch (operation) {
    case AST_ASSIGN_ADD: return TOKEN_PLUS;
    case AST_ASSIGN_SUBTRACT: return TOKEN_MINUS;
    case AST_ASSIGN_MULTIPLY: return TOKEN_STAR;
    case AST_ASSIGN_DIVIDE: return TOKEN_SLASH;
    case AST_ASSIGN_REMAINDER: return TOKEN_PERCENT;
    case AST_ASSIGN_SET: return TOKEN_ASSIGN;
    }
    return TOKEN_ERROR;
}

bool
ast_assignment_set_semantic_binding_type_name_copy(ASTNode* node,
                                                   const char* type_name)
{
    if (node == NULL || node->type != AST_ASSIGNMENT || type_name == NULL ||
        type_name[0] == '\0' || ast_assignment_target(node) == NULL ||
        ast_assignment_target(node)->type != AST_IDENTIFIER)
        return false;
    char* copy = pergyra_strdup(type_name);
    if (copy == NULL)
        return false;
    free(node->data.assignment.semantic_binding_type_name);
    node->data.assignment.semantic_binding_type_name = copy;
    return true;
}

size_t
ast_let_destructure_name_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return 0;
    return node->data.let_destructure.name_count;
}

const char*
ast_let_destructure_name(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE
        || index >= node->data.let_destructure.name_count)
        return NULL;
    return node->data.let_destructure.names[index];
}

uint32_t
ast_let_destructure_binding_stable_id(const ASTNode* node, size_t index)
{
    ClassField *field;

    if (node == NULL || node->type != AST_LET_DESTRUCTURE
        || index >= node->data.let_destructure.name_count)
        return 0;
    if (node->data.let_destructure.field_bindings == NULL)
        return node->data.let_destructure.local_binding_syntax_ids != NULL
            ? node->data.let_destructure.local_binding_syntax_ids[index] : 0;
    field = node->data.let_destructure.field_bindings[index];
    return field != NULL ? field->stable_id : 0;
}

ASTNode*
ast_let_destructure_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return NULL;
    return node->data.let_destructure.initializer;
}

ASTNode*
ast_await_expression(const ASTNode* node)
{
    if (node == NULL || node->type != AST_AWAIT_EXPR)
        return NULL;
    return node->data.await_expr.expression;
}

ASTNode*
ast_channel_send_channel(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_SEND)
        return NULL;
    return node->data.channel_send.channel;
}

ASTNode*
ast_channel_send_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_SEND)
        return NULL;
    return node->data.channel_send.value;
}

ASTNode*
ast_channel_recv_channel(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_RECV)
        return NULL;
    return node->data.channel_recv.channel;
}

ASTNode*
ast_channel_type_element_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_TYPE)
        return NULL;
    return node->data.channel_type.element_type;
}

ASTNode*
ast_channel_type_capacity(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_TYPE)
        return NULL;
    return node->data.channel_type.capacity;
}

ASTNode*
ast_future_type_value_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FUTURE_TYPE)
        return NULL;
    return node->data.future_type.value_type;
}

ASTNode*
ast_unary_operand(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNARY)
        return NULL;
    return node->data.unary.operand;
}

Token
ast_unary_operator(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNARY)
        return (Token){0};
    return node->data.unary.op;
}

ASTNode*
ast_binary_left(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return NULL;
    return node->data.binary.left;
}

ASTNode*
ast_binary_right(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return NULL;
    return node->data.binary.right;
}

Token
ast_binary_operator(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return (Token){0};
    return node->data.binary.op;
}

size_t
ast_array_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_LITERAL)
        return 0;
    return node->data.array_literal.count;
}

ASTNode*
ast_array_literal_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ARRAY_LITERAL
        || node->data.array_literal.elements == NULL
        || index >= node->data.array_literal.count)
        return NULL;
    return node->data.array_literal.elements[index];
}

size_t
ast_tuple_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL)
        return 0;
    return node->data.tuple_literal.count;
}

ASTNode*
ast_tuple_literal_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL
        || index >= node->data.tuple_literal.count)
        return NULL;
    return node->data.tuple_literal.elements[index];
}

const char *
ast_tuple_literal_semantic_type_name(const ASTNode *node)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL
        || node->stable_id == 0 || node->data.tuple_literal.semantic_type_syntax_id != node->stable_id
        || node->data.tuple_literal.semantic_type_name == NULL
        || node->data.tuple_literal.semantic_type_name[0] == '\0'
        || node->data.tuple_literal.semantic_type_child_count != node->data.tuple_literal.count
        || (node->data.tuple_literal.count != 0 && (node->data.tuple_literal.elements == NULL
            || node->data.tuple_literal.semantic_type_children == NULL))) return NULL;
    for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
        const ASTNode *child = node->data.tuple_literal.elements[i];
        if (child == NULL || child != node->data.tuple_literal.semantic_type_children[i].node
            || child->stable_id == 0 || child->stable_id != node->data.tuple_literal.semantic_type_children[i].id
            || child->type != node->data.tuple_literal.semantic_type_children[i].shape) return NULL;
    }
    return node->data.tuple_literal.semantic_type_name;
}

bool
ast_tuple_literal_clear_semantic_type_name(ASTNode *node)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL) return false;
    free(node->data.tuple_literal.semantic_type_name);
    free(node->data.tuple_literal.semantic_type_children);
    node->data.tuple_literal.semantic_type_name = NULL;
    node->data.tuple_literal.semantic_type_children = NULL;
    node->data.tuple_literal.semantic_type_syntax_id = 0;
    node->data.tuple_literal.semantic_type_child_count = 0;
    return true;
}

bool
ast_tuple_literal_set_semantic_type_name_copy(ASTNode *node, const char *type_name)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL || node->stable_id == 0
        || type_name == NULL || type_name[0] == '\0') return false;
    if (node->data.tuple_literal.semantic_type_name != NULL) {
        const char *prior = ast_tuple_literal_semantic_type_name(node);
        return prior != NULL && strcmp(prior, type_name) == 0;
    }
    if (node->data.tuple_literal.semantic_type_children != NULL
        || node->data.tuple_literal.semantic_type_syntax_id != 0
        || node->data.tuple_literal.semantic_type_child_count != 0) return false;
    size_t count = node->data.tuple_literal.count;
    if (count > SIZE_MAX / sizeof(*node->data.tuple_literal.semantic_type_children)
        || (count != 0 && node->data.tuple_literal.elements == NULL)) return false;
    for (size_t i = 0; i < count; i++)
        if (node->data.tuple_literal.elements[i] == NULL
            || node->data.tuple_literal.elements[i]->stable_id == 0) return false;
    char *copy = pergyra_strdup(type_name);
    if (copy == NULL) return false;
    if (count != 0) {
        node->data.tuple_literal.semantic_type_children = calloc(count,
            sizeof(*node->data.tuple_literal.semantic_type_children));
        if (node->data.tuple_literal.semantic_type_children == NULL) { free(copy); return false; }
    }
    for (size_t i = 0; i < count; i++) {
        ASTNode *child = node->data.tuple_literal.elements[i];
        node->data.tuple_literal.semantic_type_children[i].node = child;
        node->data.tuple_literal.semantic_type_children[i].id = child->stable_id;
        node->data.tuple_literal.semantic_type_children[i].shape = child->type;
    }
    node->data.tuple_literal.semantic_type_name = copy;
    node->data.tuple_literal.semantic_type_syntax_id = node->stable_id;
    node->data.tuple_literal.semantic_type_child_count = count;
    return true;
}

size_t
ast_map_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MAP_LITERAL)
        return 0;
    return node->data.map_literal.count;
}

ASTNode*
ast_map_literal_key(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_MAP_LITERAL
        || index >= node->data.map_literal.count)
        return NULL;
    return node->data.map_literal.keys[index];
}

ASTNode*
ast_map_literal_value(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_MAP_LITERAL
        || index >= node->data.map_literal.count)
        return NULL;
    return node->data.map_literal.values[index];
}

size_t
ast_set_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SET_LITERAL)
        return 0;
    return node->data.set_literal.count;
}

ASTNode*
ast_set_literal_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_SET_LITERAL
        || index >= node->data.set_literal.count)
        return NULL;
    return node->data.set_literal.elements[index];
}

ASTNode*
ast_cast_operand(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CAST)
        return NULL;
    return node->data.cast.operand;
}

const char*
ast_cast_target_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CAST)
        return NULL;
    return node->data.cast.target_type;
}

ASTNode*
ast_type_test_operand(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE_TEST)
        return NULL;
    return node->data.type_test.operand;
}

const char*
ast_type_test_target_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE_TEST)
        return NULL;
    return node->data.type_test.target_type;
}

/*
 * Canonical scalar name shared by semantic analysis and both backends so the
 * `expr is Type` predicate folds to an identical boolean on the C and LLVM
 * paths.  Returns one of "Int", "Long", "Float", "Bool", or NULL when the name
 * is not a lowered scalar.  Float and Double collapse to "Float" to match the
 * scalar set the cast lowering already accepts.
 */
const char*
ast_type_name_canonical_scalar(const char* name)
{
    if (name == NULL)
        return NULL;
    if (strcmp(name, "Int") == 0)
        return "Int";
    if (strcmp(name, "Long") == 0)
        return "Long";
    if (strcmp(name, "Float") == 0 || strcmp(name, "Double") == 0)
        return "Float";
    if (strcmp(name, "Bool") == 0)
        return "Bool";
    return NULL;
}

const char*
ast_break_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BREAK)
        return NULL;
    return node->data.break_stmt.label;
}

const char*
ast_continue_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CONTINUE)
        return NULL;
    return node->data.continue_stmt.label;
}

const char*
ast_for_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.label;
}

const char*
ast_for_variable(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.variable;
}

ASTNode*
ast_for_range_start(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.range_start;
}

ASTNode*
ast_for_range_end(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.range_end;
}

ASTNode*
ast_for_iterable(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.iterable;
}

ASTNode*
ast_for_detach_iterable(ASTNode* node)
{
    ASTNode *iterable;
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    iterable = node->data.for_loop.iterable;
    node->data.for_loop.iterable = NULL;
    return iterable;
}

bool
ast_for_attach_iterable(ASTNode* node, ASTNode* iterable)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return false;
    node->data.for_loop.iterable = iterable;
    return true;
}

ASTNode*
ast_for_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.body;
}

ASTNode*
ast_if_condition(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.condition;
}

ASTNode*
ast_if_then_branch(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.then_branch;
}

ASTNode*
ast_if_else_branch(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.else_branch;
}

const char*
ast_while_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.label;
}

ASTNode*
ast_while_condition(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.condition;
}

ASTNode*
ast_while_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.body;
}

ASTNode*
ast_unsafe_block_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return NULL;
    return node->data.unsafe_block.body;
}

ASTNode*
ast_transaction_block_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TRANSACTION_BLOCK)
        return NULL;
    return node->data.transaction_block.body;
}

ASTNode*
ast_fail_stmt_reason(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FAIL_STMT)
        return NULL;
    return node->data.fail_stmt.reason;
}

ASTNode*
ast_defer_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_DEFER_STMT)
        return NULL;
    return node->data.defer_stmt.body;
}

ASTNode*
ast_return_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RETURN)
        return NULL;
    return node->data.return_stmt.value;
}

ASTNode*
ast_give_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_GIVE_STMT)
        return NULL;
    return node->data.give_stmt.value;
}
