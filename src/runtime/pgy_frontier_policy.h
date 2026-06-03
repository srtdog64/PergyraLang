#ifndef PERGYRA_RUNTIME_FRONTIER_POLICY_H
#define PERGYRA_RUNTIME_FRONTIER_POLICY_H

#include <stddef.h>
#include <stdint.h>

#define PGY_FRONTIER_POLICY_SCHEMA "pgy.runtime.frontier-policy.v1"
#define PGY_FRONTIER_POLICY_SUBJECT "bounded-frontier-pass-limit"
#define PGY_FRONTIER_PASS_LIMIT_FACT_COUNT 10u
#define PGY_FRONTIER_OVERFLOW_REASON_FACT_COUNT 5u
#define PGY_FRONTIER_POLICY_FACT_COUNT \
    (PGY_FRONTIER_PASS_LIMIT_FACT_COUNT + PGY_FRONTIER_OVERFLOW_REASON_FACT_COUNT)
#define PGY_FRONTIER_REASON_GENERIC_OVERFLOW \
    "frontier recompute exceeded bounded pass limit"
#define PGY_FRONTIER_REASON_PROJECTION_OVERFLOW \
    "projection recompute exceeded bounded pass limit"
#define PGY_FRONTIER_REASON_ZONE_OVERFLOW \
    "zone frontier recompute exceeded bounded pass limit"
#define PGY_FRONTIER_REASON_WORLD_TRANSITIVE_OVERFLOW \
    "world frontier recompute exceeded bounded pass limit"
#define PGY_FRONTIER_REASON_WORLD_DERIVED_OVERFLOW \
    "world derived recompute exceeded bounded pass limit"

typedef enum PgyFrontierPublishPhase {
    PGY_FRONTIER_PUBLISH_WRITE_VALUE = 0,
    PGY_FRONTIER_PUBLISH_WRITE_EPOCH = 1,
    PGY_FRONTIER_PUBLISH_WRITE_CAUSE = 2,
    PGY_FRONTIER_PUBLISH_READY = 3,
    PGY_FRONTIER_PUBLISH_CLEAR_DIRTY = 4,
} PgyFrontierPublishPhase;

static inline int
pgy_frontier_publish_phase_order(PgyFrontierPublishPhase phase)
{
    switch (phase) {
    case PGY_FRONTIER_PUBLISH_WRITE_VALUE:
        return 0;
    case PGY_FRONTIER_PUBLISH_WRITE_EPOCH:
        return 1;
    case PGY_FRONTIER_PUBLISH_WRITE_CAUSE:
        return 2;
    case PGY_FRONTIER_PUBLISH_READY:
        return 3;
    case PGY_FRONTIER_PUBLISH_CLEAR_DIRTY:
        return 4;
    }
    return -1;
}

static inline int
pgy_frontier_publish_order_is_valid(PgyFrontierPublishPhase before,
                                    PgyFrontierPublishPhase after)
{
    int before_order = pgy_frontier_publish_phase_order(before);
    int after_order = pgy_frontier_publish_phase_order(after);
    return before_order >= 0 && after_order >= 0
        && before_order <= after_order;
}

static inline size_t
pgy_frontier_pass_limit_cap(void)
{
    return (size_t)UINT32_MAX;
}

static inline size_t
pgy_frontier_pass_limit_clamp(size_t value)
{
    size_t cap = pgy_frontier_pass_limit_cap();
    return value > cap ? cap : value;
}

static inline size_t
pgy_frontier_pass_limit_add(size_t lhs, size_t rhs)
{
    size_t cap = pgy_frontier_pass_limit_cap();
    if (lhs >= cap)
        return cap;
    if (rhs > cap - lhs)
        return cap;
    return lhs + rhs;
}

static inline size_t
pgy_frontier_pass_limit_add_one(size_t value)
{
    return pgy_frontier_pass_limit_add(value, 1);
}

static inline size_t
pgy_frontier_projection_pass_limit(size_t refresh_count)
{
    return pgy_frontier_pass_limit_add_one(refresh_count);
}

static inline size_t
pgy_frontier_zone_pass_limit(size_t state_count, size_t layer_slot_count)
{
    return pgy_frontier_pass_limit_add_one(
        pgy_frontier_pass_limit_add(state_count, layer_slot_count));
}

static inline size_t
pgy_frontier_world_pass_limit(size_t zone_count, size_t state_count)
{
    return pgy_frontier_pass_limit_add_one(
        pgy_frontier_pass_limit_add(zone_count, state_count));
}

static inline size_t
pgy_frontier_embedded_zone_member_count(size_t state_count,
                                        size_t layer_slot_count)
{
    return pgy_frontier_pass_limit_add(state_count, layer_slot_count);
}

static inline size_t
pgy_frontier_world_transitive_pass_limit(size_t zone_count, size_t state_count,
                                         size_t embedded_zone_frontier_count)
{
    /*
     * A world frontier pass may sync dirty embedded zones and then recompute
     * derived world states. The frontier is monotone over the stable beta
     * boolean state family, so each world zone/state and each embedded zone
     * frontier member can force at most one new observed frontier change
     * before convergence.
     */
    return pgy_frontier_pass_limit_add_one(
        pgy_frontier_pass_limit_add(
            pgy_frontier_pass_limit_add(zone_count, state_count),
            embedded_zone_frontier_count));
}

static inline size_t
pgy_frontier_world_derived_pass_limit(size_t state_count)
{
    return pgy_frontier_pass_limit_add_one(state_count);
}

#endif /* PERGYRA_RUNTIME_FRONTIER_POLICY_H */
