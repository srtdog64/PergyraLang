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

    return pgy_mir_program_uses_thread_pool(ctx->mir);
}
