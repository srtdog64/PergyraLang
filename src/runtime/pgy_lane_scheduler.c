/*
 * pgy_lane_scheduler.c — SEA runtime facade. Maps an ExecutionLane to an
 * executor and runs the task. See pgy_lane_scheduler.h.
 *
 * Executor mapping (first-cut; the lane decision is final, the executor backing
 * it can deepen without changing observable results):
 *   Inline / PinnedZone        -> run in place on the calling/owner thread.
 *   WorkerPool / BlockingPool /
 *   LocalAsync / MovableScheduler -> run on a worker thread, joined for the
 *                                 result. (The fiber scheduler / work-stealing
 *                                 pool is the production refinement; the facade
 *                                 contract is the same either way: the result is
 *                                 executor-invariant.)
 *   Reject                     -> fail closed, do not run.
 *
 * The synchronous join keeps the facade's observable contract identical across
 * lanes, which is exactly the property the SEA design wants to hold: the
 * scheduler does not leak into the program's meaning.
 */
#include "pgy_lane_scheduler.h"

#include <pthread.h>
#include <stddef.h>

typedef struct
{
    PgyLaneTaskFn fn;
    void         *arg;
    void         *result;
} LaneThreadCtx;

static void *
lane_thread_trampoline(void *p)
{
    LaneThreadCtx *ctx = (LaneThreadCtx *)p;
    ctx->result = ctx->fn(ctx->arg);
    return NULL;
}

static PgyLaneDispatchStatus
lane_run_on_worker(PgyLaneTaskFn fn, void *arg, void **result_out)
{
    LaneThreadCtx ctx;
    pthread_t     thread;

    ctx.fn = fn;
    ctx.arg = arg;
    ctx.result = NULL;

    if (pthread_create(&thread, NULL, lane_thread_trampoline, &ctx) != 0)
        return PGY_LANE_DISPATCH_EXECUTOR_ERROR;
    pthread_join(thread, NULL);

    if (result_out != NULL)
        *result_out = ctx.result;
    return PGY_LANE_DISPATCH_OK;
}

PgyLaneDispatchStatus
pgy_lane_dispatch(PgyExecutionLane lane, PgyLaneTaskFn fn, void *arg,
                  void **result_out)
{
    if (fn == NULL)
        return PGY_LANE_DISPATCH_INVALID;

    switch (lane)
    {
        case PGY_LANE_REJECT:
            /* Fail closed: an unclassifiable / contradictory task is never run. */
            return PGY_LANE_DISPATCH_REJECTED;

        case PGY_LANE_INLINE:
        case PGY_LANE_PINNED_ZONE:
        {
            void *r = fn(arg);
            if (result_out != NULL)
                *result_out = r;
            return PGY_LANE_DISPATCH_OK;
        }

        case PGY_LANE_WORKER_POOL:
        case PGY_LANE_BLOCKING_POOL:
        case PGY_LANE_LOCAL_ASYNC:
        case PGY_LANE_MOVABLE_SCHEDULER:
            return lane_run_on_worker(fn, arg, result_out);
    }

    return PGY_LANE_DISPATCH_INVALID;
}

const char *
pgy_lane_executor_name(PgyExecutionLane lane)
{
    switch (lane)
    {
        case PGY_LANE_REJECT:            return "(rejected)";
        case PGY_LANE_INLINE:            return "InlineExecutor";
        case PGY_LANE_PINNED_ZONE:       return "PinnedExecutor";
        case PGY_LANE_BLOCKING_POOL:     return "BlockingExecutor";
        case PGY_LANE_LOCAL_ASYNC:       return "LocalCoroutineExecutor";
        case PGY_LANE_WORKER_POOL:       return "WorkerPoolExecutor";
        case PGY_LANE_MOVABLE_SCHEDULER: return "MovableExecutor";
    }
    return "(rejected)";
}
