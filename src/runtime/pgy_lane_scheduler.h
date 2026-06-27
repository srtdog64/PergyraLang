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

#include "../compiler/execution_lane.h"

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

/* The executor name a lane routes to, for diagnostics/tracing. Never NULL. */
const char *pgy_lane_executor_name(PgyExecutionLane lane);

#endif /* PERGYRA_LANE_SCHEDULER_H */
