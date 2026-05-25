/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared call contract lookup and callee escape-summary helpers.
 */

#include "slot_summary.h"
#include "type_checker_internal.h"

#include <string.h>

static ASTNode *
call_contract_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

ASTNode *
semantic_lookup_function_param_contract(SemanticContext *ctx,
                                        const char *display_name,
                                        size_t arg_index,
                                        ParamMode *mode_out)
{
    if (mode_out != NULL)
        *mode_out = PARAM_MODE_DEFAULT;

    if (ctx == NULL || display_name == NULL)
        return NULL;

    ASTNode *stmt = semantic_find_function_decl_by_name(ctx, display_name);
    if (stmt == NULL || arg_index >= ast_func_param_count(stmt))
        return NULL;

    if (mode_out != NULL && ast_func_param(stmt, arg_index) != NULL)
        *mode_out = ast_func_param(stmt, arg_index)->mode;
    return stmt;
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

    return slot_analyze_legacy_ast_param_summary_in_program(
        ast_func_body(callee_decl),
        ast_func_param(callee_decl, arg_index) != NULL
            ? ast_func_param(callee_decl, arg_index)->name
            : NULL,
        call_contract_program(ctx));
}

bool
semantic_param_summary_has_any_escape(unsigned summary_mask)
{
    return (summary_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                            | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                            | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0;
}

bool
semantic_param_summary_has_return_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0;
}

bool
semantic_param_summary_has_channel_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0;
}

bool
semantic_param_summary_has_call_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0;
}
