/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend AST scan for generated specialization requirements.
 */

#include "transpiler_specialization_helpers.h"

#include <stddef.h>

#include "../parser/ast_api.h"

void
ensure_collection_specializations_from_stmt_to(TranspilerCtx *ctx,
                                               CodeBuf *dst,
                                               ASTNode *node)
{
    if (ctx == NULL || dst == NULL || node == NULL)
        return;

    switch (node->type) {
    case AST_FUNC_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst,
            ast_func_return_type(node));
        size_t param_count = ast_func_param_count(node);
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = ast_func_param(node, i);
            if (p != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, p->type);
        }
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_func_body(node));
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_block_statement(node, i));
        }
        break;
    case AST_LET_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst, ast_let_type(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_let_initializer(node));
        break;
    case AST_LET_DESTRUCTURE:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_let_destructure_initializer(node));
        break;
    case AST_WITH_STMT:
        ensure_type_specializations_from_ast_to(ctx, dst,
            ast_with_slot_type(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_with_body(node));
        break;
    case AST_IF_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_if_condition(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_if_then_branch(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_if_else_branch(node));
        break;
    case AST_WHILE_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_while_condition(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_while_body(node));
        break;
    case AST_FOR_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_for_range_start(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_for_range_end(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_for_iterable(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_for_body(node));
        break;
    case AST_MATCH_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_match_subject(node));
        for (size_t i = 0; i < ast_match_case_count(node); i++) {
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_match_case_at(node, i));
        }
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_match_default_body(node));
        break;
    case AST_MATCH_CASE:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_match_case_guard(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_match_case_body(node));
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_async_block_statement(node, i));
        }
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_parallel_task(node, i));
        }
        break;
    default:
        break;
    }
}
