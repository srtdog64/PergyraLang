/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct registration and field inventory materialization.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include "../parser/ast_api.h"
#include "host_decl_compat.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_forward.h"
#include "llvm_domain_method_emit.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_struct_fields.h"
#include "llvm_domain_struct_register_fields.h"
#include "llvm_inventory_decl_lookup.h"

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
            ASTNode **refreshes = NULL;
            size_t refresh_count = 0;

            llvm_domain_decl_refreshes(stmt, &decl_name, &refreshes,
                &refresh_count);
            if (decl_name == NULL)
                continue;
            LLVMHostedSharedFieldView shared_view =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(
                    &shared_view)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing domain shared-field metadata for '%s'",
                    decl_name);
                return;
            }

            size_t dyn_slot_count = 0;
            LLVMHostedRoleSlotView role_view =
                llvm_hosted_role_slot_view_from_decl(ctx, decl_name, stmt);
            if (llvm_hosted_role_slot_view_missing_mir_metadata(&role_view)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing party role-slot metadata for '%s'",
                    decl_name);
                return;
            }
            for (size_t j = 0; j < role_view.count; j++) {
                if (llvm_hosted_role_slot_view_is_dynamic(&role_view, j))
                    dyn_slot_count++;
            }

            LLVMHostedDomainSlotView domain_slot_view =
                llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt);
            size_t domain_slot_count = domain_slot_view.count;
            size_t fc = 0;
            LLVMTypeRef *ftypes = NULL;
            if (stmt->type == AST_ZONE_DECL) {
                LLVMHostedZoneLayerSlotView layer_view =
                    llvm_hosted_zone_layer_slot_view_from_decl(
                        ctx, decl_name, stmt);
                size_t state_count = 0;
                (void) ast_zone_states(stmt, &state_count);
                size_t projection_count =
                    llvm_count_domain_projection_slots_in_view(
                        &domain_slot_view,
                        refreshes,
                        refresh_count);
                if (llvm_hosted_domain_slot_view_missing_mir_metadata(
                        &domain_slot_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing zone domain-slot metadata for '%s'",
                        decl_name);
                    return;
                }
                if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(
                        &layer_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing zone layer-slot metadata for '%s'",
                        decl_name);
                    return;
                }
                fc = domain_slot_count
                    + shared_view.count
                    + layer_view.count
                    + layer_view.count
                    + (layer_view.count * 2)
                    + state_count
                    + (state_count * 2)
                    + (projection_count * 4)
                    + 1;
                ftypes = pgy_arena_calloc(&ctx->scratch,
                    (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
                if (ftypes == NULL) {
                    llvm_set_error(ctx,
                        "LLVM domain struct field allocation failed for '%s'",
                        decl_name != NULL ? decl_name : "(anonymous)");
                    return;
                }
                size_t idx = 0;
                for (size_t j = 0; j < domain_slot_count; j++, idx++) {
                    ASTNode *slot_type =
                        llvm_hosted_domain_slot_view_type(
                            &domain_slot_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(
                        ctx, stmt, slot_type, "zone slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < shared_view.count; j++, idx++) {
                    ASTNode *sf =
                        llvm_hosted_shared_field_view_source_ast(
                            &shared_view, j);
                    ASTNode *sf_type =
                        llvm_hosted_shared_field_view_type(&shared_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "zone shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < layer_view.count; j++, idx++) {
                    const char *layer_type =
                        llvm_hosted_zone_layer_slot_view_type_name(
                            &layer_view, j);
                    LLVMClassTypeEntry *layer_cls = NULL;
                    if (layer_type != NULL)
                        layer_cls = llvm_lookup_class(ctx, layer_type);
                    if (llvm_hosted_zone_layer_slot_view_is_pool(
                            &layer_view, j)
                        && layer_cls != NULL) {
                        ftypes[idx] = llvm_zone_effect_pool_struct_type(ctx,
                            layer_cls->struct_type,
                            llvm_hosted_zone_layer_slot_view_pool_capacity(
                                &layer_view, j));
                    } else {
                        ftypes[idx] = layer_cls != NULL ? layer_cls->struct_type : ctx->type_i8ptr;
                    }
                }
                for (size_t j = 0; j < layer_view.count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < layer_view.count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                for (size_t j = 0; j < state_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < state_count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                for (size_t j = 0; j < domain_slot_count; j++) {
                    if (!llvm_domain_slot_view_is_projection_slot(
                            &domain_slot_view, j, refreshes,
                            refresh_count)) {
                        continue;
                    }
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i1;
                    ftypes[idx++] = ctx->type_i32;
                    ftypes[idx++] = ctx->type_i32;
                }
                ftypes[idx++] = ctx->type_i32;
            } else if (stmt->type == AST_ROSTER_DECL) {
                LLVMHostedRosterSlotView roster_view =
                    llvm_hosted_roster_slot_view_from_decl(
                        ctx, decl_name, stmt);
                if (llvm_hosted_roster_slot_view_missing_mir_metadata(
                        &roster_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing roster-slot metadata for '%s'",
                        decl_name != NULL ? decl_name : "<anonymous>");
                    return;
                }
                fc = roster_view.count
                    + shared_view.count;
                ftypes = pgy_arena_calloc(&ctx->scratch,
                    (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
                if (ftypes == NULL) {
                    llvm_set_error(ctx,
                        "LLVM domain struct field allocation failed for '%s'",
                        decl_name != NULL ? decl_name : "(anonymous)");
                    return;
                }
                size_t idx = 0;
                for (size_t j = 0; j < roster_view.count; j++, idx++) {
                    const char *party_type =
                        llvm_hosted_roster_slot_view_type_name(
                            &roster_view, j);
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        stmt, party_type, "roster party slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < shared_view.count; j++, idx++) {
                    ASTNode *sf =
                        llvm_hosted_shared_field_view_source_ast(
                            &shared_view, j);
                    ASTNode *sf_type =
                        llvm_hosted_shared_field_view_type(&shared_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "roster shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
            } else if (stmt->type == AST_WORLD_DECL) {
                LLVMHostedWorldRosterSlotView roster_view =
                    llvm_hosted_world_roster_slot_view_from_decl(
                        ctx, decl_name, stmt);
                size_t roster_count = roster_view.count;
                LLVMHostedWorldZoneSlotView zone_view =
                    llvm_hosted_world_zone_slot_view_from_decl(ctx, decl_name,
                        stmt);
                size_t zone_count = zone_view.count;
                size_t state_count = 0;
                (void) ast_world_states(stmt, &state_count);
                if (llvm_hosted_world_roster_slot_view_missing_mir_metadata(
                        &roster_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing world roster-slot metadata for '%s'",
                        decl_name != NULL ? decl_name : "<anonymous>");
                    return;
                }
                if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(
                        &zone_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing world zone-slot metadata for '%s'",
                        decl_name != NULL ? decl_name : "<anonymous>");
                    return;
                }
                fc = roster_count
                    + zone_count
                    + shared_view.count
                    + zone_count
                    + zone_count
                    + zone_count
                    + state_count
                    + (zone_count * 2)
                    + (state_count * 2)
                    + 1;
                ftypes = pgy_arena_calloc(&ctx->scratch,
                    (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
                if (ftypes == NULL) {
                    llvm_set_error(ctx,
                        "LLVM domain struct field allocation failed for '%s'",
                        decl_name != NULL ? decl_name : "(anonymous)");
                    return;
                }
                size_t idx = 0;
                for (size_t j = 0; j < roster_count; j++, idx++) {
                    const char *roster_type =
                        llvm_hosted_world_roster_slot_view_type_name(
                            &roster_view, j);
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        stmt, roster_type, "world roster slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < zone_count; j++, idx++) {
                    const char *zone_type =
                        llvm_hosted_world_zone_slot_view_type_name(
                            &zone_view, j);
                    ftypes[idx] = llvm_domain_required_class_struct_type(ctx,
                        stmt, zone_type, "world zone slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < shared_view.count; j++, idx++) {
                    ASTNode *sf =
                        llvm_hosted_shared_field_view_source_ast(
                            &shared_view, j);
                    ASTNode *sf_type =
                        llvm_hosted_shared_field_view_type(&shared_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "world shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < zone_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < zone_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < zone_count; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                for (size_t j = 0; j < state_count; j++, idx++)
                    ftypes[idx] = ctx->type_i1;
                for (size_t j = 0; j < zone_count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                for (size_t j = 0; j < state_count * 2; j++, idx++)
                    ftypes[idx] = ctx->type_i32;
                ftypes[idx] = ctx->type_i1;
            } else {
                size_t projection_count =
                    (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                    ? llvm_count_domain_projection_slots_in_view(
                        &domain_slot_view, refreshes, refresh_count)
                    : 0;
                if ((stmt->type == AST_RELATION_DECL
                        || stmt->type == AST_EFFECT_DECL)
                    && llvm_hosted_domain_slot_view_missing_mir_metadata(
                        &domain_slot_view)) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing domain-slot metadata for '%s'",
                        decl_name);
                    return;
                }
                fc = domain_slot_count + shared_view.count + dyn_slot_count + projection_count;
                if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
                    fc = domain_slot_count + shared_view.count + dyn_slot_count + (projection_count * 4);
                ftypes = pgy_arena_calloc(&ctx->scratch,
                    (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
                if (ftypes == NULL) {
                    llvm_set_error(ctx,
                        "LLVM domain struct field allocation failed for '%s'",
                        decl_name != NULL ? decl_name : "(anonymous)");
                    return;
                }
                size_t idx = 0;
                for (size_t j = 0; j < domain_slot_count; j++, idx++) {
                    ASTNode *slot_type =
                        llvm_hosted_domain_slot_view_type(
                            &domain_slot_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(
                        ctx, stmt, slot_type, "domain slot");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < shared_view.count; j++, idx++) {
                    ASTNode *sf =
                        llvm_hosted_shared_field_view_source_ast(
                            &shared_view, j);
                    ASTNode *sf_type =
                        llvm_hosted_shared_field_view_type(&shared_view, j);
                    ftypes[idx] = llvm_domain_required_ast_type(ctx, sf, sf_type, "domain shared field");
                    if (ctx->has_error || ftypes[idx] == NULL)
                        return;
                }
                for (size_t j = 0; j < dyn_slot_count; j++, idx++)
                    ftypes[idx] = ctx->type_i8ptr;
                if (projection_count > 0) {
                    for (size_t j = 0; j < domain_slot_count; j++) {
                        if (!llvm_domain_slot_view_is_projection_slot(
                                &domain_slot_view, j, refreshes,
                                refresh_count)) {
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
                    refreshes, refresh_count)) {
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
