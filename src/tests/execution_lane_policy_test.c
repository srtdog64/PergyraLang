/*
 * execution_lane_policy_test.c — decision-table proof for the SEA ExecutionLane
 * classification policy (src/compiler/execution_lane.c).
 *
 * SEA's contract is that the same evidence always yields the same lane,
 * independent of any runtime. This test pins the whole decision table, including
 * the two load-bearing edges: the fail-closed REJECT (a pinned resource asked to
 * move) and the strict MovableScheduler gate (M:N only when every capture is a
 * pure value AND the authority boundary is clear).
 *
 * Built and run by tests/execution_lane_policy_smoke.sh.
 */
#include "execution_lane.h"
#include <stdio.h>

static int fails = 0;

static void
chk(const char *name, BoundaryCaptureFact fact, PgyExecutionLane want)
{
    PgyExecutionLane got = pgy_classify_execution_lane(&fact);
    if (got != want) {
        printf("FAIL %-22s got=%s want=%s\n",
               name, pgy_execution_lane_name(got), pgy_execution_lane_name(want));
        fails++;
    } else {
        printf("ok   %-22s -> %s\n", name, pgy_execution_lane_name(got));
    }
}

int
main(void)
{
    BoundaryCaptureFact base = {0};
    base.is_concurrent_site = true;

    { BoundaryCaptureFact e = {0};
      chk("not-concurrent", e, PGY_LANE_INLINE); }

    { BoundaryCaptureFact e = base; e.captures_pin = true;
      chk("pin-view", e, PGY_LANE_PINNED_ZONE); }

    { BoundaryCaptureFact e = base; e.captures_raw_slot = true;
      chk("raw-slot", e, PGY_LANE_PINNED_ZONE); }

    { BoundaryCaptureFact e = base; e.captures_pin = true; e.requires_movability = true;
      chk("pin+move(reject)", e, PGY_LANE_REJECT); }

    { BoundaryCaptureFact e = base; e.captures_live_view = true; e.requires_movability = true;
      chk("live-view+move", e, PGY_LANE_REJECT); }

    { BoundaryCaptureFact e = base; e.captures_raw_channel = true; e.requires_movability = true;
      chk("raw-channel+move", e, PGY_LANE_REJECT); }

    { BoundaryCaptureFact e = base; e.has_io_or_ffi_effect = true;
      e.captures_value_only = true; e.crosses_authority_boundary = true;
      chk("io-ffi", e, PGY_LANE_BLOCKING_POOL); }

    { BoundaryCaptureFact e = base; e.is_await_heavy_local = true;
      chk("await-local", e, PGY_LANE_LOCAL_ASYNC); }

    { BoundaryCaptureFact e = base; e.is_deterministic_fork_join = true;
      e.captures_value_only = true; e.crosses_authority_boundary = true;
      chk("fork-join", e, PGY_LANE_WORKER_POOL); }

    { BoundaryCaptureFact e = base; e.captures_value_only = true; e.crosses_authority_boundary = true;
      chk("movable(M:N)", e, PGY_LANE_MOVABLE_SCHEDULER); }

    { BoundaryCaptureFact e = base; e.captures_value_only = true;
      chk("pure-no-authority", e, PGY_LANE_WORKER_POOL); }

    { BoundaryCaptureFact e = base;
      chk("bare-concurrent", e, PGY_LANE_LOCAL_ASYNC); }

    if (fails) { printf("\n%d FAIL\n", fails); return 1; }
    printf("\nALL PASS (12/12)\n");
    return 0;
}
