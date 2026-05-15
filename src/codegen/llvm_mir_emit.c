/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR function emission split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_mir_vars.h"
#include "llvm_mir_phi.h"
#include "llvm_mir_type_helpers.h"

static void
llvm_mir_debug_stage(const char *stage, const MIRRoutine *routine)
{
    if (getenv("PGY_DEBUG_LLVM_STAGE") == NULL || stage == NULL)
        return;
    fprintf(stderr, "[llvm stage] %s", stage);
    if (routine != NULL && routine->name != NULL)
        fprintf(stderr, ":%s", routine->name);
    fputc('\n', stderr);
}

static void
llvm_mir_mark_owner_dirty_for_exit(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *owner_cls,
                                   const char *owner_name)
{
    LLVMVarEntry *self_entry;
    LLVMValueRef self_ptr;

    if (ctx == NULL || owner_cls == NULL || owner_name == NULL)
        return;

    self_entry = llvm_scope_lookup(ctx, "self");
    if (self_entry == NULL)
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(owner_cls->struct_type, 0),
        self_entry->alloca, llvm_tmp_name(ctx));

    if (owner_cls->domain_kind == LLVM_DOMAIN_WORLD) {
        for (int i = 0; i < owner_cls->field_count; i++) {
            const char *field_name = owner_cls->fields[i].field_name;
            LLVMValueRef dirty_ptr;

            if (field_name == NULL
                || strncmp(field_name, "__zone_dirty_", 13) != 0) {
                continue;
            }

            dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                owner_cls->struct_type, self_ptr,
                (unsigned)owner_cls->fields[i].index,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }

        {
            int derived_idx = llvm_class_field_index(owner_cls, "__world_derived_dirty");
            if (derived_idx >= 0) {
                LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                    owner_cls->struct_type, self_ptr, (unsigned)derived_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                    derived_ptr);
            }
        }
    }
}

void
llvm_mir_emit_owner_sync_exit(LLVMGenCtx *ctx,
                              LLVMClassTypeEntry *owner_cls,
                              LLVMFuncEntry *owner_sync,
                              const char *owner_name)
{
    LLVMVarEntry *self_entry;
    LLVMValueRef self_ptr;
    LLVMValueRef sync_args[1];

    if (ctx == NULL || owner_cls == NULL || owner_sync == NULL)
        return;

    llvm_mir_mark_owner_dirty_for_exit(ctx, owner_cls, owner_name);

    self_entry = llvm_scope_lookup(ctx, "self");
    if (self_entry == NULL)
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(owner_cls->struct_type, 0),
        self_entry->alloca, llvm_tmp_name(ctx));
    sync_args[0] = self_ptr;
    LLVMBuildCall2(ctx->builder, owner_sync->fn_type, owner_sync->fn,
        sync_args, 1, "");
}

#include "llvm_mir_block_emit.h"
#include "llvm_mir_local_emit.h"

LLVMValueRef
llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx)
{
    size_t param_count = 0;
    bool is_intent = false;
    bool is_method = false;
    const char *owner_name = NULL;
    LLVMClassTypeEntry *owner_cls = NULL;
    LLVMFuncEntry *owner_sync = NULL;
    const char *fn_name = NULL;
    char qualified_name[256];
    if (routine == NULL || ctx == NULL || routine->ast == NULL)
        return NULL;
    llvm_mir_debug_stage("emit_func_from_mir:begin", routine);

    ASTNode *func_decl = routine->ast;
    if (func_decl == NULL
        || (func_decl->type != AST_FUNC_DECL && func_decl->type != AST_INTENT_DECL))
        return NULL;
    if (!llvm_mir_validate_cleanup_contract(routine, ctx))
        return NULL;

    is_intent = (func_decl->type == AST_INTENT_DECL);
    is_method = (!is_intent && routine->kind == MIR_SCOPE_METHOD);
    owner_name = routine->owner_name;
    if (is_method && owner_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "MIR-only LLVM path missing owner metadata for method '%s'",
            routine->name != NULL ? routine->name : "(anonymous)");
        return NULL;
    }
    owner_cls = (is_method && owner_name != NULL)
        ? llvm_lookup_class(ctx, owner_name)
        : NULL;
    if (is_method && owner_cls != NULL
        && owner_cls->sync_function_name != NULL
        && owner_cls->domain_kind != LLVM_DOMAIN_NONE
        && owner_cls->domain_kind != LLVM_DOMAIN_SYSTEMIC) {
        owner_sync = llvm_lookup_function(ctx, owner_cls->sync_function_name);
    }
    param_count = is_intent
        ? (ast_intent_decl_binding_count(func_decl) > 0
            ? ast_intent_decl_binding_count(func_decl)
            : (ast_intent_decl_involve_count(func_decl)
                + ast_intent_decl_value_count(func_decl)))
        : (is_method ? 1 : 0);
    if (!is_intent) {
        for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
            FuncParam *p = ast_func_param(func_decl, i);
            if (is_method && p != NULL && p->type == NULL
                && p->name != NULL && strcmp(p->name, "self") == 0) {
                continue;
            }
            bool is_secure_slot = false;
            if (llvm_mir_boundary_slot_inner_name(ctx, p, &is_secure_slot) != NULL)
                param_count += is_secure_slot ? 2 : 1;
            else
                param_count++;
        }
    }
    LLVMTypeRef *param_types = pgy_arena_calloc(&ctx->scratch,
        (param_count > 0 ? param_count : 1) * sizeof(LLVMTypeRef));
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            size_t binding_count = 0;
            size_t involve_count = 0;
            size_t value_count = 0;
            ASTNode **bindings = ast_intent_decl_bindings(func_decl, &binding_count);
            ASTNode **involves = ast_intent_decl_involves(func_decl, &involve_count);
            ASTNode **values = ast_intent_decl_values(func_decl, &value_count);
            ASTNode *binding = binding_count > 0
                ? (i < binding_count ? bindings[i] : NULL)
                : (i < involve_count
                    ? involves[i]
                    : (i - involve_count < value_count
                        ? values[i - involve_count]
                        : NULL));
            ASTNode *binding_type = NULL;
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                && ast_intent_involves_subject_type(binding) != NULL) {
                binding_type = ast_intent_involves_subject_type(binding);
                param_types[i] = llvm_mir_type_from_ast(ctx, binding_type);
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
                if (llvm_intent_involves_uses_pointer_self(ctx, binding))
                    param_types[i] = LLVMPointerType(param_types[i], 0);
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                && ast_intent_value_type(binding) != NULL) {
                binding_type = ast_intent_value_type(binding);
                param_types[i] = llvm_mir_type_from_ast(ctx, binding_type);
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
            } else {
                param_types[i] = llvm_mir_required_type_from_ast(
                    ctx, binding, NULL, "intent binding");
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
            }
        } else if (is_method && i == 0) {
            if (owner_cls != NULL) {
                param_types[i] = owner_cls->is_pointer_self_host
                    ? LLVMPointerType(owner_cls->struct_type, 0)
                    : owner_cls->struct_type;
            } else {
                param_types[i] = ctx->type_i8ptr;
            }
        } else {
            size_t logical_index = is_method ? (i - 1) : i;
            size_t seen = 0;
            FuncParam *p = NULL;
            for (size_t param_index = 0;
                 param_index < ast_func_param_count(func_decl);
                 param_index++) {
                FuncParam *candidate = ast_func_param(func_decl, param_index);
                if (candidate != NULL
                    && candidate->type == NULL
                    && candidate->name != NULL
                    && strcmp(candidate->name, "self") == 0) {
                    continue;
                }
                if (seen == logical_index) {
                    p = candidate;
                    break;
                }
                seen++;
            }
            bool is_secure_slot = false;
            const char *slot_inner = llvm_mir_boundary_slot_inner_name(ctx, p, &is_secure_slot);
            if (slot_inner != NULL && p != NULL && p->type != NULL) {
                LLVMTypeRef slot_ty = llvm_mir_type_from_ast(ctx, p->type);
                if (ctx->has_error || slot_ty == NULL)
                    return NULL;
                param_types[i] = LLVMPointerType(slot_ty, 0);
                if (is_secure_slot && i + 1 < param_count) {
                    param_types[++i] = llvm_secure_token_type(ctx, slot_inner);
                }
            } else {
                if (p != NULL && p->type != NULL)
                    param_types[i] = llvm_mir_required_type_from_ast(
                        ctx, func_decl, p->type, "function parameter");
                else
                    param_types[i] = llvm_mir_required_type_from_ast(
                        ctx, func_decl, NULL, "function parameter");
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
                if (p != NULL && p->type != NULL
                    && llvm_mir_param_uses_pointer_self(ctx, p->type)) {
                    param_types[i] = LLVMPointerType(param_types[i], 0);
                }
            }
        }
    }
    LLVMTypeRef ret_type = is_intent ? ctx->type_i1 : ctx->type_i32;
    if (!is_intent && ast_func_return_type(func_decl) != NULL)
        ret_type = llvm_mir_type_from_ast(ctx, ast_func_return_type(func_decl));
    if (ctx->has_error || ret_type == NULL)
        return NULL;

    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    fn_name = routine->name;
    if (is_method && owner_name != NULL && routine->name != NULL) {
        snprintf(qualified_name, sizeof(qualified_name), "%s_%s", owner_name, routine->name);
        fn_name = qualified_name;
    }
    LLVMFuncEntry *entry = llvm_lookup_or_declare_function(ctx, fn_name, func_type, ret_type);
    LLVMValueRef fn = entry != NULL ? entry->fn : NULL;
    if (fn == NULL)
        return NULL;
    llvm_mir_debug_stage("emit_func_from_mir:fn_ready", routine);
    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    ASTNode *saved_func_decl = ctx->current_func_decl;
    int saved_slot_var_count = ctx->slot_var_count;
    int saved_view_var_count = ctx->view_var_count;
    int saved_device_slot_var_count = ctx->device_slot_var_count;
    int saved_future_var_count = ctx->future_var_count;
    int saved_channel_var_count = ctx->channel_var_count;
    int saved_var_class_count = ctx->var_class_count;
    int saved_projection_borrow_count = ctx->projection_borrow_count;
    int saved_array_var_count = ctx->array_var_count;
    int saved_list_var_count = ctx->list_var_count;
    int saved_set_var_count = ctx->set_var_count;
    int saved_queue_var_count = ctx->queue_var_count;
    int saved_map_var_count = ctx->map_var_count;
    int saved_callable_var_count = ctx->callable_var_count;
    ASTNode *saved_host_decl = NULL;
    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    ctx->current_func_decl = func_decl;
    if (is_method)
        saved_host_decl = llvm_bind_current_host_decl(
            ctx, llvm_find_host_decl_in_active_inventory(ctx, owner_name));

    size_t var_capacity = 64;
    LLVMMirVar *vars = pgy_arena_calloc(&ctx->scratch,
        var_capacity * sizeof(LLVMMirVar));
    size_t var_count = 0;

    LLVMBasicBlockRef *llvm_blocks = pgy_arena_calloc(&ctx->scratch,
        (routine->block_count > 0 ? routine->block_count : 1)
            * sizeof(LLVMBasicBlockRef));
    for (size_t i = 0; i < routine->block_count; i++) {
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "bb_%zu", i);
        llvm_blocks[i] = LLVMAppendBasicBlockInContext(ctx->context, fn, bb_name);
    }
    llvm_mir_debug_stage("emit_func_from_mir:blocks_ready", routine);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    llvm_scope_push(ctx);
    llvm_defer_scope_push(ctx);
    llvm_emit_mir_param_allocas(routine, func_decl, fn, ctx, is_intent, is_method,
                                owner_cls, owner_name, param_count);
    llvm_emit_mir_local_allocas(routine, ctx, &vars, &var_capacity, &var_count);
    llvm_mir_debug_stage("emit_func_from_mir:locals_ready", routine);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    if (owner_sync != NULL) {
        LLVMVarEntry *self_entry = llvm_scope_lookup(ctx, "self");
        if (self_entry != NULL) {
            LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                LLVMPointerType(owner_cls->struct_type, 0),
                self_entry->alloca, llvm_tmp_name(ctx));
            LLVMValueRef sync_args[] = { self_ptr };
            LLVMBuildCall2(ctx->builder, owner_sync->fn_type, owner_sync->fn,
                sync_args, 1, "");
        }
    }

    if (routine->entry_block < routine->block_count) {
        llvm_emit_mir_block_with_exprs(&routine->blocks[routine->entry_block], routine, ctx,
                                       llvm_blocks, vars, var_count, func_decl,
                                       owner_cls, owner_sync, owner_name);
    }
    llvm_mir_debug_stage("emit_func_from_mir:entry_emitted", routine);
    for (size_t i = 0; i < routine->block_count; i++) {
        if (i == routine->entry_block)
            continue;
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (mir_block->is_reachable && !mir_block->is_cleanup) {
            llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks,
                                           vars, var_count, func_decl,
                                           owner_cls, owner_sync, owner_name);
        } else if (!mir_block->is_cleanup) {
            LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[i]);
            LLVMBuildUnreachable(ctx->builder);
        }
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (!mir_block->is_reachable || mir_block->is_cleanup)
            continue;
        LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[i]);
        if (LLVMGetBasicBlockTerminator(llvm_blocks[i]) != NULL)
            continue;
        if (mir_block->has_succ_true) {
            LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
        } else if (ret_type == ctx->type_void) {
            LLVMBuildRetVoid(ctx->builder);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstNull(ret_type));
        }
    }
    llvm_mir_emit_true_phi_nodes(routine, ctx, llvm_blocks, vars, var_count);
    llvm_mir_debug_stage("emit_func_from_mir:blocks_emitted", routine);

    if (routine->has_cleanup_block) {
        for (size_t i = 0; i < routine->block_count; i++) {
            const MIRBasicBlock *mir_block = &routine->blocks[i];
            if (mir_block->is_cleanup && mir_block->is_reachable) {
                llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks,
                                               vars, var_count, func_decl,
                                               owner_cls, owner_sync, owner_name);
            }
        }
    }
    llvm_mir_debug_stage("emit_func_from_mir:cleanup_emitted", routine);

    llvm_defer_scope_pop(ctx);
    llvm_scope_pop(ctx);
    ctx->slot_var_count = saved_slot_var_count;
    ctx->view_var_count = saved_view_var_count;
    ctx->device_slot_var_count = saved_device_slot_var_count;
    ctx->future_var_count = saved_future_var_count;
    ctx->channel_var_count = saved_channel_var_count;
    ctx->var_class_count = saved_var_class_count;
    ctx->projection_borrow_count = saved_projection_borrow_count;
    ctx->array_var_count = saved_array_var_count;
    ctx->list_var_count = saved_list_var_count;
    ctx->set_var_count = saved_set_var_count;
    ctx->queue_var_count = saved_queue_var_count;
    ctx->map_var_count = saved_map_var_count;
    ctx->callable_var_count = saved_callable_var_count;
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_func_decl = saved_func_decl;
    if (is_method)
        llvm_restore_current_host_decl(ctx, saved_host_decl);
    llvm_mir_debug_stage("emit_func_from_mir:return", routine);
    return fn;
}

#endif
