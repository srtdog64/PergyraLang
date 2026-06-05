#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_projection_sync.h"

#include <stdio.h>
#include <string.h>

#include "llvm_expr_assignment_projection.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static bool
llvm_projection_sync_call_field_name(char *out,
    size_t out_size,
    const char *kind,
    const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || slot_name == NULL)
        return false;

    written = snprintf(out, out_size, "__%s_%s", kind, slot_name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_projection_sync_call_function_name(char *out,
    size_t out_size,
    const char *type_name)
{
    int written;

    if (out == NULL || out_size == 0 || type_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_sync", type_name);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_emit_world_embedded_receiver_projection_sync(LLVMGenCtx *ctx,
                                                  ASTNode *receiver)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot_name = NULL;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMFuncEntry *sync_entry;
    int zone_field_idx;
    const char *world_name;
    const char *zone_name;

    if (ctx == NULL || receiver == NULL)
        return;

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return;
    if (!llvm_world_embedded_projection_source_from_assignment(ctx, receiver,
            &zone_slot_name, &zone_decl, &source_slot_name, NULL)
        || zone_slot_name == NULL
        || zone_decl == NULL) {
        return;
    }

    world_name = llvm_decl_node_name(host_decl);
    zone_name = llvm_decl_node_name(zone_decl);
    if (world_name == NULL || zone_name == NULL)
        return;
    world_cls = llvm_lookup_class(ctx, world_name);
    zone_cls = llvm_lookup_class(ctx, zone_name);
    if (world_cls == NULL || zone_cls == NULL
        || !llvm_scope_lookup_snapshot(ctx, "self", &self_var)
        || zone_cls->sync_function_name == NULL) {
        return;
    }

    sync_entry = llvm_lookup_function(ctx, zone_cls->sync_function_name);
    if (sync_entry == NULL || sync_entry->fn == NULL || sync_entry->fn_type == NULL)
        return;

    world_ptr = self_var.alloca;
    if (self_var.type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var.type,
            self_var.alloca, llvm_tmp_name(ctx));
    }

    zone_field_idx = llvm_class_field_index(world_cls, zone_slot_name);
    if (zone_field_idx < 0)
        return;

    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
        world_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));

    LLVMHostedDomainSlotView slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing embedded zone projection metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return;
    }
    for (size_t i = 0; i < slot_view.count; i++) {
        const char *slot_name =
            llvm_hosted_domain_slot_view_name(&slot_view, i);
        char field_name[256];
        int field_idx;
        if (slot_name == NULL
            || llvm_hosted_domain_slot_view_is_subject_like(
                &slot_view, i)) {
            continue;
        }

        if (!llvm_projection_sync_call_field_name(field_name,
                sizeof(field_name), "projection_dirty",
                slot_name)) {
            llvm_set_error(ctx, "projection dirty field name is too long");
            return;
        }
        field_idx = llvm_class_field_index(zone_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }

        if (!llvm_projection_sync_call_field_name(field_name,
                sizeof(field_name), "projection_ready",
                slot_name)) {
            llvm_set_error(ctx, "projection ready field name is too long");
            return;
        }
        field_idx = llvm_class_field_index(zone_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), ready_ptr);
        }
    }

    {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
            args, 1, "");
    }

    {
        char dirty_field[256];
        int dirty_idx;
        int derived_idx;
        char world_sync_name[256];
        LLVMFuncEntry *world_sync;

        if (!llvm_projection_sync_call_field_name(dirty_field,
                sizeof(dirty_field), "zone_dirty", zone_slot_name)) {
            llvm_set_error(ctx, "world zone-dirty field name is too long");
            return;
        }
        dirty_idx = llvm_class_field_index(world_cls, dirty_field);
        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                world_cls->struct_type, world_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }
        derived_idx = llvm_class_field_index(world_cls, "__world_derived_dirty");
        if (derived_idx >= 0) {
            LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                world_cls->struct_type, world_ptr, (unsigned)derived_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), derived_ptr);
        }
        if (!llvm_projection_sync_call_function_name(world_sync_name,
                sizeof(world_sync_name),
                world_name != NULL ? world_name : "World")) {
            llvm_set_error(ctx, "world sync function name is too long");
            return;
        }
        world_sync = llvm_lookup_function(ctx, world_sync_name);
        if (world_sync != NULL && world_sync->fn != NULL && world_sync->fn_type != NULL) {
            LLVMValueRef world_args[] = { world_ptr };
            LLVMBuildCall2(ctx->builder, world_sync->fn_type, world_sync->fn,
                world_args, 1, "");
        }
    }
}

void
llvm_emit_current_zone_subject_projection_sync(LLVMGenCtx *ctx, ASTNode *receiver)
{
    ASTNode *host_decl;
    const char *source_slot_name;
    const char *host_name;
    LLVMClassTypeEntry *host_cls;
    LLVMValueRef self_ptr;
    bool emitted = false;

    if (ctx == NULL || receiver == NULL || receiver->type != AST_IDENTIFIER)
        return;

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL || host_decl->type != AST_ZONE_DECL)
        return;

    source_slot_name = ast_identifier_name(receiver);
    host_name = llvm_decl_node_name(host_decl);
    if (source_slot_name == NULL || host_name == NULL)
        return;

    host_cls = llvm_lookup_class(ctx, host_name);
    if (host_cls == NULL || host_cls->sync_function_name == NULL)
        return;

    self_ptr = llvm_current_self_base_ptr(ctx, host_cls);
    if (self_ptr == NULL)
        return;

    size_t refresh_count = 0;
    ASTNode **refreshes = ast_zone_refreshes(host_decl, &refresh_count);
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        const char *target_name;
        const char *refresh_source;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_name = ast_zone_refresh_object_slot_name(refresh);
        refresh_source = ast_zone_refresh_source_slot_name(refresh);
        if (target_name == NULL || refresh_source == NULL
            || strcmp(refresh_source, source_slot_name) != 0) {
            continue;
        }

        if (!llvm_projection_sync_call_field_name(dirty_field,
                sizeof(dirty_field), "projection_dirty", target_name)) {
            llvm_set_error(ctx, "projection dirty field name is too long");
            return;
        }
        if (!llvm_projection_sync_call_field_name(ready_field,
                sizeof(ready_field), "projection_ready", target_name)) {
            llvm_set_error(ctx, "projection ready field name is too long");
            return;
        }
        dirty_idx = llvm_class_field_index(host_cls, dirty_field);
        ready_idx = llvm_class_field_index(host_cls, ready_field);

        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, self_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
            emitted = true;
        }
        if (ready_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, self_ptr, (unsigned)ready_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
    }

    if (emitted) {
        LLVMFuncEntry *sync_entry = llvm_lookup_function(ctx,
            host_cls->sync_function_name);
        if (sync_entry != NULL && sync_entry->fn != NULL
            && sync_entry->fn_type != NULL) {
            LLVMValueRef args[] = { self_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                args, 1, "");
        }
    }
}

#endif
