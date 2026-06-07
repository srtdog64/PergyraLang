/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local EventHandler type-AST compatibility owner.
 */

#include "transpiler_mir_local_type_ast_lookup.h"

#include <string.h>

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "../parser/ast_api.h"

static ASTNode *
transpiler_event_handler_return_type_from_decl(TranspilerCtx *ctx,
                                               ASTNode *decl,
                                               const char *callee_name)
{
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    if (transpiler_active_has_mir(ctx)) {
        const MIRRoutine *routine = transpiler_find_mir_function(ctx, decl);
        ASTNode *return_type = NULL;
        if (routine == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing local EventHandler routine metadata for '%s'",
                callee_name != NULL ? callee_name : "(anonymous)");
            return NULL;
        }
        if (!transpiler_mir_routine_has_signature(routine)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing local EventHandler signature metadata for '%s'",
                callee_name != NULL ? callee_name : "(anonymous)");
            return NULL;
        }
        return_type = transpiler_mir_routine_return_type(routine);
        return return_type != NULL
            && return_type->type == AST_EVENT_HANDLER_TYPE
                ? return_type
                : NULL;
    }
    {
        ASTNode *return_type = ast_func_return_type(decl);
        return return_type != NULL
            && return_type->type == AST_EVENT_HANDLER_TYPE
                ? return_type
                : NULL;
    }
}

static ASTNode *
transpiler_find_local_event_handler_type_ast_in_block(TranspilerCtx *ctx,
                                                      ASTNode *body,
                                                      const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *found = transpiler_find_local_event_handler_type_ast_in_block(
                ctx, ast_block_statement(body, i), base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && ast_let_name(body) != NULL
        && strcmp(ast_let_name(body), base_name) == 0) {
        ASTNode *let_type = ast_let_type(body);
        ASTNode *let_init = ast_let_initializer(body);
        if (let_type != NULL && let_type->type == AST_EVENT_HANDLER_TYPE)
            return let_type;
        if (let_init != NULL
            && let_init->type == AST_CALL
            && ast_call_callee(let_init) != NULL
            && ast_call_callee(let_init)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(let_init)) != NULL) {
            const char *callee =
                ast_identifier_name(ast_call_callee(let_init));
            return transpiler_event_handler_return_type_from_decl(
                ctx, find_function_decl(ctx, callee), callee);
        }
        if (let_init != NULL
            && let_init->type == AST_IDENTIFIER
            && ast_identifier_name(let_init) != NULL) {
            const char *callee = ast_identifier_name(let_init);
            return transpiler_event_handler_return_type_from_decl(
                ctx, find_function_decl(ctx, callee), callee);
        }
        return NULL;
    }
    if (body->type == AST_WITH_STMT)
        return transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_with_body(body), base_name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_if_then_branch(body), base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_if_else_branch(body), base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_while_body(body), base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_for_body(body), base_name);
    if (body->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < ast_match_case_count(body); i++) {
            ASTNode *mc = ast_match_case_at(body, i);
            if (mc == NULL)
                continue;
            ASTNode *found = transpiler_find_local_event_handler_type_ast_in_block(
                ctx, ast_match_case_body(mc), base_name);
            if (found != NULL)
                return found;
        }
        return transpiler_find_local_event_handler_type_ast_in_block(
            ctx, ast_match_default_body(body), base_name);
    }
    return NULL;
}

ASTNode *
transpiler_find_local_event_handler_type_ast(TranspilerCtx *ctx,
                                             const ASTNode *func_decl,
                                             const char *base_name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || ast_func_body(func_decl) == NULL
        || base_name == NULL) {
        return NULL;
    }
    return transpiler_find_local_event_handler_type_ast_in_block(
        ctx, ast_func_body(func_decl), base_name);
}
