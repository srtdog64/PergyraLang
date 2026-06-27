/*
 * lane_scheduler_test.c — proof of the SEA runtime facade contract
 * (src/runtime/pgy_lane_scheduler.c).
 *
 * The two load-bearing properties:
 *   1. executor-invariance: the same task with the same input yields the same
 *      result on every non-Reject lane. The scheduler must not change meaning.
 *   2. fail-closed Reject: a Reject lane never runs the task.
 *
 * Built and run by tests/lane_scheduler_smoke.sh.
 */
#include "pgy_lane_scheduler.h"
#include <stdint.h>
#include <stdio.h>

static int g_ran = 0;

static void *
task_plus42(void *arg)
{
    g_ran++;
    return (void *)((intptr_t)arg + 42);
}

int
main(void)
{
    int fails = 0;
    PgyExecutionLane non_reject[] = {
        PGY_LANE_INLINE, PGY_LANE_PINNED_ZONE, PGY_LANE_BLOCKING_POOL,
        PGY_LANE_LOCAL_ASYNC, PGY_LANE_WORKER_POOL, PGY_LANE_MOVABLE_SCHEDULER
    };

    for (size_t i = 0; i < sizeof(non_reject) / sizeof(non_reject[0]); i++) {
        void *r = NULL;
        g_ran = 0;
        PgyLaneDispatchStatus s =
            pgy_lane_dispatch(non_reject[i], task_plus42, (void *)(intptr_t)100, &r);
        intptr_t got = (intptr_t)r;
        const char *ln = pgy_execution_lane_name(non_reject[i]);
        if (s != PGY_LANE_DISPATCH_OK || got != 142 || g_ran != 1) {
            printf("FAIL %-16s status=%d result=%ld ran=%d\n",
                   ln, (int)s, (long)got, g_ran);
            fails++;
        } else {
            printf("ok   %-16s -> %-22s result=142\n",
                   ln, pgy_lane_executor_name(non_reject[i]));
        }
    }

    {
        void *r = (void *)(intptr_t)-1;
        g_ran = 0;
        PgyLaneDispatchStatus s =
            pgy_lane_dispatch(PGY_LANE_REJECT, task_plus42, (void *)(intptr_t)100, &r);
        if (s != PGY_LANE_DISPATCH_REJECTED || g_ran != 0) {
            printf("FAIL Reject status=%d ran=%d (must not run)\n", (int)s, g_ran);
            fails++;
        } else {
            printf("ok   Reject           -> fail-closed, task not run\n");
        }
    }

    {
        PgyLaneDispatchStatus s =
            pgy_lane_dispatch(PGY_LANE_INLINE, NULL, NULL, NULL);
        if (s != PGY_LANE_DISPATCH_INVALID) {
            printf("FAIL null-task status=%d\n", (int)s);
            fails++;
        } else {
            printf("ok   null-task        -> invalid\n");
        }
    }

    if (fails) { printf("\n%d FAIL\n", fails); return 1; }
    printf("\nALL PASS\n");
    return 0;
}
