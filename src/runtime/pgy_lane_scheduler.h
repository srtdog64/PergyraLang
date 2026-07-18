#include "pgy_runtime_linkage.h"
/*
 * pgy_lane_scheduler.h — SEA runtime facade (docs/146 layer 3).
 *
 * The compiler decides a task's ExecutionLane (the ExecutionLaneFact, see
 * src/compiler/execution_lane.h). This facade is the ONLY thing that turns that
 * decision into execution: it maps a lane to a concrete executor and runs the
 * task. SEA names the contract; ExecutionLaneFact is the decision; this is the
 * runtime that consumes it.
 *
 * Contract:
 *   - PGY_LANE_REJECT fails closed: the task is NOT run.
 *   - every other lane runs the task and yields the same result for the same
 *     input — the executor must not change the program's meaning, only where/how
 *     the work runs. That observational equality is the whole point of keeping
 *     M:N below the meaning line.
 */
#ifndef PERGYRA_LANE_SCHEDULER_H
#define PERGYRA_LANE_SCHEDULER_H

#include "../common/execution_lane_kind.h"

typedef void *(*PgyLaneTaskFn)(void *arg);

typedef enum
{
    PGY_LANE_DISPATCH_OK = 0,
    PGY_LANE_DISPATCH_REJECTED,   /* fail-closed: lane was Reject, task not run */
    PGY_LANE_DISPATCH_INVALID,    /* null task or unknown lane */
    PGY_LANE_DISPATCH_EXECUTOR_ERROR /* the executor could not start the task */
} PgyLaneDispatchStatus;

/*
 * Run `fn(arg)` on the executor `lane` selects. On success stores the task's
 * return value in *result_out (if non-NULL) and returns PGY_LANE_DISPATCH_OK.
 * Reject never runs the task. Blocking: returns only after the task completes
 * (a synchronous facade; an async/handle-returning form is a later addition).
 */
PgyLaneDispatchStatus
pgy_lane_dispatch(PgyExecutionLane lane, PgyLaneTaskFn fn, void *arg,
                  void **result_out);

/*
 * Spawn-shaped lane facade. This form preserves Future<T> / PgyTaskHandle
 * semantics while keeping the concrete executor choice behind the lane fact.
 * It is available when pgy_parallel.h has already defined the task-handle ABI.
 */
#ifdef PERGYRA_RUNTIME_PGY_PARALLEL_H
PGY_RT_DECL PgyTaskHandle
pgy_lane_spawn_dispatch(PgyExecutionLane lane, PgyLaneTaskFn fn, void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyTaskHandle handle = {0};

    if (fn == NULL) {
        pgy_parallel_warn("lane-spawn", "task function is null");
        return handle;
    }

    switch (lane)
    {
        case PGY_LANE_REJECT:
            pgy_parallel_warn("lane-spawn", "execution lane rejected task");
            return handle;

        case PGY_LANE_INLINE:
        case PGY_LANE_PINNED_ZONE:
            return pgy_spawn_inline_completed(fn, arg, "lane-spawn", true,
                                              lane);

        case PGY_LANE_BLOCKING_POOL:
            handle = pgy_spawn_blocking(fn, arg);
            break;

        case PGY_LANE_LOCAL_ASYNC:
            handle = pgy_async_spawn(fn, arg);
            break;

        case PGY_LANE_WORKER_POOL:
        case PGY_LANE_MOVABLE_SCHEDULER:
            handle = pgy_spawn(fn, arg);
            break;

        default:
            pgy_parallel_warn("lane-spawn", "unknown execution lane");
            return handle;
    }

    pgy_task_handle_set_lane(handle, lane);
    return handle;
}
#else
;
#endif


PGY_RT_DECL void *
pgy_lane_await(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    switch (pgy_task_handle_lane(handle))
    {
        case PGY_LANE_REJECT:
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "await rejected execution lane");

        case PGY_LANE_INLINE:
        case PGY_LANE_PINNED_ZONE:
        case PGY_LANE_BLOCKING_POOL:
        case PGY_LANE_LOCAL_ASYNC:
        case PGY_LANE_WORKER_POOL:
        case PGY_LANE_MOVABLE_SCHEDULER:
            return pgy_await(handle);
    }

    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      "await unknown execution lane");
}
#else
;
#endif


PGY_RT_DECL void
pgy_lane_detach(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    switch (pgy_task_handle_lane(handle))
    {
        case PGY_LANE_LOCAL_ASYNC:
            pgy_async_detach(handle);
            return;

        case PGY_LANE_REJECT:
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "detach rejected execution lane");

        case PGY_LANE_INLINE:
        case PGY_LANE_PINNED_ZONE:
        case PGY_LANE_BLOCKING_POOL:
        case PGY_LANE_WORKER_POOL:
        case PGY_LANE_MOVABLE_SCHEDULER:
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "detach requires LocalAsync execution lane");
    }

    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      "detach unknown execution lane");
}
#else
;
#endif


PGY_RT_DECL bool
pgy_lane_cancel(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    switch (pgy_task_handle_lane(handle))
    {
        case PGY_LANE_REJECT:
            return false;

        case PGY_LANE_INLINE:
        case PGY_LANE_PINNED_ZONE:
        case PGY_LANE_BLOCKING_POOL:
        case PGY_LANE_LOCAL_ASYNC:
        case PGY_LANE_WORKER_POOL:
        case PGY_LANE_MOVABLE_SCHEDULER:
            return pgy_task_cancel(handle);
    }

    return false;
}
#else
;
#endif

#endif

/* The executor name a lane routes to, for diagnostics/tracing. Never NULL. */
const char *pgy_lane_executor_name(PgyExecutionLane lane);

#endif /* PERGYRA_LANE_SCHEDULER_H */
