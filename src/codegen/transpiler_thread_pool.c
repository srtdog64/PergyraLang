/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend runtime thread-pool dependency analysis.
 */

#include "transpiler_thread_pool.h"

bool
transpiler_requires_thread_pool(const TranspilerCtx *ctx)
{
    return transpiler_active_uses_thread_pool(ctx);
}
