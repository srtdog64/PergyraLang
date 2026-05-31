#ifndef PERGYRA_DOMAIN_FRONTIER_POLICY_H
#define PERGYRA_DOMAIN_FRONTIER_POLICY_H

#include "../parser/ast.h"
#include "../runtime/pgy_frontier_policy.h"

typedef ASTNode *(*PgyDomainZoneLookupFn)(void *ctx, const char *zone_name);
typedef const char *(*PgyDomainWorldZoneTypeNameFn)(void *ctx, size_t index);

size_t pgy_domain_zone_frontier_pass_limit_from_counts(
    size_t state_count,
    size_t layer_slot_count);
size_t pgy_domain_zone_frontier_pass_limit(ASTNode *zone_decl);
size_t pgy_domain_projection_frontier_pass_limit(size_t refresh_count);
size_t pgy_domain_world_derived_frontier_pass_limit_from_count(
    size_t state_count);
size_t pgy_domain_world_derived_frontier_pass_limit(ASTNode *world_decl);
size_t pgy_domain_world_embedded_frontier_count(ASTNode *world_decl,
                                                PgyDomainZoneLookupFn lookup_zone,
                                                void *lookup_ctx);
size_t pgy_domain_world_embedded_frontier_count_from_zone_types(
    size_t zone_count,
    PgyDomainWorldZoneTypeNameFn zone_type_name_at,
    void *zone_type_ctx,
    PgyDomainZoneLookupFn lookup_zone,
    void *lookup_ctx);
size_t pgy_domain_world_transitive_frontier_pass_limit_from_counts(
    size_t zone_count,
    size_t state_count,
    size_t embedded_frontier_count);
size_t pgy_domain_world_transitive_frontier_pass_limit(
    ASTNode *world_decl,
    size_t embedded_frontier_count);

#endif /* PERGYRA_DOMAIN_FRONTIER_POLICY_H */
