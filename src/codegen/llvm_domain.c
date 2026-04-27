/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend ??domain-specific passes (Party/Roster/World, Ability,
 * Role, Event).  Extracted from llvm_backend.c to keep file sizes
 * manageable.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include "llvm_domain_role_helpers.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
#include "llvm_domain_projection_sync_helpers.h"

static LLVMTypeRef
llvm_zone_effect_pool_struct_type(LLVMGenCtx *ctx, LLVMTypeRef effect_ty, int capacity)
{
    LLVMTypeRef fields[4];
    LLVMTypeRef i8_ty;
    unsigned cap;

    if (ctx == NULL || effect_ty == NULL)
        return NULL;

    if (capacity <= 0)
        capacity = 1;
    cap = (unsigned)capacity;
    i8_ty = LLVMInt8TypeInContext(ctx->context);

    fields[0] = LLVMArrayType(effect_ty, cap);
    fields[1] = LLVMArrayType(ctx->type_i1, cap);
    fields[2] = i8_ty;
    fields[3] = i8_ty;
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

/* Zone sync emission lives in llvm_domain_zone_sync.c. */

/* World sync emission lives in llvm_domain_world_sync.c. */

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

    /* Pass 0a: Register domain struct types + methods */
    for (size_t group = 0;
         group < sizeof(domain_groups) / sizeof(domain_groups[0]);
         group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
            if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **slots = NULL;
        size_t slot_count = 0;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;
        ASTNode **refreshes = NULL;
        size_t refresh_count = 0;

        llvm_domain_decl_parts(stmt, &decl_name, &slots, &slot_count,
            &shared_fields, &shared_count, &methods, &method_count,
            &refreshes, &refresh_count);
        if (decl_name == NULL) {
            continue;
        }

        /* Count dyn role slots (for vtable pointer fields) */
        size_t dyn_slot_count = 0;
        ASTNode **role_slots = NULL;
        size_t role_count = 0;
        if (stmt->type == AST_PARTY_DECL) {
            role_slots = stmt->data.party_decl.role_slots;
            role_count = stmt->data.party_decl.role_count;
        }
        for (size_t j = 0; j < role_count; j++) {
            if (role_slots[j] != NULL
                && role_slots[j]->type == AST_ROLE_SLOT
                && role_slots[j]->data.role_slot.is_dynamic)
                dyn_slot_count++;
        }

        size_t fc = 0;
        LLVMTypeRef *ftypes = NULL;
        if (stmt->type == AST_ZONE_DECL) {
            size_t projection_count =
                llvm_count_domain_projection_slots(stmt->data.zone_decl.slots,
                    stmt->data.zone_decl.slot_count,
                    stmt->data.zone_decl.refreshes,
                    stmt->data.zone_decl.refresh_count);
            fc = stmt->data.zone_decl.slot_count
                + stmt->data.zone_decl.shared_count
                + stmt->data.zone_decl.layer_slot_count
                + stmt->data.zone_decl.layer_slot_count
                + (stmt->data.zone_decl.layer_slot_count * 2)
                + stmt->data.zone_decl.state_count
                + (stmt->data.zone_decl.state_count * 2)
                + (projection_count * 4)
                + 1;
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, idx++) {
                ASTNode *slot = stmt->data.zone_decl.slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[idx] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, idx++) {
                ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                LLVMClassTypeEntry *layer_cls = NULL;
                if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
                    && slot->data.zone_layer_slot.layer_type != NULL) {
                    layer_cls = llvm_lookup_class(ctx,
                        slot->data.zone_layer_slot.layer_type);
                }
                if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
                    && slot->data.zone_layer_slot.is_pool
                    && layer_cls != NULL) {
                    ftypes[idx] = llvm_zone_effect_pool_struct_type(ctx,
                        layer_cls->struct_type,
                        slot->data.zone_layer_slot.pool_capacity);
                } else {
                    ftypes[idx] = layer_cls != NULL ? layer_cls->struct_type : ctx->type_i8ptr;
                }
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, idx++) {
                ftypes[idx] = ctx->type_i1;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count * 2; j++, idx++) {
                ftypes[idx] = ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, idx++) {
                ftypes[idx] = ctx->type_i1;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.state_count * 2; j++, idx++) {
                ftypes[idx] = ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++) {
                ASTNode *slot = stmt->data.zone_decl.slots[j];
                if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                    || (!slot->data.domain_slot.is_tobject
                        && !llvm_domain_slot_is_projection_target(slot,
                            stmt->data.zone_decl.refreshes,
                            stmt->data.zone_decl.refresh_count))) {
                    continue;
                }
                ftypes[idx++] = ctx->type_i1;
                ftypes[idx++] = ctx->type_i1;
                ftypes[idx++] = ctx->type_i32;
                ftypes[idx++] = ctx->type_i32;
            }
            ftypes[idx++] = ctx->type_i32;
        } else if (stmt->type == AST_ROSTER_DECL) {
            fc = stmt->data.roster_decl.party_count
                + stmt->data.roster_decl.shared_count;
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.roster_decl.party_count; j++, idx++) {
                ASTNode *slot = stmt->data.roster_decl.party_slots[j];
                LLVMClassTypeEntry *field_cls = NULL;
                if (slot != NULL && slot->type == AST_SYSTEMIC_SLOT
                    && slot->data.roster_slot.party_type != NULL) {
                    field_cls = llvm_lookup_class(ctx,
                        slot->data.roster_slot.party_type);
                }
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.roster_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.roster_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
        } else if (stmt->type == AST_WORLD_DECL) {
            fc = stmt->data.world_decl.roster_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.shared_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.zone_count
                + stmt->data.world_decl.state_count
                + (stmt->data.world_decl.zone_count * 2)
                + (stmt->data.world_decl.state_count * 2)
                + 1;
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < stmt->data.world_decl.roster_count; j++, idx++) {
                ASTNode *ws = stmt->data.world_decl.rosters[j];
                LLVMClassTypeEntry *field_cls = ws != NULL
                    ? llvm_lookup_class(ctx, ws->data.world_roster.roster_type) : NULL;
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++) {
                ASTNode *wz = stmt->data.world_decl.zones[j];
                LLVMClassTypeEntry *field_cls = wz != NULL
                    ? llvm_lookup_class(ctx, wz->data.world_zone.zone_type) : NULL;
                ftypes[idx] = field_cls != NULL ? field_cls->struct_type : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, idx++) {
                ASTNode *sf = stmt->data.world_decl.shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type) : ctx->type_i32;
            }
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++)
                ftypes[idx] = ctx->type_i32;
            for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, idx++)
                ftypes[idx] = ctx->type_i1;
            for (size_t j = 0; j < stmt->data.world_decl.zone_count * 2; j++, idx++)
                ftypes[idx] = ctx->type_i32;
            for (size_t j = 0; j < stmt->data.world_decl.state_count * 2; j++, idx++)
                ftypes[idx] = ctx->type_i32;
            ftypes[idx] = ctx->type_i1;
        } else {
            /* Build struct: { slots..., shared_fields..., vtable_ptrs... } */
            size_t projection_count =
                (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                ? llvm_count_domain_projection_slots(slots, slot_count,
                    refreshes, refresh_count)
                : 0;
            fc = slot_count + shared_count + dyn_slot_count + projection_count;
            if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                fc = slot_count + shared_count + dyn_slot_count + (projection_count * 4);
            ftypes = pgy_arena_calloc(&ctx->scratch,
                (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
            size_t idx = 0;
            for (size_t j = 0; j < slot_count; j++, idx++) {
                ASTNode *slot = slots[j];
                ASTNode *slot_type = slot->data.domain_slot.type;
                ftypes[idx] = (slot_type != NULL)
                    ? ast_type_to_llvm(ctx, slot_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < shared_count; j++, idx++) {
                ASTNode *sf = shared_fields[j];
                ASTNode *sf_type = sf->data.party_shared.type;
                ftypes[idx] = (sf_type != NULL)
                    ? ast_type_to_llvm(ctx, sf_type)
                    : ctx->type_i32;
            }
            for (size_t j = 0; j < dyn_slot_count; j++, idx++)
                ftypes[idx] = ctx->type_i8ptr;
            if (projection_count > 0) {
                for (size_t j = 0; j < slot_count; j++) {
                    ASTNode *slot = slots[j];
                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || (!slot->data.domain_slot.is_tobject
                            && !llvm_domain_slot_is_projection_target(slot,
                                refreshes, refresh_count))) {
                        continue;
                    }
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i32;
                    ftypes[idx++] = ctx->type_i32;
                }
            }
        }

        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                        decl_name);
        LLVMStructSetBody(struct_ty, ftypes,
                           (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            decl_name, struct_ty, false, true);
        if (entry != NULL) {
            if (stmt->type == AST_ZONE_DECL) {
                entry->domain_kind = LLVM_DOMAIN_ZONE;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.slots[j];
                    llvm_class_add_field_ex(entry, slot->data.domain_slot.slot_name,
                        ftypes[field_index], field_index,
                        slot->data.domain_slot.is_subject);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    llvm_class_add_field(entry, slot->data.zone_layer_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_active_%s",
                        slot->data.zone_layer_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_epoch_%s",
                        slot->data.zone_layer_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__layer_cause_%s",
                        slot->data.zone_layer_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_epoch_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.zone_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__state_cause_%s",
                        state->data.zone_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++) {
                    ASTNode *slot = stmt->data.zone_decl.slots[j];
                    char field_name[256];
                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || (!slot->data.domain_slot.is_tobject
                            && !llvm_domain_slot_is_projection_target(slot,
                                stmt->data.zone_decl.refreshes,
                                stmt->data.zone_decl.refresh_count))
                        || slot->data.domain_slot.slot_name == NULL) {
                        continue;
                    }
                    snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_epoch_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                    snprintf(field_name, sizeof(field_name), "__projection_cause_%s",
                        slot->data.domain_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                    field_index++;
                }
                llvm_class_add_field(entry, pergyra_strdup("__sync_generation"),
                    ftypes[field_index], field_index);
            } else if (stmt->type == AST_ROSTER_DECL) {
                entry->domain_kind = LLVM_DOMAIN_SYSTEMIC;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.roster_decl.party_count; j++, field_index++) {
                    ASTNode *slot = stmt->data.roster_decl.party_slots[j];
                    llvm_class_add_field(entry, slot->data.roster_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.roster_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.roster_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
            } else if (stmt->type == AST_WORLD_DECL) {
                entry->domain_kind = LLVM_DOMAIN_WORLD;
                int field_index = 0;
                for (size_t j = 0; j < stmt->data.world_decl.roster_count; j++, field_index++) {
                    ASTNode *ws = stmt->data.world_decl.rosters[j];
                    llvm_class_add_field(entry, ws->data.world_roster.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    llvm_class_add_field(entry, wz->data.world_zone.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, field_index++) {
                    ASTNode *sf = stmt->data.world_decl.shared_fields[j];
                    llvm_class_add_field(entry, sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_active_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_dirty_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_seen_generation_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_epoch_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_cause_%s",
                        wz->data.world_zone.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_epoch_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
                    ASTNode *state = stmt->data.world_decl.states[j];
                    char field_name[256];
                    snprintf(field_name, sizeof(field_name), "__zone_state_cause_%s",
                        state->data.world_state.state_name);
                    llvm_class_add_field(entry, pergyra_strdup(field_name),
                        ftypes[field_index], field_index);
                }
                llvm_class_add_field(entry, pergyra_strdup("__world_derived_dirty"),
                    ftypes[field_index], field_index);
            } else {
                if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                    entry->domain_kind = LLVM_DOMAIN_PROJECTION;
                int field_index = 0;
                for (size_t j = 0; j < slot_count; j++, field_index++) {
                    ASTNode *slot = slots[j];
                    llvm_class_add_field(entry,
                        slot->data.domain_slot.slot_name,
                        ftypes[field_index], field_index);
                }
                for (size_t j = 0; j < shared_count; j++, field_index++) {
                    ASTNode *sf = shared_fields[j];
                    llvm_class_add_field(entry,
                        sf->data.party_shared.name,
                        ftypes[field_index], field_index);
                }
                size_t dyn_idx = 0;
                for (size_t j = 0; j < role_count; j++) {
                    ASTNode *rs = role_slots[j];
                    if (rs == NULL || rs->type != AST_ROLE_SLOT
                        || !rs->data.role_slot.is_dynamic)
                        continue;
                    char vt_field_buf[256];
                    snprintf(vt_field_buf, sizeof(vt_field_buf), "%s_vtable",
                             rs->data.role_slot.slot_name);
                    llvm_class_add_field(entry, pergyra_strdup(vt_field_buf),
                        ctx->type_i8ptr,
                        field_index);
                    dyn_idx++;
                    field_index++;
                }
                if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL) {
                    for (size_t j = 0; j < slot_count; j++) {
                        ASTNode *slot = slots[j];
                        char field_name[256];
                        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                            || (!slot->data.domain_slot.is_tobject
                                && !llvm_domain_slot_is_projection_target(slot,
                                    refreshes, refresh_count))
                            || slot->data.domain_slot.slot_name == NULL) {
                            continue;
                        }
                        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_epoch_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                        snprintf(field_name, sizeof(field_name), "__projection_cause_%s",
                            slot->data.domain_slot.slot_name);
                        llvm_class_add_field(entry, pergyra_strdup(field_name),
                            ftypes[field_index], field_index);
                        field_index++;
                    }
                }
            }
        }
        /* ftypes is ctx->scratch-owned. */

        if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL
            || stmt->type == AST_ZONE_DECL || stmt->type == AST_WORLD_DECL) {
            char sync_name[256];
            LLVMTypeRef sync_params[] = { LLVMPointerType(struct_ty, 0) };
            LLVMTypeRef sync_ft = LLVMFunctionType(ctx->type_void, sync_params, 1, 0);
            LLVMValueRef sync_fn;
            snprintf(sync_name, sizeof(sync_name), "%s_sync", decl_name);
            sync_fn = LLVMAddFunction(ctx->module, sync_name, sync_ft);
            llvm_register_function(ctx, LLVMGetValueName(sync_fn),
                sync_fn, sync_ft, ctx->type_void);
            if (entry != NULL)
                entry->sync_function_name = pergyra_strdup(sync_name);
        }

        /* Forward-declare methods */
        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            const char *mname = method->data.func_decl.name;
            size_t pc = method->data.func_decl.param_count;

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                user_pc++;
            }

            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (user_pc + 1) * sizeof(LLVMTypeRef));
            ptypes[0] = LLVMPointerType(struct_ty, 0);
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                const char *type_name = NULL;
                LLVMClassTypeEntry *param_cls = NULL;
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                if (p->type != NULL && p->type->type == AST_TYPE)
                    type_name = p->type->data.type.name;
                param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
                if (param_cls != NULL && param_cls->is_pointer_self_host)
                    ptypes[pidx++] = LLVMPointerType(param_cls->struct_type, 0);
                else
                    ptypes[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                (unsigned)(user_pc + 1), 0);

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, mname);
            LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn),
                                    fn, ft, ret);
            /* ptypes is ctx->scratch-owned. */
        }
        }
    }

    /* Pass 0b: Register ability vtable types */
    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        const char *ab_name = stmt->data.ability_decl.name;
        size_t mc = stmt->data.ability_decl.method_count;

        /* Build vtable struct: { fn_ptr_1, fn_ptr_2, ... }.  Type arrays
         * are consumed by LLVMFunctionType / LLVMStructSetBody and never
         * retained after the struct type is registered below. */
        LLVMTypeRef *vt_fields = pgy_arena_calloc(&ctx->scratch,
            (mc > 0 ? mc : 1) * sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = stmt->data.ability_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t pc = method->data.func_decl.param_count;
            /* Count non-self params */
            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                user_pc++;
            }
            LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                (user_pc + 1) * sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr; /* self */
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef fn_type = LLVMFunctionType(ret,
                ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
            /* ptypes is ctx->scratch-owned. */
        }

        char vt_name[256];
        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        LLVMTypeRef vt_struct = LLVMStructCreateNamed(ctx->context,
                                                        vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        /* vt_fields is ctx->scratch-owned. */

        /* Register as class type so it's findable.
         * Must strdup because vt_name is a stack local. */
        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            pergyra_strdup(vt_name), vt_struct, false, false);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = stmt->data.ability_decl.methods[j];
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        method->data.func_decl.name,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }

    /* Pass 0c: Forward-declare role methods + create vtable globals */
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                const char *mname = method->data.func_decl.name;
                size_t pc = method->data.func_decl.param_count;

                LLVMTypeRef ret = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret = ast_type_to_llvm(ctx,
                        method->data.func_decl.return_type);

                /* self + user params */
                size_t user_pc = 0;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    user_pc++;
                }

                LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
                    (user_pc + 1) * sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                size_t pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    ptypes[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                    (unsigned)(user_pc + 1), 0);

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, mname);
                LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                    fname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn),
                                        fn, ft, ret);
                /* ptypes is ctx->scratch-owned. */
            }
        }

        {
            PgyTokenType ops[] = {
                TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                TOKEN_GREATER, TOKEN_GREATER_EQUAL
            };
            const char *for_type_name = NULL;
            if (stmt->data.role_decl.for_type != NULL
                && stmt->data.role_decl.for_type->type == AST_TYPE) {
                for_type_name = stmt->data.role_decl.for_type->data.type.name;
            }

            for (size_t oi = 0; for_type_name != NULL
                   && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                const char *suffix = llvm_operator_suffix(ops[oi]);
                ASTNode *method = llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
                if (suffix == NULL || method == NULL)
                    continue;

                char opname[256];
                snprintf(opname, sizeof(opname), "operator_%s_%s",
                         suffix, for_type_name);
                if (llvm_lookup_function(ctx, opname) != NULL)
                    continue;

                FuncParam *rhs_param = NULL;
                size_t rhs_param_count = 0;
                for (size_t pj = 0; pj < method->data.func_decl.param_count; pj++) {
                    FuncParam *p = method->data.func_decl.params[pj];
                    if (!llvm_param_is_implicit_self_local(p)) {
                        rhs_param = p;
                        rhs_param_count++;
                    }
                }
                if (rhs_param_count != 1)
                    continue;

                LLVMTypeRef lhs_type = ast_type_to_llvm(ctx, stmt->data.role_decl.for_type);
                LLVMTypeRef rhs_type = (rhs_param != NULL && rhs_param->type != NULL)
                    ? ast_type_to_llvm(ctx, rhs_param->type) : ctx->type_i32;
                LLVMTypeRef ret = method->data.func_decl.return_type != NULL
                    ? ast_type_to_llvm(ctx, method->data.func_decl.return_type)
                    : ctx->type_void;
                LLVMTypeRef params[] = { lhs_type, rhs_type };
                LLVMTypeRef ft = LLVMFunctionType(ret, params, 2, 0);
                LLVMValueRef fn = LLVMAddFunction(ctx->module, opname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            }
        }
    }

    /* Pass 0e: Register event types and generate helper functions */
    for (size_t i = 0; i < event_count; i++) {
        ASTNode *stmt = events[i];
        if (stmt == NULL || stmt->type != AST_EVENT_DECL)
            continue;

        const char *ename = stmt->data.event_decl.name;
        int pc = (int)stmt->data.event_decl.param_count;

        /* Event struct: { [16 x ptr], i64 } ??handlers + count */
        LLVMTypeRef handler_arr = LLVMArrayType(ctx->type_i8ptr,
                                                 PGY_EVENT_MAX_HANDLERS);
        LLVMTypeRef sfields[] = { handler_arr, ctx->type_i64 };
        char sname[256];
        snprintf(sname, sizeof(sname), "PgyEvent_%s", ename);
        LLVMTypeRef evt_struct = LLVMStructCreateNamed(ctx->context, sname);
        LLVMStructSetBody(evt_struct, sfields, 2, 0);

        /* Collect handler parameter types */
        LLVMTypeRef ptypes[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = stmt->data.event_decl.params[j];
            ptypes[j] = (p->data.let_decl.type != NULL)
                ? ast_type_to_llvm(ctx, p->data.let_decl.type)
                : ctx->type_i32;
        }
        llvm_register_event(ctx, ename, evt_struct, pc, ptypes);

        /* Handler function type: void(param_types...) */
        LLVMTypeRef handler_ft = LLVMFunctionType(ctx->type_void,
            ptypes, (unsigned)pc, 0);
        LLVMTypeRef handler_ptr_t = LLVMPointerTypeInContext(ctx->context, 0);
        (void)handler_ptr_t;

        /* --- Generate EventName_INIT(ptr) ??void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INIT", ename);
            LLVMTypeRef init_params[] = { ctx->type_i8ptr };
            LLVMTypeRef init_ft = LLVMFunctionType(ctx->type_void,
                init_params, 1, 0);
            LLVMValueRef init_fn = LLVMAddFunction(ctx->module, fname, init_ft);
            llvm_register_function(ctx, LLVMGetValueName(init_fn),
                init_fn, init_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, init_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            /* memset(e, 0, sizeof(struct)) */
            LLVMValueRef e_ptr = LLVMGetParam(init_fn, 0);
            LLVMValueRef sz = LLVMSizeOf(evt_struct);
            LLVMBuildMemSet(ctx->builder, e_ptr,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 0, 0),
                sz, 0);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_SUBSCRIBE(ptr, handler_ptr) ??void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", ename);
            LLVMTypeRef sub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef sub_ft = LLVMFunctionType(ctx->type_void,
                sub_params, 2, 0);
            LLVMValueRef sub_fn = LLVMAddFunction(ctx->module, fname, sub_ft);
            llvm_register_function(ctx, LLVMGetValueName(sub_fn),
                sub_fn, sub_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            LLVMValueRef e_ptr = LLVMGetParam(sub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(sub_fn, 1);

            /* count_ptr = GEP(e, 0, 1) ??the i64 count field */
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* if (count < 16) { handlers[count] = h; count++; } */
            LLVMValueRef max_h = LLVMConstInt(ctx->type_i64,
                PGY_EVENT_MAX_HANDLERS, 0);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, count, max_h, "cmp");

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "then");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "end");
            LLVMBuildCondBr(ctx->builder, cmp, then_bb, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
            /* handlers_ptr = GEP(e, 0, 0, count) */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                count
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMBuildStore(ctx->builder, h_ptr, slot);

            /* count++ */
            LLVMValueRef new_count = LLVMBuildAdd(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "new_count");
            LLVMBuildStore(ctx->builder, new_count, count_ptr);
            LLVMBuildBr(ctx->builder, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_UNSUBSCRIBE(ptr, handler_ptr) ??void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", ename);
            LLVMTypeRef unsub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef unsub_ft = LLVMFunctionType(ctx->type_void,
                unsub_params, 2, 0);
            LLVMValueRef unsub_fn = LLVMAddFunction(ctx->module, fname, unsub_ft);
            llvm_register_function(ctx, LLVMGetValueName(unsub_fn),
                unsub_fn, unsub_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(unsub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(unsub_fn, 1);

            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* Loop: for (i = 0; i < count; i++) */
            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "loop");
            LLVMBasicBlockRef found_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "found");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "next");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "body");
            LLVMBuildCondBr(ctx->builder, cmp, body_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");
            LLVMValueRef eq = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, val, h_ptr, "eq");
            LLVMBuildCondBr(ctx->builder, eq, found_bb, next_bb);

            /* found: shift elements left, count-- */
            LLVMPositionBuilderAtEnd(ctx->builder, found_bb);
            /* Simple: set handlers[i] = handlers[count-1], count-- */
            LLVMValueRef last_idx_val = LLVMBuildSub(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "last");
            LLVMValueRef last_gep_idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                last_idx_val
            };
            LLVMValueRef last_slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, last_gep_idx, 3, "last_slot");
            LLVMValueRef last_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, last_slot, "last_val");
            LLVMBuildStore(ctx->builder, last_val, slot);
            LLVMBuildStore(ctx->builder, last_idx_val, count_ptr);
            LLVMBuildBr(ctx->builder, done_bb);

            /* next: i++ */
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_INVOKE(ptr, params...) ??void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", ename);
            /* params: ptr (event), then handler params.  Consumed by
             * LLVMFunctionType (which copies the type array) and never
             * retained beyond this block. */
            LLVMTypeRef *inv_params = pgy_arena_calloc(&ctx->scratch,
                (size_t)(pc + 1) * sizeof(LLVMTypeRef));
            inv_params[0] = ctx->type_i8ptr;
            for (int j = 0; j < pc; j++)
                inv_params[j + 1] = ptypes[j];

            LLVMTypeRef inv_ft = LLVMFunctionType(ctx->type_void,
                inv_params, (unsigned)(pc + 1), 0);
            LLVMValueRef inv_fn = LLVMAddFunction(ctx->module, fname, inv_ft);
            llvm_register_function(ctx, LLVMGetValueName(inv_fn),
                inv_fn, inv_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(inv_fn, 0);
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "loop");
            LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "call");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBuildCondBr(ctx->builder, cmp, call_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, call_bb);
            /* Load handler pointer */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef hval = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");

            /* Call handler(params...) via indirect call.  Arg buffer is
             * consumed by LLVMBuildCall2 and never retained. */
            LLVMValueRef *call_args = pgy_arena_calloc(&ctx->scratch,
                (size_t)pc * sizeof(LLVMValueRef));
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");
            /* call_args is ctx->scratch-owned. */

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);

            /* inv_params is ctx->scratch-owned. */
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

    /* Pass 2b: Emit role method bodies + vtable globals */
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name =
                (impl->data.impl_ability.ability_ref != NULL
                 && impl->data.impl_ability.ability_ref->type == AST_TYPE)
                ? impl->data.impl_ability.ability_ref->data.type.name : NULL;

            /* Emit method bodies */
            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                const MIRRoutine *mir_method =
                    llvm_find_mir_method_routine_local(ctx, role_name, method);
                if (fentry == NULL) continue;
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    continue;
                }
                if (ctx->mir != NULL) {
                    llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_MIR_ROUTINE_MISSING, PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING, PGY_FIX_INSPECT_MIR_INVENTORY, "MIR-only LLVM path missing routine for "
                                   "domain method '%s.%s'",
                                   role_name, method->data.func_decl.name);
                    return;
                }

                LLVMValueRef fn = fentry->fn;
                LLVMTypeRef ret_type = fentry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    ctx->type_i8ptr);

                /* User params */
                size_t pc = method->data.func_decl.param_count;
                unsigned lpidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef a = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, lpidx++), a);
                    llvm_scope_declare(ctx, p->name, a, pt);
                }

                {
                    char msg[384];
                    snprintf(msg, sizeof(msg),
                             "MIR-only LLVM path missing routine for role method '%s.%s'",
                             role_name != NULL ? role_name : "(anonymous-role)",
                             method->data.func_decl.name != NULL
                                 ? method->data.func_decl.name
                                 : "(anonymous)");
                    llvm_set_error_with_hints(ctx,
                        PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "%s", msg);
                    llvm_scope_pop(ctx);
                    return;
                }

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last =
                        LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }

            {
                PgyTokenType ops[] = {
                    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                    TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                    TOKEN_GREATER, TOKEN_GREATER_EQUAL
                };
                const char *for_type_name = NULL;
                if (stmt->data.role_decl.for_type != NULL
                    && stmt->data.role_decl.for_type->type == AST_TYPE) {
                    for_type_name = stmt->data.role_decl.for_type->data.type.name;
                }

                for (size_t oi = 0; for_type_name != NULL
                       && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                    const char *suffix = llvm_operator_suffix(ops[oi]);
                    ASTNode *method = llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
                    if (suffix == NULL || method == NULL)
                        continue;

                    char opname[256];
                    char mname[256];
                    snprintf(opname, sizeof(opname), "operator_%s_%s",
                             suffix, for_type_name);
                    snprintf(mname, sizeof(mname), "%s_%s",
                             role_name, method->data.func_decl.name);

                    LLVMFuncEntry *op_entry = llvm_lookup_function(ctx, opname);
                    LLVMFuncEntry *method_entry = llvm_lookup_function(ctx, mname);
                    if (op_entry == NULL || method_entry == NULL)
                        continue;
                    if (LLVMCountBasicBlocks(op_entry->fn) > 0)
                        continue;

                    LLVMValueRef saved_fn = ctx->current_function;
                    LLVMTypeRef saved_ret = ctx->current_ret_type;
                    LLVMTypeRef op_ret_type = op_entry->ret_type;
                    ctx->current_function = op_entry->fn;
                    ctx->current_ret_type = op_ret_type;

                    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                        ctx->context, op_entry->fn, "entry");
                    LLVMPositionBuilderAtEnd(ctx->builder, bb);

                    LLVMTypeRef lhs_type = LLVMTypeOf(LLVMGetParam(op_entry->fn, 0));
                    LLVMValueRef lhs_alloca = llvm_create_entry_alloca(
                        ctx, lhs_type, "lhs.addr");
                    LLVMBuildStore(ctx->builder, LLVMGetParam(op_entry->fn, 0), lhs_alloca);

                    LLVMValueRef lhs_self = LLVMBuildBitCast(ctx->builder,
                        lhs_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
                    LLVMValueRef rhs_arg = LLVMGetParam(op_entry->fn, 1);
                    LLVMValueRef args[] = { lhs_self, rhs_arg };

                    if (op_ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, method_entry->fn_type,
                            method_entry->fn, args, 2, "");
                        LLVMBuildRetVoid(ctx->builder);
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                            method_entry->fn_type, method_entry->fn,
                            args, 2, llvm_tmp_name(ctx));
                        LLVMBuildRet(ctx->builder, result);
                    }

                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    if (saved_fn != NULL) {
                        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                        if (last != NULL)
                            LLVMPositionBuilderAtEnd(ctx->builder, last);
                    }
                }
            }

            /* Create vtable global constant */
            char vt_type_name[256];
            snprintf(vt_type_name, sizeof(vt_type_name),
                     "%s_vtable", ab_name);
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = impl->data.impl_ability.method_count;
                /* Vtable method value array ??consumed by
                 * LLVMConstNamedStruct (copies) for the global initializer. */
                LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
                    (mc > 0 ? mc : 1) * sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = impl->data.impl_ability.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    char fname[256];
                    snprintf(fname, sizeof(fname), "%s_%s",
                             role_name, method->data.func_decl.name);
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    vals[j] = (fe != NULL) ? fe->fn
                        : LLVMConstNull(ctx->type_i8ptr);
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                snprintf(global_name, sizeof(global_name),
                         "%s_%s_vtable_instance", role_name, ab_name);
                LLVMValueRef global = LLVMAddGlobal(ctx->module,
                    vt_cls->struct_type, global_name);
                LLVMSetInitializer(global, vt_const);
                LLVMSetGlobalConstant(global, 1);
                LLVMSetLinkage(global, LLVMInternalLinkage);

                /* vals is ctx->scratch-owned. */
            }
        }
    }

    /* Pass 2c: Emit domain sync helpers + method bodies */
    for (size_t group = 0;
         group < sizeof(domain_groups) / sizeof(domain_groups[0]);
         group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
            if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **slots = NULL;
        size_t slot_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **refreshes = NULL;
        size_t refresh_count = 0;

        llvm_domain_decl_parts(stmt, &decl_name, &slots, &slot_count,
            &shared_fields, &shared_count, &methods, &method_count,
            &refreshes, &refresh_count);
        if (decl_name == NULL) {
            continue;
        }

        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);
        if (cls != NULL && cls->domain_kind != LLVM_DOMAIN_NONE
            && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC
            && cls->sync_function_name != NULL) {
            LLVMFuncEntry *sync_entry;
            sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
            if (sync_entry != NULL) {
                if (cls->domain_kind == LLVM_DOMAIN_ZONE)
                    llvm_emit_zone_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else if (cls->domain_kind == LLVM_DOMAIN_WORLD)
                    llvm_emit_world_sync(stmt, decl_name, cls, sync_entry->fn, ctx);
                else
                    llvm_emit_domain_projection_sync(stmt, decl_name, cls,
                        sync_entry->fn, ctx);
            }
        }

        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            const MIRRoutine *mir_method;
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            mir_method = llvm_find_mir_method_routine_local(ctx, decl_name, method);
            if (mir_method != NULL) {
                llvm_emit_func_from_mir(mir_method, ctx);
                continue;
            }
            if (ctx->mir != NULL) {
                char msg[384];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path missing routine for domain method '%s.%s'",
                         decl_name != NULL ? decl_name : "(anonymous-domain)",
                         method->data.func_decl.name != NULL
                             ? method->data.func_decl.name
                             : "(anonymous)");
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "%s", msg);
                return;
            }

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMTypeRef ret_type = fentry->ret_type;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ASTNode *saved_host_decl = NULL;
            LLVMFuncEntry *sync_entry = NULL;
            bool has_sync = false;
            ctx->current_function = fn;
            ctx->current_ret_type = ret_type;
            saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);

            if (cls != NULL && cls->sync_function_name != NULL
                && cls->domain_kind != LLVM_DOMAIN_NONE
                && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC) {
                sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
                has_sync = (sync_entry != NULL);
            }

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);
            llvm_scope_push(ctx);

            /* self param */
            LLVMValueRef self_val = LLVMGetParam(fn, 0);
            if (cls != NULL) {
                LLVMTypeRef self_ptr_t = LLVMPointerType(
                    cls->struct_type, 0);
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, self_ptr_t, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, self_ptr_t);
                llvm_register_var_class(ctx, "self", decl_name);
            } else {
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, ctx->type_i8ptr);
            }

            /* User params */
            size_t pc = method->data.func_decl.param_count;
            unsigned lpidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                const char *type_name = NULL;
                LLVMClassTypeEntry *param_cls = NULL;
                LLVMTypeRef pt;
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                if (p->type != NULL && p->type->type == AST_TYPE)
                    type_name = p->type->data.type.name;
                param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
                if (param_cls != NULL && param_cls->is_pointer_self_host)
                    pt = LLVMPointerType(param_cls->struct_type, 0);
                else
                    pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                LLVMValueRef a = llvm_create_entry_alloca(
                    ctx, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, lpidx++), a);
                llvm_scope_declare(ctx, p->name, a, pt);
                if (type_name != NULL && param_cls != NULL)
                    llvm_register_var_class(ctx, p->name, type_name);
            }

            if (has_sync) {
                LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                    LLVMPointerType(cls->struct_type, 0),
                    llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                LLVMValueRef sync_args[] = { self_ptr };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }

            {
                char msg[384];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path missing routine for domain method '%s.%s'",
                         decl_name != NULL ? decl_name : "(anonymous-domain)",
                         method->data.func_decl.name != NULL
                             ? method->data.func_decl.name
                             : "(anonymous)");
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "%s", msg);
                return;
            }

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (stmt->type == AST_WORLD_DECL && cls != NULL) {
                    for (size_t k = 0; k < stmt->data.world_decl.zone_count; k++) {
                        ASTNode *zone = stmt->data.world_decl.zones[k];
                        char dirty_field[256];
                        int dirty_idx;
                        LLVMValueRef self_ptr;
                        LLVMValueRef dirty_ptr;
                        const char *slot_name = zone != NULL
                            ? zone->data.world_zone.slot_name
                            : NULL;
                        if (slot_name == NULL)
                            continue;
                        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
                        dirty_idx = llvm_class_field_index(cls, dirty_field);
                        if (dirty_idx < 0)
                            continue;
                        self_ptr = LLVMBuildLoad2(ctx->builder,
                            LLVMPointerType(cls->struct_type, 0),
                            llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                        dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
                    }
                    int derived_idx = llvm_class_field_index(cls, "__world_derived_dirty");
                    if (derived_idx >= 0) {
                        LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                            LLVMPointerType(cls->struct_type, 0),
                            llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, self_ptr, (unsigned)derived_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), derived_ptr);
                    }
                }
                if (has_sync) {
                    LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                        LLVMPointerType(cls->struct_type, 0),
                        llvm_scope_lookup(ctx, "self")->alloca, llvm_tmp_name(ctx));
                    LLVMValueRef sync_args[] = { self_ptr };
                    LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                        sync_args, 1, "");
                }
                if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(ret_type, 0, 0));
            }

            llvm_scope_pop(ctx);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            llvm_restore_current_host_decl(ctx, saved_host_decl);

            if (saved_fn != NULL) {
                LLVMBasicBlockRef last =
                    LLVMGetLastBasicBlock(saved_fn);
                if (last != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, last);
            }
        }
        }
    }

}

#endif /* PGY_LLVM_ENABLED */
