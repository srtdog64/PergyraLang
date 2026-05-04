/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend domain-specific pass orchestration.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include "llvm_domain_event.h"
#include "llvm_domain_forward.h"
#include "llvm_domain_method_emit.h"
#include "llvm_domain_role_emit.h"

void
llvm_emit_domain_passes(LLVMGenCtx *ctx)
{
    LLVMDomainInventory inventory;
    ASTNode **abilities;
    ASTNode **relations;
    ASTNode **effects;
    ASTNode **zones;
    ASTNode **worlds;
    ASTNode **parties;
    ASTNode **rosters;
    ASTNode **roles;
    ASTNode **events;
    size_t ability_count;
    size_t relation_count;
    size_t effect_count;
    size_t zone_count;
    size_t world_count;
    size_t party_count;
    size_t roster_count;
    size_t role_count;
    size_t event_count;

    if (ctx == NULL)
        return;

    llvm_active_domain_inventory(ctx, &inventory);
    abilities = inventory.abilities;
    relations = inventory.relations;
    effects = inventory.effects;
    zones = inventory.zones;
    worlds = inventory.worlds;
    parties = inventory.parties;
    rosters = inventory.rosters;
    roles = inventory.roles;
    events = inventory.events;
    ability_count = inventory.ability_count;
    relation_count = inventory.relation_count;
    effect_count = inventory.effect_count;
    zone_count = inventory.zone_count;
    world_count = inventory.world_count;
    party_count = inventory.party_count;
    roster_count = inventory.roster_count;
    role_count = inventory.role_count;
    event_count = inventory.event_count;

    ASTNode **domain_groups[] = {
        relations,
        effects,
        zones,
        worlds,
        parties,
        rosters,
    };
    size_t domain_group_counts[] = {
        relation_count,
        effect_count,
        zone_count,
        world_count,
        party_count,
        roster_count,
    };
    size_t domain_group_count =
        sizeof(domain_groups) / sizeof(domain_groups[0]);

    llvm_register_domain_structs(ctx, domain_groups, domain_group_counts,
                                 domain_group_count);

    llvm_emit_domain_ability_vtables(ctx, abilities, ability_count);
    llvm_emit_domain_role_forward_decls(ctx, roles, role_count);
    llvm_emit_domain_event_helpers(ctx, events, event_count);

    if (!llvm_emit_domain_role_method_bodies(ctx, roles, role_count))
        return;

    if (!llvm_emit_domain_sync_and_method_bodies(ctx, domain_groups,
            domain_group_counts, domain_group_count))
        return;
}

#endif /* PGY_LLVM_ENABLED */
