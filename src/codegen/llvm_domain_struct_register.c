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
#include "llvm_domain_struct_register_fields.h"

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
            size_t role_count = 0;
            if (stmt->type == AST_PARTY_DECL) {
                role_count = ast_party_role_count(stmt);
            }
            for (size_t j = 0; j < role_count; j++) {
                ASTNode *role_slot = ast_party_role(stmt, j);
                if (role_slot != NULL
                    && role_slot->type == AST_ROLE_SLOT
                    && role_slot->data.role_slot.is_dynamic)
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
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, idx++) {
                    ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "zone shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
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
                fc = ast_roster_party_count(stmt)
                    + ast_roster_shared_count(stmt);
                ftypes = pgy_arena_calloc(&ctx->scratch,
                    (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
                size_t idx = 0;
                for (size_t j = 0; j < ast_roster_party_count(stmt); j++, idx++) {
                    ASTNode *slot = ast_roster_party(stmt, j);
                    const char *party_type =
                        (slot != NULL && slot->type == AST_SYSTEMIC_SLOT)
                        ? slot->data.roster_slot.party_type : NULL;
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        slot, party_type, "roster party slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < ast_roster_shared_count(stmt); j++, idx++) {
                    ASTNode *sf = ast_roster_shared(stmt, j);
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "roster shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
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
                    const char *roster_type =
                        ws != NULL ? ws->data.world_roster.roster_type : NULL;
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        ws, roster_type, "world roster slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, idx++) {
                    ASTNode *wz = stmt->data.world_decl.zones[j];
                    const char *zone_type =
                        wz != NULL ? wz->data.world_zone.zone_type : NULL;
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        wz, zone_type, "world zone slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, idx++) {
                    ASTNode *sf = stmt->data.world_decl.shared_fields[j];
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "world shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
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
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < shared_count; j++, idx++) {
                    ASTNode *sf = shared_fields[j];
                    ASTNode *sf_type = sf->data.party_shared.type;
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "domain shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
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
            if (entry != NULL
                && !llvm_domain_struct_register_fields(ctx, stmt, entry, ftypes,
                    slots, slot_count, shared_fields, shared_count, refreshes,
                    refresh_count)) {
                return;
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
