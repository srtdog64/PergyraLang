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
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || ctx->mir == NULL)
        return false;

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        if (pgy_mir_routine_uses_thread_pool(routine))
            return true;
    }

    return false;
}
