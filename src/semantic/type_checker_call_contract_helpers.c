/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared call contract lookup and callee escape-summary helpers.
 */

#include "slot_analyzer.h"
#include "type_checker_internal.h"

#include <string.h>

ASTNode *
semantic_lookup_function_param_contract(SemanticContext *ctx,
                                        const char *display_name,
                                        size_t arg_index,
                                        ParamMode *mode_out)
{
    if (mode_out != NULL)
        *mode_out = PARAM_MODE_DEFAULT;

    if (ctx == NULL || ctx->program_root == NULL || display_name == NULL)
        return NULL;

    ASTNode *prog = ctx->program_root;
    for (size_t si = 0; si < ast_program_statement_count(prog); si++) {
        ASTNode *stmt = ast_program_statement(prog, si);
        const char *stmt_name = ast_declaration_name(stmt);
        if (stmt == NULL || stmt->type != AST_FUNC_DECL
            || stmt_name == NULL
            || strcmp(stmt_name, display_name) != 0
            || arg_index >= ast_func_param_count(stmt)) {
            continue;
        }
        if (mode_out != NULL && ast_func_param(stmt, arg_index) != NULL)
            *mode_out = ast_func_param(stmt, arg_index)->mode;
        return stmt;
    }

    return NULL;
}

unsigned
semantic_callable_param_escape_summary(ASTNode *callee_decl,
                                       size_t arg_index,
                                       SemanticContext *ctx)
{
    if (callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL
        || ast_func_body(callee_decl) == NULL
        || arg_index >= ast_func_param_count(callee_decl)) {
        return 0u;
    }

    return slot_analyze_param_summary_in_program(
        ast_func_body(callee_decl),
        ast_func_param(callee_decl, arg_index) != NULL
            ? ast_func_param(callee_decl, arg_index)->name
            : NULL,
        ctx != NULL ? ctx->program_root : NULL);
}
