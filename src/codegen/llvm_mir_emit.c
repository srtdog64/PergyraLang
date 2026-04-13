/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR function emission split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

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

typedef struct {
    const char *mir_name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
} LLVMMirVar;

static LLVMValueRef
llvm_mir_get_var(LLVMMirVar *vars, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (vars[i].mir_name && strcmp(vars[i].mir_name, name) == 0)
            return vars[i].alloca;
    }
    return NULL;
}

static LLVMTypeRef
llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return ctx->type_i32;
    LLVMTypeRef type = ast_type_to_llvm(ctx, type_node);
    return type != NULL ? type : ctx->type_i32;
}

static bool
llvm_mir_param_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node)
{
    LLVMClassTypeEntry *cls;

    if (ctx == NULL || type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return false;
    }

    cls = llvm_lookup_class(ctx, type_node->data.type.name);
    return cls != NULL && cls->is_pointer_self_host;
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

static void
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

#include "llvm_mir_blocks.inc"
#include "llvm_mir_locals.inc"

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

    is_intent = (func_decl->type == AST_INTENT_DECL);
    is_method = (!is_intent && routine->kind == MIR_SCOPE_METHOD);
    owner_name = routine->owner_name;
    if (is_method && owner_name == NULL) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "MIR-only LLVM path missing owner metadata for method '%s'",
                 routine->name != NULL ? routine->name : "(anonymous)");
        llvm_set_error(ctx, msg);
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
        ? (func_decl->data.intent_decl.involve_count
            + func_decl->data.intent_decl.value_count)
        : (is_method ? 1 : 0);
    if (!is_intent) {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *p = func_decl->data.func_decl.params[i];
            if (is_method && p != NULL && p->type == NULL
                && p->name != NULL && strcmp(p->name, "self") == 0) {
                continue;
            }
            param_count++;
        }
    }
    LLVMTypeRef *param_types = calloc(param_count > 0 ? param_count : 1, sizeof(LLVMTypeRef));
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            if (i < func_decl->data.intent_decl.involve_count) {
                ASTNode *involves = func_decl->data.intent_decl.involves[i];
                if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
                    param_types[i] = llvm_mir_type_from_ast(
                        ctx, involves->data.intent_involves.subject_type);
                    if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                        param_types[i] = LLVMPointerType(param_types[i], 0);
                } else {
                    param_types[i] = ctx->type_i32;
                }
            } else {
                size_t value_index = i - func_decl->data.intent_decl.involve_count;
                ASTNode *value = func_decl->data.intent_decl.values[value_index];
                if (value != NULL && value->data.intent_value.value_type != NULL)
                    param_types[i] = llvm_mir_type_from_ast(
                        ctx, value->data.intent_value.value_type);
                else
                    param_types[i] = ctx->type_i32;
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
                 param_index < func_decl->data.func_decl.param_count;
                 param_index++) {
                FuncParam *candidate = func_decl->data.func_decl.params[param_index];
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
            if (p != NULL && p->type != NULL)
                param_types[i] = llvm_mir_type_from_ast(ctx, p->type);
            else
                param_types[i] = ctx->type_i32;
            if (p != NULL && p->type != NULL
                && llvm_mir_param_uses_pointer_self(ctx, p->type)) {
                param_types[i] = LLVMPointerType(param_types[i], 0);
            }
        }
    }
    LLVMTypeRef ret_type = is_intent ? ctx->type_i1 : ctx->type_i32;
    if (!is_intent && func_decl->data.func_decl.return_type != NULL)
        ret_type = llvm_mir_type_from_ast(ctx, func_decl->data.func_decl.return_type);

    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    fn_name = routine->name;
    if (is_method && owner_name != NULL && routine->name != NULL) {
        snprintf(qualified_name, sizeof(qualified_name), "%s_%s", owner_name, routine->name);
        fn_name = qualified_name;
    }
    LLVMFuncEntry *entry = llvm_lookup_or_create_function(ctx, fn_name, func_type, ret_type);
    LLVMValueRef fn = entry != NULL ? entry->fn : NULL;
    if (fn == NULL)
        return NULL;
    llvm_mir_debug_stage("emit_func_from_mir:fn_ready", routine);
    free(param_types);

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
    const char *saved_class_name = ctx->current_class_name;
    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    ctx->current_func_decl = func_decl;
    if (is_method)
        ctx->current_class_name = owner_name;

    size_t var_capacity = 64;
    LLVMMirVar *vars = calloc(var_capacity, sizeof(LLVMMirVar));
    size_t var_count = 0;

    LLVMBasicBlockRef *llvm_blocks = calloc(routine->block_count, sizeof(LLVMBasicBlockRef));
    for (size_t i = 0; i < routine->block_count; i++) {
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "bb_%zu", i);
        llvm_blocks[i] = LLVMAppendBasicBlockInContext(ctx->context, fn, bb_name);
    }
    llvm_mir_debug_stage("emit_func_from_mir:blocks_ready", routine);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    llvm_emit_mir_local_allocas(routine, ctx, &vars, &var_capacity, &var_count);
    llvm_mir_debug_stage("emit_func_from_mir:locals_ready", routine);

    llvm_scope_push(ctx);
    llvm_defer_scope_push(ctx);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    llvm_emit_mir_param_allocas(routine, func_decl, fn, ctx, is_intent, is_method,
                                owner_cls, owner_name, param_count);
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
    ctx->current_class_name = saved_class_name;
    free(vars);
    free(llvm_blocks);
    llvm_mir_debug_stage("emit_func_from_mir:return", routine);
    return fn;
}

#endif
