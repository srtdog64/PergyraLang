#include "transpiler_parallel_capture.h"

#include <stdbool.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_symbols.h"

ASTNode *
transpiler_find_local_let_type_node(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *found = transpiler_find_local_let_type_node(
                ast_block_statement(body, i), base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && ast_let_name(body) != NULL
        && strcmp(ast_let_name(body), base_name) == 0) {
        return ast_let_type(body);
    }
    return NULL;
}

static const char *
transpiler_current_local_type_name(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return NULL;
    return transpiler_find_local_type_name(ctx, ctx->current_func_decl, name);
}

static bool
transpiler_parallel_capture_has_name(char names[MAX_SLOT_VARS][64],
                                     int count,
                                     const char *name)
{
    if (name == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void
transpiler_parallel_add_capture_name(TranspilerCtx *ctx,
                                     const char *name,
                                     char slot_names[MAX_SLOT_VARS][64],
                                     int *slot_count,
                                     char typed_names[MAX_SLOT_VARS][64],
                                     int *typed_count)
{
    if (ctx == NULL || name == NULL || name[0] == '\0'
        || strcmp(name, "self") == 0) {
        return;
    }

    if (is_slot_var(ctx, name)) {
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && slot_count != NULL && *slot_count < MAX_SLOT_VARS) {
            pergyra_str_copy(slot_names[*slot_count],
                sizeof(slot_names[*slot_count]), name);
            (*slot_count)++;
        }
        return;
    }

    if (lookup_typed_entry(ctx, name) != NULL) {
        const char *type_name = lookup_typed_var(ctx, name);
        if (type_name == NULL || strcmp(type_name, "Unknown") == 0) {
            type_name = transpiler_current_local_type_name(ctx, name);
            if (type_name != NULL && type_name[0] != '\0'
                && strcmp(type_name, "Unknown") != 0) {
                register_typed_var(ctx, name, type_name);
            }
        }
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL && *typed_count < MAX_SLOT_VARS) {
            pergyra_str_copy(typed_names[*typed_count],
                sizeof(typed_names[*typed_count]), name);
            (*typed_count)++;
        }
    } else {
        const char *type_name = transpiler_current_local_type_name(ctx, name);
        if (type_name != NULL && type_name[0] != '\0'
            && strcmp(type_name, "Unknown") != 0
            && !transpiler_parallel_capture_has_name(slot_names,
                    slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                    typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL && *typed_count < MAX_SLOT_VARS) {
            register_typed_var(ctx, name, type_name);
            pergyra_str_copy(typed_names[*typed_count],
                sizeof(typed_names[*typed_count]), name);
            (*typed_count)++;
        }
    }
}

void
transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          char slot_names[MAX_SLOT_VARS][64],
                                          int *slot_count,
                                          char typed_names[MAX_SLOT_VARS][64],
                                          int *typed_count)
{
    if (node == NULL || ctx == NULL)
        return;

    switch (node->type) {
    case AST_IDENTIFIER:
        transpiler_parallel_add_capture_name(ctx, ast_identifier_name(node),
            slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_LET_DECL:
        transpiler_parallel_collect_stmt_captures(ast_let_initializer(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ASSIGNMENT:
        transpiler_parallel_collect_stmt_captures(ast_assignment_target(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_assignment_value(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_SEND:
        transpiler_parallel_collect_stmt_captures(ast_channel_send_channel(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_channel_send_value(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_RECV:
        transpiler_parallel_collect_stmt_captures(ast_channel_recv_channel(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CALL:
        transpiler_parallel_collect_stmt_captures(ast_call_callee(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(ast_call_argument(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_MEMBER_ACCESS:
        transpiler_parallel_collect_stmt_captures(ast_member_object(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_ACCESS:
        transpiler_parallel_collect_stmt_captures(ast_array_access_array(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_array_access_index(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(ast_array_literal_element(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_BINARY:
        transpiler_parallel_collect_stmt_captures(ast_binary_left(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_binary_right(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_UNARY:
        transpiler_parallel_collect_stmt_captures(ast_unary_operand(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_AWAIT_EXPR:
        transpiler_parallel_collect_stmt_captures(ast_await_expression(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_SPAWN_EXPR:
        transpiler_parallel_collect_stmt_captures(ast_spawn_function(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < ast_spawn_arg_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(ast_spawn_argument(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_IF_STMT:
        transpiler_parallel_collect_stmt_captures(ast_if_condition(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_if_then_branch(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(ast_if_else_branch(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_RETURN:
        transpiler_parallel_collect_stmt_captures(ast_return_value(node),
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(ast_block_statement(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(ast_parallel_task(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
            transpiler_parallel_collect_stmt_captures(
                ast_async_block_statement(node, i),
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    default:
        break;
    }
}
