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
#include "llvm_domain_event.h"
#include "llvm_domain_method_emit.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_role_emit.h"

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
    llvm_emit_domain_event_helpers(ctx, events, event_count);

    /* Pass 2b: Emit role method bodies + vtable globals */
    if (!llvm_emit_domain_role_method_bodies(ctx, roles, role_count))
        return;

    /* Pass 2c: Emit domain sync helpers + method bodies */
    if (!llvm_emit_domain_sync_and_method_bodies(ctx, domain_groups,
            domain_group_counts,
            sizeof(domain_groups) / sizeof(domain_groups[0])))
        return;

}

#endif /* PGY_LLVM_ENABLED */
