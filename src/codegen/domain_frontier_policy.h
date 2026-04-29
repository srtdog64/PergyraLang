#ifndef PERGYRA_DOMAIN_FRONTIER_POLICY_H
#define PERGYRA_DOMAIN_FRONTIER_POLICY_H

#include <stdint.h>
#include <stddef.h>

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
pgy_frontier_world_transitive_pass_limit(size_t zone_count, size_t state_count)
{
    /*
     * A world frontier pass may sync dirty embedded zones and then recompute
     * derived world states.  The frontier is monotone over the stable beta
     * boolean state family, so each zone/state can force at most one new
     * observed frontier change before convergence.
     */
    return pgy_frontier_world_pass_limit(zone_count, state_count);
}

static inline size_t
pgy_frontier_world_derived_pass_limit(size_t state_count)
{
    return pgy_frontier_pass_limit_add_one(state_count);
}

#endif /* PERGYRA_DOMAIN_FRONTIER_POLICY_H */
