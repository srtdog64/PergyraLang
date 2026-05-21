#ifdef PGY_LLVM_ENABLED
#include "llvm_domain_world_frontier_internal.h"
#include "llvm_domain_world_sync_internal.h"

void
llvm_world_frontier_emit_zone_sync_pass(ASTNode *stmt,
                                        LLVMClassTypeEntry *decl_cls,
                                        LLVMValueRef sync_fn,
                                        LLVMValueRef needs_derived_addr,
                                        LLVMValueRef derived_ptr,
                                        ASTNode **zones,
                                        size_t zone_count,
                                        LLVMGenCtx *ctx)
{
    (void)stmt;

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        int zone_idx;
        int dirty_idx;
        int seen_idx;
        char dirty_field[256];
        char seen_field[256];
        LLVMValueRef self_ptr;
        LLVMValueRef dirty_ptr;
        LLVMValueRef dirty_val;
        LLVMBasicBlockRef sync_bb;
        LLVMBasicBlockRef cont_bb;
        const char *slot_name = ast_world_zone_slot_name(zone);
        const char *zone_type_name = ast_world_zone_type_name(zone);
        if (slot_name == NULL)
            continue;
        zone_idx = llvm_class_field_index(decl_cls, slot_name);
        if (!llvm_world_frontier_field_name(dirty_field, sizeof(dirty_field),
                "zone_dirty", slot_name))
            continue;
        if (!llvm_world_frontier_field_name(seen_field, sizeof(seen_field),
                "zone_seen_generation", slot_name))
            continue;
        dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
        seen_idx = llvm_class_field_index(decl_cls, seen_field);
        self_ptr = LLVMGetParam(sync_fn, 0);
        if (zone_idx < 0 || dirty_idx < 0 || zone_type_name == NULL)
            continue;
        {
            LLVMClassTypeEntry *zone_cls = llvm_lookup_class(ctx, zone_type_name);
            char sync_name[256];
            LLVMFuncEntry *zone_sync;
            if (!llvm_world_frontier_sync_name(sync_name, sizeof(sync_name),
                    zone_type_name))
                continue;
            zone_sync = llvm_lookup_function(ctx, sync_name);
            if (zone_cls == NULL || zone_sync == NULL)
                continue;
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            if (seen_idx >= 0) {
                int generation_idx = llvm_class_field_index(zone_cls, "__sync_generation");
                if (generation_idx >= 0) {
                    LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)zone_idx,
                        llvm_tmp_name(ctx));
                    LLVMValueRef generation_ptr = LLVMBuildStructGEP2(ctx->builder,
                        zone_cls->struct_type, zone_ptr, (unsigned)generation_idx,
                        llvm_tmp_name(ctx));
                    LLVMValueRef seen_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)seen_idx,
                        llvm_tmp_name(ctx));
                    /*
                     * Atomic acquire load — pairs with the release-order
                     * atomic increment emitted by
                     * llvm_emit_sync_generation_increment and the
                     * PGY_ZONE_GENERATION_INC macro on the C backend.
                     * Without this the compare-against-seen race-loses on
                     * parallel/spawn paths.
                     */
                    LLVMValueRef generation_val = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i32, generation_ptr, llvm_tmp_name(ctx));
                    LLVMSetOrdering(generation_val, LLVMAtomicOrderingAcquire);
                    LLVMSetAlignment(generation_val, 4);
                    LLVMValueRef seen_val = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i32, seen_ptr, llvm_tmp_name(ctx));
                    LLVMValueRef generation_changed = LLVMBuildICmp(ctx->builder,
                        LLVMIntNE, generation_val, seen_val, llvm_tmp_name(ctx));
                    LLVMValueRef dirty_prev = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i1, dirty_ptr, llvm_tmp_name(ctx));
                    LLVMValueRef dirty_next = LLVMBuildOr(ctx->builder,
                        dirty_prev, generation_changed, llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, dirty_next, dirty_ptr);
                    if (derived_ptr != NULL) {
                        LLVMValueRef derived_next = LLVMBuildOr(ctx->builder,
                            LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                                derived_ptr, llvm_tmp_name(ctx)),
                            generation_changed, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, derived_next, derived_ptr);
                    }
                }
            }
            dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                dirty_ptr, llvm_tmp_name(ctx));
            sync_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.sync");
            cont_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.cont");
            LLVMBuildCondBr(ctx->builder, dirty_val, sync_bb, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, sync_bb);
            {
                LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, zone_sync->fn_type, zone_sync->fn, args, 1, "");
                if (seen_idx >= 0) {
                    int generation_idx = llvm_class_field_index(zone_cls, "__sync_generation");
                    if (generation_idx >= 0) {
                        LLVMValueRef generation_ptr = LLVMBuildStructGEP2(ctx->builder,
                            zone_cls->struct_type, zone_ptr, (unsigned)generation_idx,
                            llvm_tmp_name(ctx));
                        LLVMValueRef seen_ptr = LLVMBuildStructGEP2(ctx->builder,
                            decl_cls->struct_type, self_ptr, (unsigned)seen_idx,
                            llvm_tmp_name(ctx));
                        /*
                         * Atomic acquire load of the zone's
                         * __sync_generation counter so the seen-cache
                         * update sees a coherent value when another
                         * task incremented under release order.
                         */
                        LLVMValueRef generation_val = LLVMBuildLoad2(ctx->builder,
                            ctx->type_i32, generation_ptr, llvm_tmp_name(ctx));
                        LLVMSetOrdering(generation_val, LLVMAtomicOrderingAcquire);
                        LLVMSetAlignment(generation_val, 4);
                        LLVMBuildStore(ctx->builder, generation_val, seen_ptr);
                    }
                }
            }
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), dirty_ptr);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), needs_derived_addr);
            LLVMBuildBr(ctx->builder, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        }
    }
}

void
llvm_world_frontier_emit_pending_zone_dirty(LLVMClassTypeEntry *decl_cls,
                                           LLVMValueRef sync_fn,
                                           LLVMValueRef derived_dirty_addr,
                                           LLVMValueRef derived_ptr,
                                           LLVMValueRef frontier_continue_addr,
                                           LLVMValueRef changed_any_addr,
                                           ASTNode **zones,
                                           size_t zone_count,
                                           LLVMGenCtx *ctx)
{
    LLVMValueRef pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
        changed_any_addr, llvm_tmp_name(ctx));
    LLVMValueRef dirty_pending = derived_ptr != NULL
        ? LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx))
        : LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
            llvm_tmp_name(ctx));
    pending_val = LLVMBuildOr(ctx->builder, pending_val, dirty_pending,
        llvm_tmp_name(ctx));
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        char dirty_field[256];
        int dirty_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef dirty_ptr;
        LLVMValueRef dirty_val;
        const char *slot_name = ast_world_zone_slot_name(zone);
        if (slot_name == NULL)
            continue;
        if (!llvm_world_frontier_field_name(dirty_field,
                sizeof(dirty_field), "zone_dirty", slot_name))
            continue;
        dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
        if (dirty_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
        dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            dirty_ptr, llvm_tmp_name(ctx));
        pending_val = LLVMBuildOr(ctx->builder, pending_val, dirty_val,
            llvm_tmp_name(ctx));
    }
    LLVMBuildStore(ctx->builder, pending_val, frontier_continue_addr);
}

#endif /* PGY_LLVM_ENABLED */
