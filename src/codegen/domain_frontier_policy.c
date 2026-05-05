#include "domain_frontier_policy.h"

#include "../runtime/pgy_frontier_policy.h"

size_t
pgy_domain_zone_frontier_pass_limit(ASTNode *zone_decl)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return pgy_frontier_zone_pass_limit(0, 0);

    return pgy_frontier_zone_pass_limit(
        zone_decl->data.zone_decl.state_count,
        zone_decl->data.zone_decl.layer_slot_count);
}

size_t
pgy_domain_projection_frontier_pass_limit(size_t refresh_count)
{
    return pgy_frontier_projection_pass_limit(refresh_count);
}

size_t
pgy_domain_world_derived_frontier_pass_limit(ASTNode *world_decl)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return pgy_frontier_world_derived_pass_limit(0);

    return pgy_frontier_world_derived_pass_limit(
        world_decl->data.world_decl.state_count);
}

size_t
pgy_domain_world_embedded_frontier_count(ASTNode *world_decl,
                                         PgyDomainZoneLookupFn lookup_zone,
                                         void *lookup_ctx)
{
    size_t count = 0;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || lookup_zone == NULL) {
        return 0;
    }

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone_slot = world_decl->data.world_decl.zones[i];
        ASTNode *zone_decl;
        size_t zone_members;

        if (zone_slot == NULL || zone_slot->type != AST_WORLD_ZONE
            || zone_slot->data.world_zone.zone_type == NULL) {
            continue;
        }

        zone_decl = lookup_zone(lookup_ctx, zone_slot->data.world_zone.zone_type);
        if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
            continue;

        zone_members = pgy_frontier_pass_limit_add(
            zone_decl->data.zone_decl.state_count,
            zone_decl->data.zone_decl.layer_slot_count);
        count = pgy_frontier_pass_limit_add(count, zone_members);
    }

    return count;
}

size_t
pgy_domain_world_transitive_frontier_pass_limit(ASTNode *world_decl,
                                                size_t embedded_frontier_count)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL) {
        return pgy_frontier_world_transitive_pass_limit(
            0, 0, embedded_frontier_count);
    }

    return pgy_frontier_world_transitive_pass_limit(
        world_decl->data.world_decl.zone_count,
        world_decl->data.world_decl.state_count,
        embedded_frontier_count);
}
