#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_projection_sync.h"

#include <stdio.h>
#include <string.h>

#include "llvm_expr_assignment_projection.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"

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
    LLVMVarEntry *self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMFuncEntry *sync_entry;
    int zone_field_idx;

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

    world_cls = llvm_lookup_class(ctx, host_decl->data.world_decl.name);
    zone_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (world_cls == NULL || zone_cls == NULL || self_var == NULL
        || zone_cls->sync_function_name == NULL) {
        return;
    }

    sync_entry = llvm_lookup_function(ctx, zone_cls->sync_function_name);
    if (sync_entry == NULL || sync_entry->fn == NULL || sync_entry->fn_type == NULL)
        return;

    world_ptr = self_var->alloca;
    if (self_var->type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
            self_var->alloca, llvm_tmp_name(ctx));
    }

    zone_field_idx = llvm_class_field_index(world_cls, zone_slot_name);
    if (zone_field_idx < 0)
        return;

    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
        world_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        char field_name[256];
        int field_idx;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || slot->data.domain_slot.is_subject) {
            continue;
        }

        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
            slot->data.domain_slot.slot_name);
        field_idx = llvm_class_field_index(zone_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }

        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            slot->data.domain_slot.slot_name);
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

        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", zone_slot_name);
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
        snprintf(world_sync_name, sizeof(world_sync_name), "%s_sync",
            host_decl->data.world_decl.name != NULL
                ? host_decl->data.world_decl.name : "World");
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

    source_slot_name = receiver->data.identifier.name;
    host_name = host_decl->data.zone_decl.name;
    if (source_slot_name == NULL || host_name == NULL)
        return;

    host_cls = llvm_lookup_class(ctx, host_name);
    if (host_cls == NULL || host_cls->sync_function_name == NULL)
        return;

    self_ptr = llvm_current_self_base_ptr(ctx, host_cls);
    if (self_ptr == NULL)
        return;

    for (size_t i = 0; i < host_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = host_decl->data.zone_decl.refreshes[i];
        const char *target_name;
        const char *refresh_source;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_name = refresh->data.zone_refresh.object_slot_name;
        refresh_source = refresh->data.zone_refresh.source_slot_name;
        if (target_name == NULL || refresh_source == NULL
            || strcmp(refresh_source, source_slot_name) != 0) {
            continue;
        }

        snprintf(dirty_field, sizeof(dirty_field), "__projection_dirty_%s",
            target_name);
        snprintf(ready_field, sizeof(ready_field), "__projection_ready_%s",
            target_name);
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
