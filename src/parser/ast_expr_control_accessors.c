/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner. Keep responsibility slices below the 600 LOC signal.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

GenericParams*
ast_call_generic_args(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.generic_args;
}

size_t
ast_call_generic_arg_count(const ASTNode* node)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    return generic_args != NULL ? generic_args->count : 0;
}

GenericParam*
ast_call_generic_arg(const ASTNode* node, size_t index)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    if (generic_args == NULL
        || generic_args->params == NULL
        || index >= generic_args->count) {
        return NULL;
    }
    return generic_args->params[index];
}

ASTNode*
ast_call_callee(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.callee;
}

size_t
ast_call_arg_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return 0;
    return node->data.call.arg_count;
}

ASTNode**
ast_call_arguments(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_call_arg_count(node);
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.arguments;
}

ASTNode*
ast_call_argument(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CALL
        || index >= node->data.call.arg_count) {
        return NULL;
    }
    return node->data.call.arguments[index];
}

const char*
ast_call_argument_name(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL
        || index >= node->data.call.arg_count) {
        return NULL;
    }
    return node->data.call.arg_names[index];
}

bool
ast_call_has_named_arguments(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL) {
        return false;
    }
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        if (node->data.call.arg_names[i] != NULL)
            return true;
    }
    return false;
}

ASTNode*
ast_call_find_named_argument(const ASTNode* node, const char* field_name)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL || field_name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        const char* nm = node->data.call.arg_names[i];
        if (nm != NULL && strcmp(nm, field_name) == 0)
            return node->data.call.arguments[i];
    }
    return NULL;
}

bool
ast_replace_identifier_name_copy(ASTNode* node, const char* name)
{
    char *copy;

    if (node == NULL || node->type != AST_IDENTIFIER || name == NULL)
        return false;
    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    free(node->data.identifier.name);
    node->data.identifier.name = copy;
    return true;
}

void
ast_init_call_borrowed_view(ASTNode* node, ASTNode* callee,
                            ASTNode** arguments, size_t arg_count)
{
    if (node == NULL)
        return;
    memset(node, 0, sizeof(*node));
    node->type = AST_CALL;
    node->data.call.callee = callee;
    node->data.call.arguments = arguments;
    node->data.call.arg_count = arg_count;
}

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
