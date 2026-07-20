/*
 * execution_lane.c — the SEA lane classification policy (pure decision table).
 *
 * See execution_lane.h. This file schedules nothing; it only decides which lane
 * a task's evidence permits. The ordering of the rules IS the contract.
 */
#include "execution_lane.h"

PgyExecutionLane
pgy_classify_execution_lane(const BoundaryCaptureFact *e)
{
    if (e == NULL)
        return PGY_LANE_REJECT;

    /* Not a concurrency site at all: run in place. */
    if (!e->is_concurrent_site)
        return PGY_LANE_INLINE;

    /* (1) Resource evidence wins first. A live pin/view or a raw Slot/Channel
       capture binds the task to its owner. If the SAME site also demands
       movability (handed to a detached/movable executor), that is a
       contradiction the evidence cannot satisfy — fail closed, do not silently
       pick a weaker lane. Otherwise it is pinned to its zone. */
    if (e->captures_pin || e->captures_live_view
        || e->captures_raw_slot || e->captures_raw_channel)
    {
        if (e->requires_movability)
            return PGY_LANE_REJECT;
        return PGY_LANE_PINNED_ZONE;
    }

    /* (2) IO/FFI/OS-blocking gets its own lane regardless of shape, so blocking
       calls never starve a cooperative or work-stealing lane. */
    if (e->has_io_or_ffi_effect)
        return PGY_LANE_BLOCKING_POOL;

    /* (3) Await-heavy work that only touches local state is cooperative within
       its owner — "can wait", not "can move". */
    if (e->is_await_heavy_local)
        return PGY_LANE_LOCAL_ASYNC;

    /* (4) Small scoped fork-join with a joined result is deterministic: a
       bounded worker pool, NOT M:N migration (determinism over throughput). */
    if (e->is_deterministic_fork_join)
        return PGY_LANE_WORKER_POOL;

    /* (5) The only path to the movable M:N lane: every capture is a pure copied
       value, nothing raw is held, and the authority boundary is explicit. This
       is the strictest gate — M:N is an optimisation unlocked by evidence, not a
       default. */
    if (e->captures_value_only
        && !e->captures_raw_slot
        && !e->captures_raw_channel
        && e->crosses_authority_boundary)
        return PGY_LANE_MOVABLE_SCHEDULER;

    /* (6) Pure-value work without a clear authority boundary still parallelises,
       but stays in the bounded pool rather than a migrating one. */
    if (e->captures_value_only)
        return PGY_LANE_WORKER_POOL;

    /* (7) Concurrent site with no movability evidence: keep it local and
       cooperative rather than guessing a stronger lane. */
    return PGY_LANE_LOCAL_ASYNC;
}

const char *
pgy_execution_lane_name(PgyExecutionLane lane)
{
    switch (lane)
    {
        case PGY_LANE_REJECT:            return "Reject";
        case PGY_LANE_INLINE:            return "Inline";
        case PGY_LANE_PINNED_ZONE:       return "PinnedZone";
        case PGY_LANE_BLOCKING_POOL:     return "BlockingPool";
        case PGY_LANE_LOCAL_ASYNC:       return "LocalAsync";
        case PGY_LANE_WORKER_POOL:       return "WorkerPool";
        case PGY_LANE_MOVABLE_SCHEDULER: return "MovableScheduler";
    }
    return "Reject";
}
