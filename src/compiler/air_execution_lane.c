/*
 * air_execution_lane.c — derive the SEA ExecutionLane for an AIR boundary.
 *
 * First-cut: builds PgyLaneEvidence from the facts a boundary already carries
 * (kind, authority). The richer evidence — pin/live-view, raw-vs-value capture,
 * effect mask — lives in MIR/semantic and is not yet threaded onto the boundary;
 * until it is, this stays conservative and NEVER reaches MovableScheduler (M:N),
 * because that lane requires explicit pure-value + authority evidence that a
 * boundary kind alone cannot supply. See docs/146 §5.
 */
#include "air.h"

PgyExecutionLane
air_boundary_classify_lane(const AIRBoundaryNode *boundary)
{
    PgyLaneEvidence e = {0};

    if (boundary == NULL)
        return PGY_LANE_INLINE;

    /* authority_required + named participants => the authority boundary is
       explicit. (Necessary-but-not-sufficient for MovableScheduler, which also
       needs pure-value capture evidence not yet plumbed here.) */
    e.authority_boundary_clear =
        boundary->authority_required && boundary->authority_name_count > 0;

    switch (boundary->kind)
    {
        case AIR_BOUNDARY_ZONE:
            /* A zone is a resource-ownership boundary; a task that needs the
               zone's resources is bound to it. */
            e.is_concurrent_site = true;
            e.has_pin_or_live_view = true;
            break;

        case AIR_BOUNDARY_CHANNEL:
            /* A channel boundary holds a channel handle — a raw resource, so the
               task stays pinned to its owner rather than migrating. */
            e.is_concurrent_site = true;
            e.has_raw_slot_or_channel_capture = true;
            break;

        case AIR_BOUNDARY_IO:
            e.is_concurrent_site = true;
            e.has_io_or_ffi_effect = true;
            break;

        case AIR_BOUNDARY_PARALLEL:
            /* `parallel` is scoped fork-join: deterministic, bounded pool. */
            e.is_concurrent_site = true;
            e.is_deterministic_fork_join = true;
            break;

        case AIR_BOUNDARY_WORLD:
        case AIR_BOUNDARY_EXECUTION:
            /* A concurrent site whose capture/effect shape is not yet plumbed:
               keep it local and cooperative rather than guessing a stronger
               lane. */
            e.is_concurrent_site = true;
            break;

        case AIR_BOUNDARY_UNKNOWN:
        default:
            /* No boundary evidence => treat as non-concurrent (run in place)
               rather than inventing a lane. */
            e.is_concurrent_site = false;
            break;
    }

    return pgy_classify_execution_lane(&e);
}
