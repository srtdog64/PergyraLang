#ifndef PERGYRA_DOMAIN_FRONTIER_POLICY_H
#define PERGYRA_DOMAIN_FRONTIER_POLICY_H

#include <stddef.h>

static inline size_t
pgy_frontier_projection_pass_limit(size_t refresh_count)
{
    return refresh_count + 1;
}

static inline size_t
pgy_frontier_zone_pass_limit(size_t state_count, size_t layer_slot_count)
{
    return state_count + layer_slot_count + 1;
}

static inline size_t
pgy_frontier_world_pass_limit(size_t zone_count, size_t state_count)
{
    return zone_count + state_count + 1;
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
    return state_count + 1;
}

#endif /* PERGYRA_DOMAIN_FRONTIER_POLICY_H */
