#include "domain_frontier_policy.h"

size_t
pgy_domain_zone_frontier_pass_limit_from_counts(size_t state_count,
                                                size_t layer_slot_count)
{
    return pgy_frontier_zone_pass_limit(state_count, layer_slot_count);
}

size_t
pgy_domain_projection_frontier_pass_limit(size_t refresh_count)
{
    return pgy_frontier_projection_pass_limit(refresh_count);
}

size_t
pgy_domain_world_derived_frontier_pass_limit_from_count(size_t state_count)
{
    return pgy_frontier_world_derived_pass_limit(state_count);
}

size_t
pgy_domain_world_embedded_frontier_count_from_zone_types(
    size_t zone_count,
    PgyDomainWorldZoneTypeNameFn zone_type_name_at,
    void *zone_type_ctx,
    PgyDomainZoneFrontierMemberCountFn zone_member_count,
    void *zone_member_ctx)
{
    size_t count = 0;

    if (zone_type_name_at == NULL || zone_member_count == NULL)
        return 0;

    for (size_t i = 0; i < zone_count; i++) {
        size_t zone_members;
        const char *zone_type_name = zone_type_name_at(zone_type_ctx, i);

        if (zone_type_name == NULL) {
            continue;
        }

        zone_members = zone_member_count(zone_member_ctx, zone_type_name);
        count = pgy_frontier_pass_limit_add(count, zone_members);
    }

    return count;
}

size_t
pgy_domain_world_transitive_frontier_pass_limit_from_counts(
    size_t zone_count,
    size_t state_count,
    size_t embedded_frontier_count)
{
    return pgy_frontier_world_transitive_pass_limit(
        zone_count,
        state_count,
        embedded_frontier_count);
}
