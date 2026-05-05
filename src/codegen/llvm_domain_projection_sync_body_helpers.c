#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_sync_body_helpers.h"

#include <stdio.h>
#include <string.h>

#include "domain_frontier_policy.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_internal_api.h"

void
llvm_emit_domain_projection_sync_body(ASTNode *stmt,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx)
{
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    const char *unused_name = NULL;
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    ASTNode **unused_shared = NULL;
    size_t unused_shared_count = 0;

    llvm_domain_decl_parts(stmt, &unused_name, &slots, &slot_count,
        &unused_shared, &unused_shared_count, &refreshes, &refresh_count);

    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL
        || refresh_count == 0) {
        return;
    }

    {
        LLVMValueRef pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
            "projection.pass.addr");
        LLVMValueRef continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "projection.continue.addr");
        LLVMBasicBlockRef loop_check_bb = LLVMAppendBasicBlockInContext(ctx->context,
            sync_fn, "projection.loop.check");
        LLVMBasicBlockRef loop_body_bb = LLVMAppendBasicBlockInContext(ctx->context,
            sync_fn, "projection.loop.body");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(ctx->context,
            sync_fn, "projection.loop.done");
        LLVMBasicBlockRef overflow_bb = LLVMAppendBasicBlockInContext(ctx->context,
            sync_fn, "projection.loop.overflow");
        LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(ctx->context,
            sync_fn, "projection.loop.exit");

        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), pass_addr);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);

        for (size_t i = 0; i < refresh_count; i++) {
            ASTNode *refresh = refreshes[i];
            const char *target_slot_name;
            char field_name[256];
            int dirty_index;
            LLVMValueRef self_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef dirty_val;
            LLVMValueRef continue_val;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            target_slot_name = refresh->data.zone_refresh.object_slot_name;
            if (target_slot_name == NULL)
                continue;
            snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                target_slot_name);
            dirty_index = llvm_class_field_index(decl_cls, field_name);
            if (dirty_index < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_index, llvm_tmp_name(ctx));
            dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                dirty_ptr, llvm_tmp_name(ctx));
            continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                continue_addr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMBuildOr(ctx->builder, continue_val, dirty_val, llvm_tmp_name(ctx)),
                continue_addr);
        }

        LLVMBuildBr(ctx->builder, loop_check_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, loop_check_bb);
        {
            LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                continue_addr, llvm_tmp_name(ctx));
            LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                pass_addr, llvm_tmp_name(ctx));
            LLVMValueRef pass_limit = LLVMConstInt(ctx->type_i32,
                (unsigned)pgy_domain_projection_frontier_pass_limit(refresh_count), 0);
            LLVMValueRef within_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
                pass_val, pass_limit, llvm_tmp_name(ctx));
            LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
                within_limit, llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, loop_cond, loop_body_bb, done_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, loop_body_bb);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);
        LLVMBuildStore(ctx->builder,
            LLVMBuildAdd(ctx->builder,
                LLVMBuildLoad2(ctx->builder, ctx->type_i32, pass_addr, llvm_tmp_name(ctx)),
                LLVMConstInt(ctx->type_i32, 1, 0),
                llvm_tmp_name(ctx)),
            pass_addr);

        for (size_t i = 0; i < refresh_count; i++) {
            ASTNode *refresh = refreshes[i];
            LLVMClassTypeEntry *target_cls;
            LLVMClassTypeEntry *source_cls;
            int target_index;
            int source_index;
            int dirty_index;
            int ready_index;
            LLVMValueRef self_ptr;
            const char *target_slot_name;
            const char *source_slot_name;
            const char *target_type_name;
            const char *source_type_name;
            ASTNode *target_slot_decl = NULL;
            ASTNode *source_slot_decl = NULL;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;

            target_slot_name = refresh->data.zone_refresh.object_slot_name;
            source_slot_name = refresh->data.zone_refresh.source_slot_name;
            if (target_slot_name == NULL || source_slot_name == NULL)
                continue;

            for (size_t j = 0; j < slot_count; j++) {
                ASTNode *slot = slots[j];
                if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                    continue;
                if (slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, target_slot_name) == 0) {
                    target_slot_decl = slot;
                }
                if (slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, source_slot_name) == 0) {
                    source_slot_decl = slot;
                }
            }
            if (target_slot_decl == NULL || source_slot_decl == NULL
                || target_slot_decl->data.domain_slot.type == NULL
                || source_slot_decl->data.domain_slot.type == NULL
                || target_slot_decl->data.domain_slot.type->type != AST_TYPE
                || source_slot_decl->data.domain_slot.type->type != AST_TYPE) {
                continue;
            }

            target_type_name = target_slot_decl->data.domain_slot.type->data.type.name;
            source_type_name = source_slot_decl->data.domain_slot.type->data.type.name;
            if (target_type_name == NULL || source_type_name == NULL)
                continue;

            target_cls = llvm_lookup_class(ctx, target_type_name);
            source_cls = llvm_lookup_class(ctx, source_type_name);
            target_index = llvm_class_field_index(decl_cls, target_slot_name);
            source_index = llvm_class_field_index(decl_cls, source_slot_name);
            {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                    target_slot_name);
                dirty_index = llvm_class_field_index(decl_cls, field_name);
                snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                    target_slot_name);
                ready_index = llvm_class_field_index(decl_cls, field_name);
            }
            if (target_cls == NULL || source_cls == NULL
                || target_index < 0 || source_index < 0 || dirty_index < 0) {
                continue;
            }

            self_ptr = LLVMGetParam(sync_fn, 0);
            {
                LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                    decl_cls->struct_type, self_ptr, (unsigned)dirty_index,
                    llvm_tmp_name(ctx));
                LLVMValueRef dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    dirty_ptr, llvm_tmp_name(ctx));
                LLVMBasicBlockRef sync_bb = LLVMAppendBasicBlockInContext(ctx->context,
                    sync_fn, "projection.sync");
                LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
                    sync_fn, "projection.cont");

                LLVMBuildCondBr(ctx->builder, dirty_val, sync_bb, cont_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, sync_bb);
                {
                    LLVMValueRef target_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)target_index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef source_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)source_index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef projected;

                    if (ready_index >= 0) {
                        LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                            decl_cls->struct_type, self_ptr, (unsigned)ready_index,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                            ready_ptr);
                    }

                    projected = llvm_build_domain_projection_value(ctx, target_cls,
                        source_cls, llvm_find_domain_projection_nominal_decl(ctx, source_type_name),
                        refresh, source_ptr);
                    LLVMBuildStore(ctx->builder, projected, target_ptr);

                    if (ready_index >= 0) {
                        LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                            decl_cls->struct_type, self_ptr, (unsigned)ready_index,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                            ready_ptr);
                    }
                    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                        dirty_ptr);
                    llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                        "projection", target_slot_name, PGY_PROP_CAUSE_REFRESH);

                    for (size_t dep_i = 0; dep_i < refresh_count; dep_i++) {
                        ASTNode *dependent = refreshes[dep_i];
                        const char *dependent_target_name;
                        const char *dependent_source_name;
                        char dep_field_name[256];
                        int dep_dirty_index;
                        int dep_ready_index;

                        if (dependent == NULL || dependent->type != AST_ZONE_REFRESH)
                            continue;
                        dependent_target_name = dependent->data.zone_refresh.object_slot_name;
                        dependent_source_name = dependent->data.zone_refresh.source_slot_name;
                        if (dependent_target_name == NULL || dependent_source_name == NULL
                            || strcmp(dependent_source_name, target_slot_name) != 0) {
                            continue;
                        }

                        snprintf(dep_field_name, sizeof(dep_field_name),
                            "__projection_dirty_%s", dependent_target_name);
                        dep_dirty_index = llvm_class_field_index(decl_cls, dep_field_name);
                        snprintf(dep_field_name, sizeof(dep_field_name),
                            "__projection_ready_%s", dependent_target_name);
                        dep_ready_index = llvm_class_field_index(decl_cls, dep_field_name);
                        if (dep_dirty_index >= 0) {
                            LLVMValueRef dep_dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                                decl_cls->struct_type, self_ptr, (unsigned)dep_dirty_index,
                                llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                                dep_dirty_ptr);
                        }
                        if (dep_ready_index >= 0) {
                            LLVMValueRef dep_ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                                decl_cls->struct_type, self_ptr, (unsigned)dep_ready_index,
                                llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                                dep_ready_ptr);
                        }
                        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                            continue_addr);
                    }
                }
                LLVMBuildBr(ctx->builder, cont_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
            }
        }

        LLVMBuildBr(ctx->builder, loop_check_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
        {
            LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                continue_addr, llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, continue_val, overflow_bb, exit_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, overflow_bb);
        llvm_emit_frontier_overflow_abort(ctx,
            "projection recompute exceeded bounded pass limit");

        LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
    }
}

#endif
