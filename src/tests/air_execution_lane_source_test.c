/*
 * air_execution_lane_source_test.c - AIR BoundaryCaptureFact evidence proof.
 *
 * AIR_BOUNDARY_PARALLEL is a coarse boundary family. Lane-relevant shape must
 * come from boundary-local RIR/MIR evidence, not from source-name taxonomy or a
 * routine-level resource guess.
 */
#include "air.h"

#include <stdio.h>

static int fails = 0;

static AIRBoundaryNode
boundary(AIRBoundaryKind kind,
         bool has_rir_await_local_evidence,
         bool has_rir_movability_requirement_evidence,
         bool has_rir_deterministic_fork_join_evidence,
         bool has_rir_raw_channel_capture_evidence,
         bool has_rir_raw_slot_capture_evidence,
         bool has_rir_live_view_capture_evidence,
         bool has_mir_pin_cleanup_evidence,
         bool has_mir_value_capture_evidence,
         bool has_rir_zone_pin_evidence,
         bool crosses_authority_boundary)
{
    AIRBoundaryNode b = {0};
    b.kind = kind;
    b.authority_required = crosses_authority_boundary;
    b.authority_name_count = crosses_authority_boundary ? 1 : 0;
    b.has_rir_await_local_evidence = has_rir_await_local_evidence;
    b.has_rir_movability_requirement_evidence =
        has_rir_movability_requirement_evidence;
    b.has_rir_deterministic_fork_join_evidence =
        has_rir_deterministic_fork_join_evidence;
    b.has_rir_raw_channel_capture_evidence =
        has_rir_raw_channel_capture_evidence;
    b.has_rir_raw_slot_capture_evidence =
        has_rir_raw_slot_capture_evidence;
    b.has_rir_live_view_capture_evidence =
        has_rir_live_view_capture_evidence;
    b.has_mir_pin_cleanup_evidence = has_mir_pin_cleanup_evidence;
    b.has_mir_value_capture_evidence = has_mir_value_capture_evidence;
    b.has_rir_zone_pin_evidence = has_rir_zone_pin_evidence;
    return b;
}

static void
chk(const char *name,
    AIRBoundaryKind kind,
    bool has_rir_await_local_evidence,
    bool has_rir_movability_requirement_evidence,
    bool has_rir_deterministic_fork_join_evidence,
    bool has_rir_raw_channel_capture_evidence,
    bool has_rir_raw_slot_capture_evidence,
    bool has_rir_live_view_capture_evidence,
    bool has_mir_pin_cleanup_evidence,
    bool has_mir_value_capture_evidence,
    bool has_rir_zone_pin_evidence,
    bool crosses_authority_boundary,
    bool want_pin,
    bool want_raw_slot,
    bool want_live_view,
    bool want_raw_channel,
    bool want_value_only,
    bool want_crosses_authority,
    bool want_await_local,
    bool want_fork_join,
    bool want_requires_move,
    PgyExecutionLane want_lane)
{
    AIRBoundaryNode b = boundary(kind,
                                 has_rir_await_local_evidence,
                                 has_rir_movability_requirement_evidence,
                                 has_rir_deterministic_fork_join_evidence,
                                 has_rir_raw_channel_capture_evidence,
                                 has_rir_raw_slot_capture_evidence,
                                 has_rir_live_view_capture_evidence,
                                 has_mir_pin_cleanup_evidence,
                                 has_mir_value_capture_evidence,
                                 has_rir_zone_pin_evidence,
                                 crosses_authority_boundary);
    BoundaryCaptureFact fact = air_boundary_capture_fact(&b);
    PgyExecutionLane got_lane = pgy_classify_execution_lane(&fact);

    if (fact.captures_pin != want_pin
        || fact.captures_raw_slot != want_raw_slot
        || fact.captures_live_view != want_live_view
        || fact.captures_raw_channel != want_raw_channel
        || fact.captures_value_only != want_value_only
        || fact.crosses_authority_boundary != want_crosses_authority
        || fact.is_await_heavy_local != want_await_local
        || fact.is_deterministic_fork_join != want_fork_join
        || fact.requires_movability != want_requires_move
        || got_lane != want_lane) {
        printf("FAIL %-18s pin=%d raw_slot=%d live_view=%d raw_channel=%d value=%d auth=%d await=%d fork=%d move=%d lane=%s\n",
               name,
               (int)fact.captures_pin,
               (int)fact.captures_raw_slot,
               (int)fact.captures_live_view,
               (int)fact.captures_raw_channel,
               (int)fact.captures_value_only,
               (int)fact.crosses_authority_boundary,
               (int)fact.is_await_heavy_local,
               (int)fact.is_deterministic_fork_join,
               (int)fact.requires_movability,
               pgy_execution_lane_name(got_lane));
        fails++;
    } else {
        printf("ok   %-18s -> %s\n", name, pgy_execution_lane_name(got_lane));
    }
}

int
main(void)
{
    chk("parallel", AIR_BOUNDARY_PARALLEL,
        false, false, true, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, true, false,
        PGY_LANE_WORKER_POOL);
    chk("parallel-no-rir", AIR_BOUNDARY_PARALLEL,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false,
        PGY_LANE_LOCAL_ASYNC);
    chk("await-local", AIR_BOUNDARY_PARALLEL,
        true, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, true, false, false,
        PGY_LANE_LOCAL_ASYNC);
    chk("spawn-raw-slot", AIR_BOUNDARY_PARALLEL,
        false, true, false, false, true, false, false, false, false, false,
        false, true, false, false, false, false, false, false, true,
        PGY_LANE_REJECT);
    chk("parallel-raw-chan", AIR_BOUNDARY_PARALLEL,
        false, false, false, true, false, false, false, false, false, false,
        false, false, false, true, false, false, false, false, false,
        PGY_LANE_PINNED_ZONE);
    chk("spawn-raw-chan", AIR_BOUNDARY_PARALLEL,
        false, true, false, true, false, false, false, false, false, false,
        false, false, false, true, false, false, false, false, true,
        PGY_LANE_REJECT);
    chk("spawn-value-auth", AIR_BOUNDARY_PARALLEL,
        false, true, false, false, false, false, false, true, false, true,
        false, false, false, false, true, true, false, false, true,
        PGY_LANE_MOVABLE_SCHEDULER);
    chk("channel-rir", AIR_BOUNDARY_CHANNEL,
        false, false, false, true, false, false, false, false, false, false,
        false, false, false, true, false, false, false, false, false,
        PGY_LANE_PINNED_ZONE);
    chk("channel-no-rir", AIR_BOUNDARY_CHANNEL,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false,
        PGY_LANE_LOCAL_ASYNC);
    chk("zone-no-rir-pin", AIR_BOUNDARY_ZONE,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false,
        PGY_LANE_LOCAL_ASYNC);
    chk("zone-rir-pin", AIR_BOUNDARY_ZONE,
        false, false, false, false, false, false, false, false, true, false,
        true, false, false, false, false, false, false, false, false,
        PGY_LANE_PINNED_ZONE);
    chk("pin-no-mir", AIR_BOUNDARY_EXECUTION,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false,
        PGY_LANE_LOCAL_ASYNC);
    chk("pin-mir", AIR_BOUNDARY_EXECUTION,
        false, false, false, false, false, false, true, false, false, false,
        true, false, false, false, false, false, false, false, false,
        PGY_LANE_PINNED_ZONE);

    if (fails) {
        printf("\n%d FAIL\n", fails);
        return 1;
    }
    printf("\nALL PASS (13/13)\n");
    return 0;
}
