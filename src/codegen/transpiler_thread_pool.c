/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend runtime thread-pool dependency analysis.
 */

#include "transpiler_thread_pool.h"
#include "thread_pool_usage.h"

bool
transpiler_requires_thread_pool(const TranspilerCtx *ctx)
{
    ASTNode *synthetic_executable_func;

    if (ctx == NULL || ctx->mir == NULL)
        return false;

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        if (pgy_mir_routine_uses_thread_pool(&ctx->mir->routines[i]))
            return true;
    }

    synthetic_executable_func = mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    if (synthetic_executable_func != NULL
        && synthetic_executable_func->type == AST_FUNC_DECL
        && pgy_ast_uses_thread_pool(synthetic_executable_func->data.func_decl.body)) {
        return true;
    }

    return false;
}
