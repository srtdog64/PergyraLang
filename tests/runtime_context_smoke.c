#include "../src/runtime/pgy_runtime.h"

#include <stdio.h>

int
main(void)
{
    PgyRuntimeContext first;
    PgyRuntimeContext second;

    pgy_runtime_context_init(&first, 101);
    pgy_runtime_context_init(&second, 202);
    if (!pgy_runtime_context_bind(&first)
        || pgy_runtime_context_instance_id() != 101)
        return 1;

    pgy_cap_set_manifest_export(PGY_CAP_IO_READ);
    pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 64);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 32, "context-first");

    if (!pgy_runtime_context_bind(&second)
        || pgy_runtime_context_instance_id() != 202
        || pgy_cap_granted_export() != PGY_CAP_ALL
        || pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) != 0)
        return 2;

    pgy_cap_set_manifest_export(PGY_CAP_NETWORK);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 16, "context-second");
    if (!pgy_runtime_context_bind(&first)
        || pgy_runtime_context_instance_id() != 101
        || pgy_cap_granted_export() != PGY_CAP_IO_READ
        || pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) != 32)
        return 3;

    pgy_runtime_context_unbind();
    if (pgy_runtime_context_instance_id() != 0)
        return 4;
    puts("[runtime-context] PASS capability and budget isolation by bound instance");
    return 0;
}
