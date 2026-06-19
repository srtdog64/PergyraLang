#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_sync_body_helpers.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "domain_frontier_policy.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static bool
llvm_projection_sync_field_name(char *out,
                                size_t out_size,
                                const char *kind,
                                const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || slot_name == NULL)
        return false;
    written = snprintf(out, out_size, "__projection_%s_%s", kind, slot_name);
    return written >= 0 && (size_t)written < out_size;
}

static const char *
llvm_projection_sync_slot_type_name(const LLVMHostedDomainSlotView *slot_view,
                                    const char *slot_name)
{
    if (slot_view == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_view->count; i++) {
        const char *candidate =
            llvm_hosted_domain_slot_view_name(slot_view, i);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0)
            return llvm_hosted_domain_slot_view_type_name(slot_view, i);
    }
    return NULL;
}

void
llvm_emit_domain_projection_sync_body(ASTNode *stmt,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx)
{
    const char *decl_name = NULL;
    LLVMHostedDomainSlotView slot_view;
    LLVMHostedZoneRefreshView refresh_view = {0};

    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    decl_name = llvm_decl_node_name(stmt);
    if (stmt->type != AST_RELATION_DECL
        && stmt->type != AST_EFFECT_DECL
        && stmt->type != AST_ZONE_DECL) {
        return;
    }
    refresh_view = llvm_hosted_zone_refresh_view_from_decl(ctx, decl_name,
        stmt);
    if (llvm_hosted_zone_refresh_view_missing_mir_metadata(&refresh_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing domain refresh declaration metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        return;
    }
    if (refresh_view.count == 0)
        return;

    slot_view = llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing projection sync domain-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
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

        for (size_t i = 0; i < refresh_view.count; i++) {
            const char *target_slot_name = NULL;
            char field_name[256];
            int dirty_index;
            LLVMValueRef self_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef dirty_val;
            LLVMValueRef continue_val;

            target_slot_name =
                llvm_hosted_zone_refresh_view_object_slot_name(
                    &refresh_view, i);
            if (target_slot_name == NULL)
                continue;
            if (!llvm_projection_sync_field_name(field_name,
                    sizeof(field_name), "dirty", target_slot_name))
                continue;
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
                (unsigned)pgy_domain_projection_frontier_pass_limit(
                    refresh_view.count), 0);
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

        for (size_t i = 0; i < refresh_view.count; i++) {
            ASTNode *refresh = NULL;
            const MIRDeclZoneRefresh *zone_refresh = NULL;
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

            zone_refresh = llvm_hosted_zone_refresh_view_metadata(
                &refresh_view, i);
            if (zone_refresh == NULL
                && refresh_view.ast_compat_refreshes != NULL) {
                refresh = refresh_view.ast_compat_refreshes[i];
            }
            target_slot_name =
                llvm_hosted_zone_refresh_view_object_slot_name(
                    &refresh_view, i);
            source_slot_name =
                llvm_hosted_zone_refresh_view_source_slot_name(
                    &refresh_view, i);
            if (target_slot_name == NULL || source_slot_name == NULL)
                continue;

            target_type_name = llvm_projection_sync_slot_type_name(
                &slot_view, target_slot_name);
            source_type_name = llvm_projection_sync_slot_type_name(
                &slot_view, source_slot_name);
            if (target_type_name == NULL || source_type_name == NULL)
                continue;

            target_cls = llvm_lookup_class(ctx, target_type_name);
            source_cls = llvm_lookup_class(ctx, source_type_name);
            target_index = llvm_class_field_index(decl_cls, target_slot_name);
            source_index = llvm_class_field_index(decl_cls, source_slot_name);
            {
                char field_name[256];
                if (!llvm_projection_sync_field_name(field_name,
                        sizeof(field_name), "dirty", target_slot_name))
                    continue;
                dirty_index = llvm_class_field_index(decl_cls, field_name);
                if (!llvm_projection_sync_field_name(field_name,
                        sizeof(field_name), "ready", target_slot_name))
                    continue;
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

                    projected = zone_refresh != NULL
                        ? llvm_build_domain_projection_value_from_zone_refresh_metadata(
                            ctx, target_cls, source_cls, source_type_name,
                            zone_refresh, source_ptr)
                        : llvm_build_domain_projection_value(ctx, target_cls,
                            source_cls, source_type_name, refresh, source_ptr);
                    if (projected == NULL || ctx->has_error)
                        return;
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

                    for (size_t dep_i = 0; dep_i < refresh_view.count; dep_i++) {
                        const char *dependent_target_name = NULL;
                        const char *dependent_source_name = NULL;
                        char dep_field_name[256];
                        int dep_dirty_index;
                        int dep_ready_index;

                        dependent_target_name =
                            llvm_hosted_zone_refresh_view_object_slot_name(
                                &refresh_view, dep_i);
                        dependent_source_name =
                            llvm_hosted_zone_refresh_view_source_slot_name(
                                &refresh_view, dep_i);
                        if (dependent_target_name == NULL || dependent_source_name == NULL
                            || strcmp(dependent_source_name, target_slot_name) != 0) {
                            continue;
                        }

                        if (!llvm_projection_sync_field_name(dep_field_name,
                                sizeof(dep_field_name), "dirty",
                                dependent_target_name))
                            continue;
                        dep_dirty_index = llvm_class_field_index(decl_cls, dep_field_name);
                        if (!llvm_projection_sync_field_name(dep_field_name,
                                sizeof(dep_field_name), "ready",
                                dependent_target_name))
                            continue;
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
            PGY_FRONTIER_REASON_PROJECTION_OVERFLOW);

        LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
    }
}

#endif
