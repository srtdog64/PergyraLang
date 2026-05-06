/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_method_emit.h"

#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_inventory_host_methods.h"
#include "domain_frontier_policy.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
#include "llvm_domain_projection_sync_helpers.h"

bool
llvm_emit_domain_sync_and_method_bodies(LLVMGenCtx *ctx,
    ASTNode ***domain_groups,
    const size_t *domain_group_counts,
    size_t domain_group_count)
{
    if (ctx == NULL || domain_groups == NULL || domain_group_counts == NULL)
        return true;

    for (size_t group = 0; group < domain_group_count; group++) {
        for (size_t i = 0; i < domain_group_counts[group]; i++) {
            ASTNode *stmt = domain_groups[group][i];
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

            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);
            if (cls != NULL && cls->domain_kind != LLVM_DOMAIN_NONE
                && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC
                && cls->sync_function_name != NULL) {
                LLVMFuncEntry *sync_entry;
                sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
                if (sync_entry != NULL) {
                    if (cls->domain_kind == LLVM_DOMAIN_ZONE)
                        llvm_emit_zone_sync(stmt, decl_name, cls, sync_entry->fn,
                            ctx);
                    else if (cls->domain_kind == LLVM_DOMAIN_WORLD)
                        llvm_emit_world_sync(stmt, decl_name, cls, sync_entry->fn,
                            ctx);
                    else
                        llvm_emit_domain_projection_sync(stmt, decl_name, cls,
                            sync_entry->fn, ctx);
                }
            }

            LLVMHostedMethodView method_view =
                llvm_hosted_method_view_from_decl(ctx, decl_name, stmt);
            for (size_t j = 0; j < method_view.count; j++) {
                const MIRDeclMethod *method_meta =
                    llvm_hosted_method_view_metadata(&method_view, j);
                ASTNode *method =
                    llvm_hosted_method_view_source_ast(&method_view, j);
                const char *method_name = llvm_mir_decl_method_name(method_meta);
                const MIRRoutine *mir_method = NULL;
                if (method_name == NULL && method != NULL
                    && method->type == AST_FUNC_DECL)
                    method_name = method->data.func_decl.name;
                if (method_meta == NULL
                    && (method == NULL || method->type != AST_FUNC_DECL)) {
                    continue;
                }

                if (method_meta != NULL && method_meta->has_routine) {
                    LLVMMIRRoutineInventory routine_inventory;
                    llvm_active_routine_inventory(ctx, &routine_inventory);
                    mir_method = llvm_routine_inventory_get(
                        &routine_inventory, method_meta->routine_index);
                }
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    continue;
                }
                if (ctx->mir != NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing routine for domain method '%s.%s'",
                        decl_name != NULL ? decl_name : "(anonymous-domain)",
                        method_name != NULL
                            ? method_name
                            : "(anonymous)");
                    return false;
                }

                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                if (method->data.func_decl.name == NULL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         decl_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                if (fentry == NULL)
                    continue;

                LLVMValueRef fn = fentry->fn;
                LLVMTypeRef ret_type = fentry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ASTNode *saved_host_decl = NULL;
                LLVMFuncEntry *sync_entry = NULL;
                bool has_sync = false;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;
                saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);

                if (cls != NULL && cls->sync_function_name != NULL
                    && cls->domain_kind != LLVM_DOMAIN_NONE
                    && cls->domain_kind != LLVM_DOMAIN_SYSTEMIC) {
                    sync_entry = llvm_lookup_function(ctx, cls->sync_function_name);
                    has_sync = (sync_entry != NULL);
                }

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                if (cls != NULL) {
                    LLVMTypeRef self_ptr_t = LLVMPointerType(
                        cls->struct_type, 0);
                    LLVMValueRef sa = llvm_create_entry_alloca(
                        ctx, self_ptr_t, "self.addr");
                    LLVMBuildStore(ctx->builder, self_val, sa);
                    llvm_scope_declare(ctx, "self", sa, self_ptr_t);
                    llvm_register_var_class(ctx, "self", decl_name);
                } else {
                    LLVMValueRef sa = llvm_create_entry_alloca(
                        ctx, ctx->type_i8ptr, "self.addr");
                    LLVMBuildStore(ctx->builder, self_val, sa);
                    llvm_scope_declare(ctx, "self", sa, ctx->type_i8ptr);
                }

                /* User params */
                size_t pc = method->data.func_decl.param_count;
                unsigned lpidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    const char *type_name = NULL;
                    LLVMClassTypeEntry *param_cls = NULL;
                    LLVMTypeRef pt;
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    if (p == NULL || p->name == NULL) {
                        lpidx++;
                        continue;
                    }
                    if (p->type != NULL && p->type->type == AST_TYPE)
                        type_name = p->type->data.type.name;
                    param_cls = type_name != NULL
                        ? llvm_lookup_class(ctx, type_name) : NULL;
                    if (param_cls != NULL && param_cls->is_pointer_self_host)
                        pt = LLVMPointerType(param_cls->struct_type, 0);
                    else
                        pt = (p->type != NULL)
                            ? ast_type_to_llvm(ctx, p->type)
                            : ctx->type_i32;
                    LLVMValueRef a = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, lpidx++), a);
                    llvm_scope_declare(ctx, p->name, a, pt);
                    if (type_name != NULL && param_cls != NULL)
                        llvm_register_var_class(ctx, p->name, type_name);
                }

                if (has_sync) {
                    LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                        LLVMPointerType(cls->struct_type, 0),
                        llvm_scope_lookup(ctx, "self")->alloca,
                        llvm_tmp_name(ctx));
                    LLVMValueRef sync_args[] = { self_ptr };
                    LLVMBuildCall2(ctx->builder, sync_entry->fn_type,
                        sync_entry->fn, sync_args, 1, "");
                }

                {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing routine for domain method '%s.%s'",
                        decl_name != NULL ? decl_name : "(anonymous-domain)",
                        method->data.func_decl.name != NULL
                            ? method->data.func_decl.name
                            : "(anonymous)");
                    return false;
                }

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (stmt->type == AST_WORLD_DECL && cls != NULL) {
                        for (size_t k = 0; k < stmt->data.world_decl.zone_count;
                             k++) {
                            ASTNode *zone = stmt->data.world_decl.zones[k];
                            char dirty_field[256];
                            int dirty_idx;
                            LLVMValueRef self_ptr;
                            LLVMValueRef dirty_ptr;
                            const char *slot_name = zone != NULL
                                ? zone->data.world_zone.slot_name
                                : NULL;
                            if (slot_name == NULL)
                                continue;
                            snprintf(dirty_field, sizeof(dirty_field),
                                "__zone_dirty_%s", slot_name);
                            dirty_idx = llvm_class_field_index(cls, dirty_field);
                            if (dirty_idx < 0)
                                continue;
                            self_ptr = LLVMBuildLoad2(ctx->builder,
                                LLVMPointerType(cls->struct_type, 0),
                                llvm_scope_lookup(ctx, "self")->alloca,
                                llvm_tmp_name(ctx));
                            dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                                cls->struct_type, self_ptr,
                                (unsigned)dirty_idx, llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder,
                                LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
                        }
                        int derived_idx =
                            llvm_class_field_index(cls, "__world_derived_dirty");
                        if (derived_idx >= 0) {
                            LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                                LLVMPointerType(cls->struct_type, 0),
                                llvm_scope_lookup(ctx, "self")->alloca,
                                llvm_tmp_name(ctx));
                            LLVMValueRef derived_ptr = LLVMBuildStructGEP2(
                                ctx->builder, cls->struct_type, self_ptr,
                                (unsigned)derived_idx, llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder,
                                LLVMConstInt(ctx->type_i1, 1, 0), derived_ptr);
                        }
                    }
                    if (has_sync) {
                        LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                            LLVMPointerType(cls->struct_type, 0),
                            llvm_scope_lookup(ctx, "self")->alloca,
                            llvm_tmp_name(ctx));
                        LLVMValueRef sync_args[] = { self_ptr };
                        LLVMBuildCall2(ctx->builder, sync_entry->fn_type,
                            sync_entry->fn, sync_args, 1, "");
                    }
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;
                llvm_restore_current_host_decl(ctx, saved_host_decl);

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last =
                        LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */
