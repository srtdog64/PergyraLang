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
    if (ctx == NULL || ctx->mir == NULL)
        return false;

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        if (pgy_mir_routine_uses_thread_pool(&ctx->mir->routines[i]))
            return true;
    }

    return false;
}
