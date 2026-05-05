/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct registration and field inventory materialization.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_forward.h"
#include "llvm_domain_method_emit.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_projection_target_helpers.h"
#include "llvm_domain_struct_fields.h"

static LLVMTypeRef
llvm_domain_required_ast_type(LLVMGenCtx *ctx,
                              ASTNode *field_node,
                              ASTNode *type_node,
                              const char *field_kind)
{
    if (ctx == NULL)
        return NULL;
    if (type_node != NULL)
        return ast_type_to_llvm(ctx, type_node);

    llvm_set_error_at_with_hints(ctx, field_node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM domain %s requires explicit type metadata; silent i32 fallback is not allowed",
        field_kind != NULL ? field_kind : "field");
    return ctx->type_i32;
}

void
llvm_register_domain_structs(LLVMGenCtx *ctx,
                             ASTNode ***domain_groups,
                             const size_t *domain_group_counts,
                             size_t domain_group_count)
{
    for (size_t group = 0; group < domain_group_count; group++) {
        ASTNode **group_nodes = domain_groups[group];
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = group_nodes[i];
            if (stmt == NULL)
                continue;

            const char *decl_name = NULL;
            ASTNode **slots = NULL;
            size_t slot_count = 0;
            ASTNode **shared_fields = NULL;
            size_t shared_count = 0;
            ASTNode **refreshes = NULL;
            size_t refresh_count = 0;

            llvm_domain_decl_parts(stmt, &decl_name, &slots, &slot_count,
                &shared_fields, &shared_count, &refreshes, &refresh_count);
            if (decl_name == NULL)
                continue;

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
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, slot, slot_type, "zone slot");
                }
                for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, idx++) {
                    ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "zone shared field");
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
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < stmt->data.zone_decl.state_count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
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
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "roster shared field");
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
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "world shared field");
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
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, slot, slot_type, "domain slot");
                }
                for (size_t j = 0; j < shared_count; j++, idx++) {
                    ASTNode *sf = shared_fields[j];
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "domain shared field");
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

            LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context, decl_name);
            LLVMStructSetBody(struct_ty, ftypes, (unsigned)fc, 0);

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
                    llvm_domain_add_projection_state_fields(ctx, entry, ftypes, &field_index,
                        stmt->data.zone_decl.slots,
                        stmt->data.zone_decl.slot_count,
                        stmt->data.zone_decl.refreshes,
                        stmt->data.zone_decl.refresh_count);
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
                        field_index++;
                    }
                    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                        llvm_domain_add_projection_state_fields(ctx, entry, ftypes, &field_index,
                            slots, slot_count, refreshes, refresh_count);
                }
            }

            if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL
                || stmt->type == AST_ZONE_DECL || stmt->type == AST_WORLD_DECL) {
                llvm_emit_domain_sync_forward_decl(ctx, decl_name, struct_ty, entry);
            }

            {
                LLVMHostedMethodView method_view =
                    llvm_hosted_method_view_from_decl(ctx, decl_name, stmt);
                llvm_emit_domain_method_forward_decls(ctx, decl_name, struct_ty,
                    &method_view);
            }
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
