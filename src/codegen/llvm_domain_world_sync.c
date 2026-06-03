#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_world_sync_internal.h"
#include "llvm_inventory_decl_lookup.h"

static bool
llvm_world_sync_field_name(char *out,
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
llvm_world_sync_prev_active_name(char *out,
                                 size_t out_size,
                                 const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || slot_name == NULL)
        return false;
    written = snprintf(out, out_size, "world.prev_active.%s", slot_name);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_emit_world_sync(ASTNode *stmt, const char *decl_name,
                     LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                     LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef saved_bb;
    LLVMLexicalRegistrySnapshot lexical_snapshot;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_bb = LLVMGetInsertBlock(ctx->builder);
    lexical_snapshot = llvm_lexical_registry_snapshot(ctx);
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMValueRef derived_dirty_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.derived_dirty.addr");
        LLVMValueRef needs_derived_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.needs_derived.addr");
        int derived_idx = llvm_class_field_index(decl_cls, "__world_derived_dirty");
        LLVMValueRef derived_ptr = NULL;
        LLVMValueRef derived_val = LLVMConstInt(ctx->type_i1, 0, 0);
        const char *world_name = llvm_decl_node_name(stmt);
        LLVMHostedWorldZoneSlotView zone_view =
            llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name, stmt);
        size_t zone_count = zone_view.count;
        /* Per-zone "previously active" pointer cache populated during
         * world sync emission and consumed once before this function
         * returns.  Never escapes. */
        LLVMValueRef *prev_active_addrs = pgy_arena_calloc(&ctx->scratch,
            (zone_count > 0 ? zone_count : 1) * sizeof(LLVMValueRef));
        if (prev_active_addrs == NULL) {
            llvm_set_error(ctx,
                "LLVM world sync previous-active allocation failed for '%s'",
                world_name != NULL ? world_name : "(anonymous)");
            llvm_scope_pop(ctx);
            llvm_lexical_registry_restore(ctx, lexical_snapshot);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            if (saved_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
            llvm_restore_current_host_decl(ctx, saved_host_decl);
            return;
        }

        if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing world zone-slot metadata for '%s'",
                world_name != NULL ? world_name : "<anonymous>");
            llvm_scope_pop(ctx);
            llvm_lexical_registry_restore(ctx, lexical_snapshot);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            if (saved_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
            llvm_restore_current_host_decl(ctx, saved_host_decl);
            return;
        }

        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
        llvm_scope_declare(ctx, "__world_derived_dirty_local", derived_dirty_addr, ctx->type_i1);

        if (derived_idx >= 0) {
            derived_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                LLVMGetParam(sync_fn, 0), (unsigned)derived_idx, llvm_tmp_name(ctx));
            derived_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                derived_ptr, llvm_tmp_name(ctx));
        }
        LLVMBuildStore(ctx->builder, derived_val, derived_dirty_addr);

        /* world command pass: reset */
        for (size_t i = 0; i < zone_count; i++) {
            char active_field[256];
            char prev_name[256];
            int active_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef prev_addr;
            LLVMValueRef prev_val;
            const char *slot_name =
                llvm_hosted_world_zone_slot_view_name(&zone_view, i);
            if (slot_name == NULL)
                continue;
            if (!llvm_world_sync_field_name(active_field, sizeof(active_field),
                    "zone_active", slot_name))
                continue;
            active_idx = llvm_class_field_index(decl_cls, active_field);
            self_ptr = LLVMGetParam(sync_fn, 0);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            if (!llvm_world_sync_prev_active_name(prev_name, sizeof(prev_name),
                    slot_name))
                continue;
            prev_addr = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, prev_val, prev_addr);
            prev_active_addrs[i] = prev_addr;
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
        }

        /* world command pass: directives */
        llvm_world_sync_emit_directives(stmt, decl_cls, sync_fn, ctx);

        for (size_t i = 0; i < zone_count; i++) {
            const char *slot_name;
            char active_field[256];
            char dirty_field[256];
            int active_idx;
            int dirty_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef active_val;
            LLVMValueRef prev_val;
            LLVMValueRef changed_val;
            slot_name = llvm_hosted_world_zone_slot_view_name(&zone_view, i);
            if (slot_name == NULL || prev_active_addrs[i] == NULL)
                continue;
            if (!llvm_world_sync_field_name(active_field, sizeof(active_field),
                    "zone_active", slot_name))
                continue;
            if (!llvm_world_sync_field_name(dirty_field, sizeof(dirty_field),
                    "zone_dirty", slot_name))
                continue;
            active_idx = llvm_class_field_index(decl_cls, active_field);
            dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
            if (active_idx < 0 || dirty_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                prev_active_addrs[i], llvm_tmp_name(ctx));
            changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, active_val, prev_val,
                llvm_tmp_name(ctx));
            {
                LLVMValueRef prev_dirty = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    dirty_ptr, llvm_tmp_name(ctx));
                changed_val = LLVMBuildOr(ctx->builder, prev_dirty, changed_val,
                    llvm_tmp_name(ctx));
            }
            LLVMBuildStore(ctx->builder, changed_val, dirty_ptr);
            {
                LLVMValueRef derived_dirty_val = LLVMBuildOr(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                        llvm_tmp_name(ctx)),
                    changed_val, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, derived_dirty_val, derived_dirty_addr);
                if (derived_ptr != NULL)
                    LLVMBuildStore(ctx->builder, derived_dirty_val, derived_ptr);
            }
        }

        llvm_world_sync_emit_frontier(stmt, decl_cls, sync_fn,
            derived_dirty_addr, needs_derived_addr, derived_ptr, ctx);

        /* prev_active_addrs is ctx->scratch-owned. */
    }

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif /* PGY_LLVM_ENABLED */
