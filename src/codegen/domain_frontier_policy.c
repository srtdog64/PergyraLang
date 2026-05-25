#include "domain_frontier_policy.h"
#include "../parser/ast_domain_api.h"

size_t
pgy_domain_zone_frontier_pass_limit(ASTNode *zone_decl)
{
    size_t state_count = 0;
    size_t layer_slot_count = 0;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return pgy_frontier_zone_pass_limit(0, 0);

    (void) ast_zone_states(zone_decl, &state_count);
    (void) ast_zone_layer_slots(zone_decl, &layer_slot_count);
    return pgy_frontier_zone_pass_limit(state_count, layer_slot_count);
}

size_t
pgy_domain_projection_frontier_pass_limit(size_t refresh_count)
{
    return pgy_frontier_projection_pass_limit(refresh_count);
}

size_t
pgy_domain_world_derived_frontier_pass_limit(ASTNode *world_decl)
{
    size_t state_count = 0;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return pgy_frontier_world_derived_pass_limit(0);

    (void) ast_world_states(world_decl, &state_count);
    return pgy_frontier_world_derived_pass_limit(state_count);
}

size_t
pgy_domain_world_embedded_frontier_count(ASTNode *world_decl,
                                         PgyDomainZoneLookupFn lookup_zone,
                                         void *lookup_ctx)
{
    size_t count = 0;
    size_t zone_count = 0;
    ASTNode **zones;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || lookup_zone == NULL) {
        return 0;
    }

    zones = ast_world_zones(world_decl, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone_slot = zones[i];
        ASTNode *zone_decl;
        size_t zone_members;
        size_t state_count = 0;
        size_t layer_slot_count = 0;

        const char *zone_type_name = ast_world_zone_type_name(zone_slot);

        if (zone_type_name == NULL) {
            continue;
        }

        zone_decl = lookup_zone(lookup_ctx, zone_type_name);
        if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
            continue;

        (void) ast_zone_states(zone_decl, &state_count);
        (void) ast_zone_layer_slots(zone_decl, &layer_slot_count);
        zone_members = pgy_frontier_pass_limit_add(state_count, layer_slot_count);
        count = pgy_frontier_pass_limit_add(count, zone_members);
    }

    return count;
}

size_t
pgy_domain_world_transitive_frontier_pass_limit(ASTNode *world_decl,
                                                size_t embedded_frontier_count)
{
    size_t zone_count = 0;
    size_t state_count = 0;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL) {
        return pgy_frontier_world_transitive_pass_limit(
            0, 0, embedded_frontier_count);
    }

    (void) ast_world_zones(world_decl, &zone_count);
    (void) ast_world_states(world_decl, &state_count);
    return pgy_frontier_world_transitive_pass_limit(
        zone_count,
        state_count,
        embedded_frontier_count);
}
