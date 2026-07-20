/*
 * air_execution_lane.c - derive SEA BoundaryCaptureFact and ExecutionLane for
 * an AIR boundary.
 *
 * Builds BoundaryCaptureFact from facts a boundary already carries (kind,
 * authority, and RIR/MIR evidence). Lane-relevant resource, movability, and
 * value-capture shape comes from boundary-local evidence, not source spelling
 * or routine-level guesses.
 */
#include "air_internal.h"

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
    fact.captures_live_view =
        boundary->has_rir_live_view_capture_evidence;
    fact.captures_raw_slot =
        boundary->has_rir_raw_slot_capture_evidence;
    fact.captures_raw_channel =
        boundary->has_rir_raw_channel_capture_evidence;
    fact.captures_value_only =
        boundary->has_mir_value_capture_evidence;
    fact.captures_pin =
        boundary->has_rir_zone_pin_evidence;

    switch (boundary->kind)
    {
        case AIR_BOUNDARY_ZONE:
            fact.is_concurrent_site = true;
            break;

        case AIR_BOUNDARY_CHANNEL:
            fact.is_concurrent_site = true;
            break;

        case AIR_BOUNDARY_IO:
            fact.is_concurrent_site = true;
            fact.has_io_or_ffi_effect = true;
            break;

        case AIR_BOUNDARY_PARALLEL:
            fact.is_concurrent_site = true;
            /* Declared `spawn blocking` is exactly the OS-blocking effect the
               policy routes to BlockingPool before any movability rule. */
            fact.has_io_or_ffi_effect =
                boundary->has_declared_blocking_evidence;
            fact.is_await_heavy_local =
                boundary->has_rir_await_local_evidence;
            fact.requires_movability =
                boundary->has_rir_movability_requirement_evidence;
            fact.is_deterministic_fork_join =
                boundary->has_rir_deterministic_fork_join_evidence;
            break;

        case AIR_BOUNDARY_WORLD:
            fact.is_concurrent_site = true;
            break;

        case AIR_BOUNDARY_EXECUTION:
            fact.is_concurrent_site = true;
            if (boundary->has_mir_pin_cleanup_evidence) {
                fact.captures_pin = true;
            }
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
