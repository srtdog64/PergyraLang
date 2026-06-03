#ifndef PERGYRA_DOMAIN_FRONTIER_POLICY_H
#define PERGYRA_DOMAIN_FRONTIER_POLICY_H

#include <stddef.h>

#include "../runtime/pgy_frontier_policy.h"

typedef const char *(*PgyDomainWorldZoneTypeNameFn)(void *ctx, size_t index);
typedef size_t (*PgyDomainZoneFrontierMemberCountFn)(
    void *ctx,
    const char *zone_name);

size_t pgy_domain_zone_frontier_pass_limit_from_counts(
    size_t state_count,
    size_t layer_slot_count);
size_t pgy_domain_projection_frontier_pass_limit(size_t refresh_count);
size_t pgy_domain_world_derived_frontier_pass_limit_from_count(
    size_t state_count);
size_t pgy_domain_world_embedded_frontier_count_from_zone_types(
    size_t zone_count,
    PgyDomainWorldZoneTypeNameFn zone_type_name_at,
    void *zone_type_ctx,
    PgyDomainZoneFrontierMemberCountFn zone_member_count,
    void *zone_member_ctx);
size_t pgy_domain_world_transitive_frontier_pass_limit_from_counts(
    size_t zone_count,
    size_t state_count,
    size_t embedded_frontier_count);

#endif /* PERGYRA_DOMAIN_FRONTIER_POLICY_H */
