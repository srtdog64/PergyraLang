/*
 * air_execution_lane.c - derive SEA BoundaryCaptureFact and ExecutionLane for
 * an AIR boundary.
 *
 * First-cut: builds BoundaryCaptureFact from facts a boundary already carries
 * (kind, authority). Richer evidence, such as precise raw-vs-value closure
 * capture and pin/live-view provenance, lives in MIR/semantic and is not yet
 * fully threaded onto the boundary. Until it is, this stays conservative and
 * never reaches MovableScheduler from boundary kind alone.
 */
#include "air.h"

BoundaryCaptureFact
air_boundary_capture_fact(const AIRBoundaryNode *boundary)
{
    BoundaryCaptureFact fact = {0};

    if (boundary == NULL)
        return fact;

    /* Explicit authority participants are necessary, but not sufficient, for a
       movable lane. Pure-value capture evidence must also be present. */
    fact.crosses_authority_boundary =
        boundary->authority_required && boundary->authority_name_count > 0;

    switch (boundary->kind)
    {
        case AIR_BOUNDARY_ZONE:
            fact.is_concurrent_site = true;
            fact.captures_pin = true;
            break;

        case AIR_BOUNDARY_CHANNEL:
            fact.is_concurrent_site = true;
            fact.captures_raw_channel = true;
            break;

        case AIR_BOUNDARY_IO:
            fact.is_concurrent_site = true;
            fact.has_io_or_ffi_effect = true;
            break;

        case AIR_BOUNDARY_PARALLEL:
            fact.is_concurrent_site = true;
            fact.is_deterministic_fork_join = true;
            break;

        case AIR_BOUNDARY_WORLD:
        case AIR_BOUNDARY_EXECUTION:
            fact.is_concurrent_site = true;
            break;

        case AIR_BOUNDARY_UNKNOWN:
        default:
            fact.is_concurrent_site = false;
            break;
    }

    return fact;
}

PgyExecutionLane
air_boundary_classify_lane(const AIRBoundaryNode *boundary)
{
    BoundaryCaptureFact fact = air_boundary_capture_fact(boundary);
    return pgy_classify_execution_lane(&fact);
}
