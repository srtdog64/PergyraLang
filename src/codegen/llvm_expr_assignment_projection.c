/*
 * LLVM projection invalidation and world-embedded sync for assignments.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_assignment_projection.h"

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static bool
llvm_host_projection_source_from_assignment(LLVMGenCtx *ctx,
                                            ASTNode *host_decl,
                                            ASTNode *target,
                                            const char **source_slot_out,
                                            const char **source_field_out)
{
    const char *host_name;
    LLVMHostedDomainSlotView slot_view;
    ASTNode *cursor = target;
    const char *source_field = NULL;

    if (source_slot_out != NULL)
        *source_slot_out = NULL;
    if (source_field_out != NULL)
        *source_field_out = NULL;
    if (host_decl == NULL || target == NULL)
        return false;

    switch (host_decl->type) {
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        break;
    default:
        return false;
    }
    host_name = llvm_decl_node_name(host_decl);
    slot_view = llvm_hosted_domain_slot_view_from_decl(ctx, host_name,
        host_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing domain-slot assignment metadata for '%s'",
            host_name != NULL ? host_name : "<anonymous>");
        return false;
    }

    if (target->type == AST_IDENTIFIER && ast_identifier_name(target) != NULL) {
        const char *target_name = ast_identifier_name(target);
        for (size_t i = 0; i < slot_view.count; i++) {
            const char *slot_name =
                llvm_hosted_domain_slot_view_name(&slot_view, i);
            if (slot_name != NULL
                && strcmp(slot_name, target_name) == 0) {
                if (source_slot_out != NULL)
                    *source_slot_out = target_name;
                return true;
            }
        }
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = ast_member_object(cursor);
        if (source_field == NULL)
            source_field = ast_member_name(cursor);
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && ast_identifier_name(obj) != NULL) {
            const char *obj_name = ast_identifier_name(obj);
            for (size_t i = 0; i < slot_view.count; i++) {
                const char *slot_name =
                    llvm_hosted_domain_slot_view_name(&slot_view, i);
                if (slot_name != NULL
                    && strcmp(slot_name, obj_name) == 0) {
                    if (source_slot_out != NULL)
                        *source_slot_out = obj_name;
                    if (source_field_out != NULL)
                        *source_field_out = source_field;
                    return true;
                }
            }
        }
        cursor = obj;
    }

    return false;
}

static bool
llvm_refresh_mentions_source_field(ASTNode *refresh, const char *source_field)
{
    if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
        return false;
    if (source_field == NULL)
        return true;
    if (ast_zone_refresh_field_map_count(refresh) == 0)
        return true;

    for (size_t i = 0; i < ast_zone_refresh_field_map_count(refresh); i++) {
        const char *mapped_source =
            ast_zone_refresh_mapped_source_field(refresh, i);
        if (mapped_source != NULL && strcmp(mapped_source, source_field) == 0)
            return true;
    }
    return false;
}

static bool
llvm_zone_refresh_metadata_mentions_source_field(
    const MIRDeclZoneRefresh *refresh,
    const char *source_field)
{
    if (refresh == NULL)
        return false;
    if (source_field == NULL)
        return true;
    if (mir_decl_zone_refresh_field_map_count(refresh) == 0)
        return true;

    for (size_t i = 0; i < mir_decl_zone_refresh_field_map_count(refresh); i++) {
        const char *mapped_source =
            mir_decl_zone_refresh_mapped_source_field(refresh, i);
        if (mapped_source != NULL && strcmp(mapped_source, source_field) == 0)
            return true;
    }
    return false;
}

static void
llvm_emit_projection_invalidations_for_zone_refresh_view(
    LLVMGenCtx *ctx,
    const LLVMHostedZoneRefreshView *refresh_view,
    LLVMClassTypeEntry *host_cls,
    LLVMValueRef host_ptr,
    const char *source_slot,
    const char *source_field)
{
    if (ctx == NULL || refresh_view == NULL || refresh_view->count == 0
        || host_cls == NULL || host_ptr == NULL || source_slot == NULL) {
        return;
    }

    for (size_t i = 0; i < refresh_view->count; i++) {
        const MIRDeclZoneRefresh *refresh =
            llvm_hosted_zone_refresh_view_metadata(refresh_view, i);
        const char *target_slot =
            llvm_hosted_zone_refresh_view_object_slot_name(refresh_view, i);
        const char *refresh_source =
            llvm_hosted_zone_refresh_view_source_slot_name(refresh_view, i);
        bool mentions_source_field = refresh != NULL
            ? llvm_zone_refresh_metadata_mentions_source_field(refresh,
                source_field)
            : llvm_refresh_mentions_source_field(
                refresh_view->ast_compat_refreshes != NULL
                    ? refresh_view->ast_compat_refreshes[i] : NULL,
                source_field);
        char field_name[256];
        int field_idx;

        if (target_slot == NULL || refresh_source == NULL
            || strcmp(refresh_source, source_slot) != 0
            || !mentions_source_field) {
            continue;
        }

        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
            target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }

        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
    }
}

bool
llvm_world_embedded_projection_source_from_assignment(LLVMGenCtx *ctx,
                                                      ASTNode *target,
                                                      const char **zone_slot_out,
                                                      ASTNode **zone_decl_out,
                                                      const char **source_slot_out,
                                                      const char **source_field_out)
{
    ASTNode *world_decl;
    ASTNode *cursor = target;
    const char *source_field = NULL;

    if (zone_slot_out != NULL)
        *zone_slot_out = NULL;
    if (zone_decl_out != NULL)
        *zone_decl_out = NULL;
    if (source_slot_out != NULL)
        *source_slot_out = NULL;
    if (source_field_out != NULL)
        *source_field_out = NULL;

    if (ctx == NULL || target == NULL)
        return false;

    world_decl = llvm_current_host_decl(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return false;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *receiver = ast_member_object(cursor);
        const char *slot_name = ast_member_name(cursor);
        const char *zone_slot_name = NULL;
        ASTNode *zone_decl = NULL;

        if (receiver != NULL && slot_name != NULL) {
            if (receiver->type == AST_IDENTIFIER
                && ast_identifier_name(receiver) != NULL) {
                zone_slot_name = ast_identifier_name(receiver);
            } else if (receiver->type == AST_MEMBER_ACCESS
                       && ast_member_object(receiver) != NULL
                       && ast_member_object(receiver)->type == AST_IDENTIFIER
                       && ast_identifier_name(ast_member_object(receiver)) != NULL
                       && strcmp(ast_identifier_name(ast_member_object(receiver)),
                           "self") == 0
                       && ast_member_name(receiver) != NULL) {
                zone_slot_name = ast_member_name(receiver);
            }

            if (zone_slot_name != NULL) {
                zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl,
                    zone_slot_name);
                if (zone_decl != NULL
                    && llvm_zone_has_domain_slot(ctx, zone_decl,
                        slot_name)) {
                    if (zone_slot_out != NULL)
                        *zone_slot_out = zone_slot_name;
                    if (zone_decl_out != NULL)
                        *zone_decl_out = zone_decl;
                    if (source_slot_out != NULL)
                        *source_slot_out = slot_name;
                    if (source_field_out != NULL)
                        *source_field_out = source_field;
                    return true;
                }
            }
        }

        source_field = ast_member_name(cursor);
        cursor = ast_member_object(cursor);
    }

    return false;
}

void
llvm_emit_host_projection_invalidations(LLVMGenCtx *ctx, ASTNode *target)
{
    ASTNode *host_decl;
    LLVMHostedZoneRefreshView refresh_view = {0};
    const char *source_slot = NULL;
    const char *source_field = NULL;
    LLVMClassTypeEntry *host_cls;
    LLVMVarEntry self_var;
    bool has_self_var;
    LLVMValueRef host_ptr;

    if (ctx == NULL || target == NULL)
        return;

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL)
        return;

    switch (host_decl->type) {
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        refresh_view = llvm_hosted_zone_refresh_view_from_decl(ctx,
            llvm_decl_node_name(host_decl), host_decl);
        if (llvm_hosted_zone_refresh_view_missing_mir_metadata(
                &refresh_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing domain refresh assignment metadata for '%s'",
                llvm_decl_node_name(host_decl) != NULL
                    ? llvm_decl_node_name(host_decl) : "(anonymous-domain)");
            return;
        }
        break;
    case AST_WORLD_DECL: {
        const char *zone_slot = NULL;
        ASTNode *zone_decl = NULL;
        LLVMClassTypeEntry *world_cls;
        LLVMHostedZoneRefreshView embedded_refresh_view;
        int zone_field_idx;
        const char *world_name;
        const char *zone_name;

        if (!llvm_world_embedded_projection_source_from_assignment(ctx, target,
                &zone_slot, &zone_decl, &source_slot, &source_field)
            || zone_slot == NULL
            || zone_decl == NULL
            || source_slot == NULL) {
            return;
        }

        world_name = llvm_decl_node_name(host_decl);
        zone_name = llvm_decl_node_name(zone_decl);
        world_cls = llvm_lookup_class(ctx, world_name);
        has_self_var = llvm_scope_lookup_snapshot(ctx, "self", &self_var);
        if (world_cls == NULL || !has_self_var)
            return;

        host_cls = llvm_lookup_class(ctx, zone_name);
        embedded_refresh_view = llvm_hosted_zone_refresh_view_from_decl(ctx,
            zone_name, zone_decl);
        if (llvm_hosted_zone_refresh_view_missing_mir_metadata(
                &embedded_refresh_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing embedded zone refresh assignment metadata for '%s'",
                zone_name != NULL ? zone_name : "(anonymous-zone)");
            return;
        }
        if (host_cls == NULL || embedded_refresh_view.count == 0)
            return;

        host_ptr = self_var.alloca;
        if (self_var.type == LLVMPointerType(world_cls->struct_type, 0)) {
            host_ptr = LLVMBuildLoad2(ctx->builder, self_var.type,
                self_var.alloca, llvm_tmp_name(ctx));
        }

        zone_field_idx = llvm_class_field_index(world_cls, zone_slot);
        if (zone_field_idx < 0)
            return;

        host_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
            host_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));
        llvm_emit_projection_invalidations_for_zone_refresh_view(ctx,
            &embedded_refresh_view, host_cls, host_ptr,
            source_slot, source_field);
        return;
    }
    default:
        return;
    }

    if (!llvm_host_projection_source_from_assignment(ctx, host_decl, target,
            &source_slot, &source_field)
        || source_slot == NULL
        || refresh_view.count == 0) {
        return;
    }

    host_cls = llvm_lookup_class(ctx, llvm_decl_node_name(host_decl));
    has_self_var = llvm_scope_lookup_snapshot(ctx, "self", &self_var);
    if (host_cls == NULL || !has_self_var)
        return;

    host_ptr = self_var.alloca;
    if (self_var.type == LLVMPointerType(host_cls->struct_type, 0)) {
        host_ptr = LLVMBuildLoad2(ctx->builder, self_var.type,
            self_var.alloca, llvm_tmp_name(ctx));
    }
    llvm_emit_projection_invalidations_for_zone_refresh_view(ctx,
        &refresh_view, host_cls, host_ptr, source_slot, source_field);
}

void
llvm_emit_world_embedded_assignment_sync(LLVMGenCtx *ctx, ASTNode *target)
{
    const char *zone_slot = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot = NULL;
    ASTNode *world_decl;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMFuncEntry *sync_entry;
    int zone_field_idx;
    const char *world_name;
    const char *zone_name;

    if (ctx == NULL || target == NULL)
        return;

    if (!llvm_world_embedded_projection_source_from_assignment(ctx, target,
            &zone_slot, &zone_decl, &source_slot, NULL)
        || zone_slot == NULL
        || zone_decl == NULL
        || source_slot == NULL) {
        return;
    }

    world_decl = llvm_current_host_decl(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    world_name = llvm_decl_node_name(world_decl);
    zone_name = llvm_decl_node_name(zone_decl);
    world_cls = llvm_lookup_class(ctx, world_name);
    zone_cls = llvm_lookup_class(ctx, zone_name);
    if (world_cls == NULL || zone_cls == NULL
        || !llvm_scope_lookup_snapshot(ctx, "self", &self_var)
        || zone_cls->sync_function_name == NULL) {
        return;
    }

    sync_entry = llvm_lookup_function(ctx, zone_cls->sync_function_name);
    if (sync_entry == NULL || sync_entry->fn == NULL
        || sync_entry->fn_type == NULL) {
        return;
    }

    world_ptr = self_var.alloca;
    if (self_var.type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var.type,
            self_var.alloca, llvm_tmp_name(ctx));
    }

    zone_field_idx = llvm_class_field_index(world_cls, zone_slot);
    if (zone_field_idx < 0)
        return;

    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
        world_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));
    {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
            args, 1, "");
    }
}

#endif
