/*
 * execution_lane.h — SEA ExecutionLane fact + evidence-based lane classification
 *
 * SEA (Structured Effect Async) is the SEMANTIC model: execution boundaries are
 * decided by intent/effect/authority/coordination EVIDENCE, not by a thread
 * count. M:N is not the centre of the language — it is one runtime lane among
 * several, allowed only when the evidence permits it.
 *
 * Three named layers (do not collapse them):
 *   - SEA                : semantic model / contract (docs/114, docs/146).
 *   - BoundaryCaptureFact: the AIR/MIR input fact below. It records what a
 *                          boundary captures and whether that boundary may move.
 *   - ExecutionLaneFact  : the AIR/MIR output fact below. A per-task compiler
 *                          decision derived from BoundaryCaptureFact.
 *   - PgyLaneScheduler   : the runtime facade that consumes the fact and
 *                          dispatches to a concrete executor.
 *
 * This header owns the facts and the classification POLICY (a pure decision
 * table, §5 policy-centred / §1 fail-closed). It does NOT schedule anything.
 */
#ifndef PERGYRA_EXECUTION_LANE_H
#define PERGYRA_EXECUTION_LANE_H

#include "../common/execution_lane_kind.h"

#include <stdbool.h>
#include <stddef.h>


/*
 * BoundaryCaptureFact is the evidence the classifier reads. Every field is a
 * fact the compiler either owns at the boundary or must fail to prove before it
 * can pick a stronger lane. AIR/MIR populates it per concurrency site, then the
 * policy below maps it to ExecutionLaneFact. No field is a backend heuristic.
 */
typedef struct
{
    /* Resource capture evidence: forces PinnedZone, or REJECT if it must move. */
    bool captures_pin;
    bool captures_live_view;
    bool captures_raw_slot;
    bool captures_raw_channel;

    /* Movability evidence. A site requires movability when it is handed to a
       detached/movable executor rather than scoped fork-join. */
    bool requires_movability;
    bool captures_value_only;
    bool crosses_authority_boundary;

    /* Effect / shape evidence. */
    bool has_io_or_ffi_effect;          /* effect mask includes IO/FFI/OS-blocking */
    bool is_await_heavy_local;          /* suspends often, touches only local state */
    bool is_deterministic_fork_join;    /* small scoped parallel with joined result */
    bool is_concurrent_site;            /* is this a spawn/async/parallel site at all */
} BoundaryCaptureFact;

/*
 * The classification policy: BoundaryCaptureFact -> lane. Pure, total, deterministic,
 * fail-closed. This is the SEA decision table; the priority ORDER is the
 * contract (a pinned resource is decided before an effect, which is decided
 * before a movability optimisation), so the same evidence always yields the
 * same lane regardless of runtime.
 */
PgyExecutionLane pgy_classify_execution_lane(const BoundaryCaptureFact *fact);

/* Stable name for diagnostics / AIR dump / tests. Never NULL. */
const char *pgy_execution_lane_name(PgyExecutionLane lane);

/*
 * Spawn sites carry no shortcut here anymore: the declared `spawn blocking`
 * marker enters BoundaryCaptureFact as IO/blocking effect evidence during AIR
 * boundary discovery, the classifier above decides the lane, and the verified
 * spawn-lane plan (verified_projection_plan.h) carries the per-site fact into
 * both backends. Deriving a lane from source spelling in an emitter is the
 * drift that plan exists to remove.
 */

#endif /* PERGYRA_EXECUTION_LANE_H */
