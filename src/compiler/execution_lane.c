/*
 * execution_lane.c — the SEA lane classification policy (pure decision table).
 *
 * See execution_lane.h. This file schedules nothing; it only decides which lane
 * a task's evidence permits. The ordering of the rules IS the contract.
 */
#include "execution_lane.h"

PgyExecutionLane
pgy_classify_execution_lane(const PgyLaneEvidence *e)
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
    if (e->has_pin_or_live_view || e->has_raw_slot_or_channel_capture)
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
    if (e->capture_is_pure_value
        && !e->has_raw_slot_or_channel_capture
        && e->authority_boundary_clear)
        return PGY_LANE_MOVABLE_SCHEDULER;

    /* (6) Pure-value work without a clear authority boundary still parallelises,
       but stays in the bounded pool rather than a migrating one. */
    if (e->capture_is_pure_value)
        return PGY_LANE_WORKER_POOL;

    /* (7) Concurrent site with no movability evidence: keep it local and
       cooperative rather than guessing a stronger lane. */
    return PGY_LANE_LOCAL_ASYNC;
}

PgyExecutionLane
pgy_spawn_lane_from_blocking(bool is_blocking)
{
    PgyLaneEvidence e = {0};
    e.is_concurrent_site = true;
    /* A blocking spawn is exactly the IO/FFI/OS-blocking evidence the policy
       routes to BlockingPool; a non-blocking spawn falls to LocalAsync. Richer
       evidence (pure-value capture, authority) would promote the non-blocking
       case to Worker/Movable through the same policy. */
    e.has_io_or_ffi_effect = is_blocking;
    return pgy_classify_execution_lane(&e);
}

bool
pgy_lane_uses_blocking_executor(PgyExecutionLane lane)
{
    return lane == PGY_LANE_BLOCKING_POOL;
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
