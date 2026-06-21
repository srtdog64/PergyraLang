/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Resource-budget gate test (the quantitative sandbox boundary).
 *
 *   no arg      : granted-path unit tests (default unlimited; a charge under the
 *                 imposed limit succeeds; the running total is observable).
 *   "deny-alloc": impose a small ALLOC_BYTES budget, then charge past it -- must
 *                 panic fail-closed (class budget-exceeded).
 *   "deny-spawn": impose a SPAWN_COUNT budget of 2, spawn-charge three times --
 *                 the third must panic. The harness asserts the abort.
 */

#include "pgy_runtime.h"

#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv)
{
    int fail = 0;

    if (argc > 1 && strcmp(argv[1], "deny-alloc") == 0) {
        pgy_budget_reset_export();
        pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 100);
        printf("charging 101 bytes under a 100-byte budget"
               " (expect budget-exceeded)\n");
        fflush(stdout);
        pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 101, "alloc");
        printf("ERROR: charge returned without a budget panic\n");
        return 1;                       /* unreachable on a correct gate */
    }

    if (argc > 1 && strcmp(argv[1], "deny-spawn") == 0) {
        pgy_budget_reset_export();
        pgy_budget_set_limit_export(PGY_BUDGET_SPAWN_COUNT, 2);
        printf("spawn #1\n"); pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");
        printf("spawn #2\n"); pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");
        printf("spawn #3 (expect budget-exceeded)\n"); fflush(stdout);
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");
        printf("ERROR: third spawn returned without a budget panic\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "deny-alloc-real") == 0) {
        /* The WIRED path: a real pgy_alloc charges the budget via the allocator
         * accounting hook, not a direct charge call. */
        PgyAllocator a = pgy_allocator_pool(4096);
        pgy_budget_reset_export();
        pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 100);
        printf("allocating 200 bytes via pgy_alloc under a 100-byte budget"
               " (expect budget-exceeded)\n");
        fflush(stdout);
        (void)pgy_alloc(&a, 200, 8);   /* charges 200 > 100 -> panic */
        printf("ERROR: pgy_alloc returned without a budget panic\n");
        return 1;
    }

    printf("=== Resource Budget Gate Test ===\n");

    /* Default: every kind unlimited, so charges never panic. */
    pgy_budget_reset_export();
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 1000000, "alloc");
    if (pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) == 1000000)
        printf("  [PASS] default unlimited -> charge records, never denies\n");
    else { printf("  [FAIL] default unlimited\n"); fail++; }

    /* Imposed budget: charges up to the ceiling are allowed. */
    pgy_budget_reset_export();
    pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 1000);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 500, "alloc");
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 500, "alloc"); /* total == limit, ok */
    if (pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) == 1000)
        printf("  [PASS] charge up to the ceiling is allowed (used=1000/1000)\n");
    else { printf("  [FAIL] at-ceiling charge\n"); fail++; }

    /* Budgets are per-kind: a different kind is independent. */
    pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 7, "spawn");
    if (pgy_budget_used_export(PGY_BUDGET_SPAWN_COUNT) == 7)
        printf("  [PASS] budgets are per-kind (spawn used=7 under unlimited)\n");
    else { printf("  [FAIL] per-kind independence\n"); fail++; }

    /* Wired allocator path: under an imposed budget, a real pgy_alloc charges
     * the running total -- proves the gate is wired into the allocator, not
     * just directly callable. */
    {
        PgyAllocator a = pgy_allocator_pool(4096);
        pgy_budget_reset_export();
        pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 100000);
        (void)pgy_alloc(&a, 512, 8);
        if (pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) >= 512)
            printf("  [PASS] wired allocator charges budget (used>=512 after pgy_alloc)\n");
        else {
            printf("  [FAIL] allocator not wired (used=%llu)\n",
                   (unsigned long long)pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES));
            fail++;
        }
        pgy_allocator_destroy(&a);
    }

    /* Trusted default: no budget imposed -> allocator charges nothing. */
    {
        PgyAllocator a = pgy_allocator_pool(4096);
        pgy_budget_reset_export();
        (void)pgy_alloc(&a, 512, 8);
        if (pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) == 0)
            printf("  [PASS] no budget imposed -> allocator fast-path skips (used=0)\n");
        else { printf("  [FAIL] fast-path charged without a budget\n"); fail++; }
        pgy_allocator_destroy(&a);
    }

    pgy_budget_reset_export();

    if (fail == 0)
        printf("ALL PASS (0 failures)\n");
    else
        printf("FAILED (%d)\n", fail);
    return fail == 0 ? 0 : 1;
}
