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
 *   - ExecutionLaneFact  : the AIR/MIR fact below — THIS file. A per-task
 *                          compiler decision derived from evidence.
 *   - PgyLaneScheduler   : the runtime facade that consumes the fact and
 *                          dispatches to a concrete executor.
 *
 * This header owns the fact and the classification POLICY (a pure decision
 * table, §5 policy-centred / §1 fail-closed). It does NOT schedule anything.
 */
#ifndef PERGYRA_EXECUTION_LANE_H
#define PERGYRA_EXECUTION_LANE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The lane a task is assigned to. Ordered from most-constrained (must stay put)
 * to most-movable. REJECT is the fail-closed verdict: the evidence is
 * contradictory (a pinned/raw resource asked to move) and no lane is sound.
 */
typedef enum
{
    PGY_LANE_REJECT = 0,        /* fail-closed: pinned/raw resource cannot move */
    PGY_LANE_INLINE,            /* no concurrency need — run in place */
    PGY_LANE_PINNED_ZONE,       /* has pin/slot/live-view: bound to its owner/zone */
    PGY_LANE_BLOCKING_POOL,     /* IO/FFI/OS blocking: its own lane */
    PGY_LANE_LOCAL_ASYNC,       /* await-heavy + local state: cooperative, same owner */
    PGY_LANE_WORKER_POOL,       /* deterministic fork-join / pure value, bounded pool */
    PGY_LANE_MOVABLE_SCHEDULER  /* the ONLY M:N lane: pure value, no pin/raw,
                                   authority boundary clear — work-stealing eligible */
} PgyExecutionLane;

/*
 * The evidence the classifier reads. Every field is a fact the compiler already
 * computes elsewhere (effect mask, pin/slot presence, capture purity, async).
 * This struct is the seam: AIR/MIR populates it per concurrency site, the policy
 * below maps it to a lane. No field is a heuristic guess — each is a yes/no the
 * IR can answer.
 */
typedef struct
{
    /* Resource evidence — forces PinnedZone, or REJECT if it must move. */
    bool has_pin_or_live_view;          /* pin slot as view / ReadView / WriteView live */
    bool has_raw_slot_or_channel_capture; /* captured a Slot/Channel handle, not a value */

    /* Movability evidence. A site "requires movability" when it is handed to a
       detached/movable executor (escaping spawn) rather than scoped fork-join. */
    bool requires_movability;
    bool capture_is_pure_value;         /* every capture is a copied value (Stage A) */
    bool authority_boundary_clear;      /* the task's capability/authority is explicit */

    /* Effect / shape evidence. */
    bool has_io_or_ffi_effect;          /* effect mask includes IO/FFI/OS-blocking */
    bool is_await_heavy_local;          /* suspends often, touches only local state */
    bool is_deterministic_fork_join;    /* small scoped parallel with joined result */
    bool is_concurrent_site;            /* is this a spawn/async/parallel site at all */
} PgyLaneEvidence;

/*
 * The classification policy: evidence -> lane. Pure, total, deterministic,
 * fail-closed. This is the SEA decision table; the priority ORDER is the
 * contract (a pinned resource is decided before an effect, which is decided
 * before a movability optimisation), so the same evidence always yields the
 * same lane regardless of runtime.
 */
PgyExecutionLane pgy_classify_execution_lane(const PgyLaneEvidence *evidence);

/* Stable name for diagnostics / AIR dump / tests. Never NULL. */
const char *pgy_execution_lane_name(PgyExecutionLane lane);

/*
 * SEA spawn-site convenience: the lane for a spawn whose only distinguishing
 * evidence today is whether it is a blocking call. A blocking spawn is the
 * BlockingPool lane; a non-blocking spawn is cooperative LocalAsync until richer
 * capture/authority evidence promotes it. Both codegen backends route their
 * spawn-executor choice through this so the decision has ONE source (the policy),
 * not an independent `is_blocking ? ... : ...` branch per backend.
 */
PgyExecutionLane pgy_spawn_lane_from_blocking(bool is_blocking);

/* Whether a lane runs on the dedicated blocking executor (vs the async path).
   The single fact the spawn emitters branch on to pick the runtime export. */
bool pgy_lane_uses_blocking_executor(PgyExecutionLane lane);

#endif /* PERGYRA_EXECUTION_LANE_H */
