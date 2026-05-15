/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local type-AST lookup helpers.
 */

#include "transpiler_mir_local_type_ast_lookup.h"

#include <string.h>

#include "transpiler_decl_lookup.h"

static ASTNode *
transpiler_find_local_type_ast_in_block(TranspilerCtx *ctx,
                                        ASTNode *body,
                                        const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_type_ast_in_block(
                ctx, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL)
            return body->data.let_decl.type;
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_CALL
            && ast_call_callee(body->data.let_decl.initializer) != NULL
            && ast_call_callee(body->data.let_decl.initializer)->type == AST_IDENTIFIER
            && ast_call_callee(body->data.let_decl.initializer)->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                ast_call_callee(body->data.let_decl.initializer)->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && ast_func_return_type(decl) != NULL
                && ast_func_return_type(decl)->type == AST_EVENT_HANDLER_TYPE) {
                return ast_func_return_type(decl);
            }
        }
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && ast_func_return_type(decl) != NULL
                && ast_func_return_type(decl)->type == AST_EVENT_HANDLER_TYPE) {
                return ast_func_return_type(decl);
            }
        }
        return NULL;
    }
    if (body->type == AST_WITH_STMT)
        return transpiler_find_local_type_ast_in_block(
            ctx, ast_with_body(body), base_name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_local_type_ast_in_block(
            ctx, ast_if_then_branch(body), base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_ast_in_block(
            ctx, ast_if_else_branch(body), base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, ast_while_body(body), base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, ast_for_body(body), base_name);
    return NULL;
}

ASTNode *
transpiler_find_local_type_ast(TranspilerCtx *ctx,
                               const ASTNode *func_decl,
                               const char *base_name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || ast_func_body(func_decl) == NULL
        || base_name == NULL) {
        return NULL;
    }
    return transpiler_find_local_type_ast_in_block(
        ctx, ast_func_body(func_decl), base_name);
}
