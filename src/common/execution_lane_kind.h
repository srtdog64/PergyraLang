/*
 * execution_lane_kind.h -- shared SEA ExecutionLane kind.
 *
 * This header owns only the lane enum. Compiler-only evidence such as
 * BoundaryCaptureFact lives in src/compiler/execution_lane.h and must not leak
 * into generated C or runtime headers.
 */
#ifndef PERGYRA_COMMON_EXECUTION_LANE_KIND_H
#define PERGYRA_COMMON_EXECUTION_LANE_KIND_H

/*
 * The lane a task is assigned to. Ordered from most-constrained (must stay put)
 * to most-movable. REJECT is the fail-closed verdict: the evidence is
 * contradictory (a pinned/raw resource asked to move) and no lane is sound.
 */
typedef enum
{
    PGY_LANE_REJECT = 0,        /* fail-closed: pinned/raw resource cannot move */
    PGY_LANE_INLINE,            /* no concurrency need -- run in place */
    PGY_LANE_PINNED_ZONE,       /* has pin/slot/live-view: bound to its owner/zone */
    PGY_LANE_BLOCKING_POOL,     /* IO/FFI/OS blocking: its own lane */
    PGY_LANE_LOCAL_ASYNC,       /* await-heavy + local state: cooperative, same owner */
    PGY_LANE_WORKER_POOL,       /* deterministic fork-join / pure value, bounded pool */
    PGY_LANE_MOVABLE_SCHEDULER  /* the ONLY M:N lane: pure value, no pin/raw,
                                   authority boundary clear */
} PgyExecutionLane;

#endif /* PERGYRA_COMMON_EXECUTION_LANE_KIND_H */
