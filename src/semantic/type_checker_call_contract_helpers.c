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
    for (size_t si = 0; si < prog->data.program.count; si++) {
        ASTNode *stmt = prog->data.program.statements[si];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL
            || stmt->data.func_decl.name == NULL
            || strcmp(stmt->data.func_decl.name, display_name) != 0
            || arg_index >= stmt->data.func_decl.param_count) {
            continue;
        }
        if (mode_out != NULL && stmt->data.func_decl.params[arg_index] != NULL)
            *mode_out = stmt->data.func_decl.params[arg_index]->mode;
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
        || callee_decl->data.func_decl.body == NULL
        || arg_index >= callee_decl->data.func_decl.param_count) {
        return 0u;
    }

    return slot_analyze_param_summary_in_program(
        callee_decl->data.func_decl.body,
        callee_decl->data.func_decl.params[arg_index] != NULL
            ? callee_decl->data.func_decl.params[arg_index]->name
            : NULL,
        ctx != NULL ? ctx->program_root : NULL);
}
