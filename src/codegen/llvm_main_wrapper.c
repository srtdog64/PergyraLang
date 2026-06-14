/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend top-level executable wrapper emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include <string.h>

static bool
llvm_main_requires_thread_pool(const LLVMGenCtx *ctx)
{
    return llvm_active_uses_thread_pool(ctx);
}

static bool
llvm_main_emit_args_init(LLVMGenCtx *ctx,
                         LLVMValueRef argc_value,
                         LLVMValueRef argv_value)
{
    LLVMFuncEntry *args_init_fn = llvm_lookup_function(ctx, "pgy_args_init");

    if (args_init_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM main wrapper requires registered runtime function '%s'",
            "pgy_args_init");
        return false;
    }

    LLVMValueRef args[] = { argc_value, argv_value };
    LLVMBuildCall2(ctx->builder, args_init_fn->fn_type,
                   args_init_fn->fn, args, 2, "");
    return true;
}

static bool
llvm_main_emit_thread_pool_init(LLVMGenCtx *ctx, bool needs_thread_pool)
{
    if (!needs_thread_pool)
        return true;

    LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
                                 "pgy_pool_init_export");
    if (init_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM thread-pool entry requires registered runtime function '%s'",
            "pgy_pool_init_export");
        return false;
    }
    LLVMValueRef args[] = { LLVMConstInt(ctx->type_i64, 4, 0) };
    LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                   init_fn->fn, args, 1, "");
    return true;
}

static bool
llvm_main_emit_event_initializers(LLVMGenCtx *ctx)
{
    int event_count = llvm_event_type_count(ctx);

    for (int i = 0; i < event_count; i++) {
        LLVMEventTypeEntry *evt = llvm_event_type_at(ctx, i);
        char fname[256];
        LLVMValueRef args[1];
        LLVMFuncEntry *init_fn;
        LLVMValueRef gv;

        if (evt == NULL)
            continue;
        snprintf(fname, sizeof(fname), "%s_INIT", evt->event_name);
        init_fn = llvm_lookup_function(ctx, fname);
        gv = LLVMGetNamedGlobal(ctx->module, evt->event_name);
        if (init_fn == NULL || gv == NULL) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM event initialization requires generated event function '%s' and event storage '%s'",
                fname, evt->event_name);
            return false;
        }
        args[0] = gv;
        LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                       init_fn->fn, args, 1, "");
    }
    return true;
}

static void
llvm_main_emit_user_main(LLVMGenCtx *ctx, LLVMFuncEntry *main_user)
{
    if (main_user != NULL)
        LLVMBuildCall2(ctx->builder, main_user->fn_type,
                       main_user->fn, NULL, 0, "");
}

static bool
llvm_main_emit_top_level_exec(LLVMGenCtx *ctx, ASTNode *synthetic_func)
{
    LLVMFuncEntry *top_level_entry;

    if (synthetic_func == NULL)
        return true;

    top_level_entry = llvm_lookup_function(ctx, "__pgy_top_level_exec");
    if (top_level_entry != NULL) {
        LLVMBuildCall2(ctx->builder, top_level_entry->fn_type,
                       top_level_entry->fn, NULL, 0, "");
        return true;
    }

    llvm_set_mir_inventory_missing(ctx,
        "MIR-only LLVM path missing emitted top-level executable wrapper '__pgy_top_level_exec'");
    return false;
}

static bool
llvm_main_emit_thread_pool_shutdown(LLVMGenCtx *ctx, bool needs_thread_pool)
{
    if (!needs_thread_pool)
        return true;

    LLVMFuncEntry *shutdown_fn = llvm_lookup_function(ctx,
                                     "pgy_pool_shutdown_export");
    if (shutdown_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM thread-pool exit requires registered runtime function '%s'",
            "pgy_pool_shutdown_export");
        return false;
    }
    LLVMBuildCall2(ctx->builder, shutdown_fn->fn_type,
                   shutdown_fn->fn, NULL, 0, "");
    return true;
}

void
llvm_emit_main_wrapper(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return;

    LLVMFuncEntry *user_lowercase = llvm_lookup_function(ctx, "main");
    const char *active_main_name = llvm_active_main_function_name(ctx);

    if (user_lowercase != NULL) {
        user_lowercase->name = "__pgy_user_main_lowercase";
        LLVMSetValueName(user_lowercase->fn, "__pgy_user_main_lowercase");
    }

    if (!llvm_active_has_mir(ctx)
        && llvm_lookup_function(ctx, "Main") == NULL
        && user_lowercase == NULL) {
        return;
    }

    ASTNode *synthetic_executable_func = NULL;
    bool has_top_level_exec = false;
    bool has_main_function = false;
    bool needs_thread_pool = false;

    LLVMFuncEntry *main_user = NULL;
    const char *emitted_main_name = active_main_name;
    synthetic_executable_func = llvm_active_synthetic_executable_func(ctx);
    has_top_level_exec = llvm_active_has_top_level_exec(ctx);
    has_main_function = llvm_active_has_main_function(ctx)
        || llvm_lookup_function(ctx, "Main") != NULL
        || user_lowercase != NULL;
    needs_thread_pool = llvm_main_requires_thread_pool(ctx);

    if (emitted_main_name != NULL
        && strcmp(emitted_main_name, "main") == 0) {
        emitted_main_name = "__pgy_user_main_lowercase";
    }
    if (has_main_function) {
        if (emitted_main_name != NULL)
            main_user = llvm_lookup_function(ctx, emitted_main_name);
        if (main_user == NULL)
            main_user = llvm_lookup_function(ctx, "Main");
        if (main_user == NULL)
            main_user = user_lowercase;
    }

    if (has_main_function && main_user == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing registered executable function '%s'",
            emitted_main_name != NULL ? emitted_main_name : "Main");
        return;
    }
    if (has_top_level_exec && synthetic_executable_func == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing synthetic top-level executable function '__pgy_top_level_exec'");
        return;
    }

    bool has_top_level = has_top_level_exec
        || has_main_function;
    if (!has_top_level)
        return;

    LLVMTypeRef main_params[] = {
        ctx->type_i32,
        LLVMPointerType(ctx->type_i8ptr, 0),
    };
    LLVMTypeRef main_type = LLVMFunctionType(
        ctx->type_i32, main_params, 2, 0);
    LLVMFuncEntry *main_entry = llvm_lookup_or_declare_function(
        ctx, "main", main_type, ctx->type_i32);
    LLVMValueRef main_fn = main_entry != NULL ? main_entry->fn : NULL;
    if (main_fn == NULL)
        return;

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
    const char *saved_return_type_name = ctx->current_return_type_name;
    ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);
    bool scope_pushed = false;

    LLVMSetLinkage(main_fn, LLVMExternalLinkage);
    ctx->current_function = main_fn;
    ctx->current_ret_type = ctx->type_i32;
    ctx->current_function_ret_type = ctx->type_i32;
    ctx->current_return_type_name = NULL;
    ctx->current_return_callable_type = NULL;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->context, main_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    LLVMValueRef argc_value = LLVMGetParam(main_fn, 0);
    LLVMValueRef argv_value = LLVMGetParam(main_fn, 1);
    LLVMSetValueName(argc_value, "argc");
    LLVMSetValueName(argv_value, "argv");

    if (!llvm_main_emit_args_init(ctx, argc_value, argv_value))
        goto restore_state;

    if (!llvm_main_emit_thread_pool_init(ctx, needs_thread_pool))
        goto restore_state;

    llvm_scope_push(ctx);
    if (ctx->has_error)
        goto restore_state;
    scope_pushed = true;

    if (!llvm_main_emit_event_initializers(ctx))
        goto restore_state;
    if (has_main_function)
        llvm_main_emit_user_main(ctx, main_user);
    if (!llvm_main_emit_top_level_exec(ctx, synthetic_executable_func))
        goto restore_state;

    llvm_scope_pop(ctx);
    scope_pushed = false;

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (!llvm_main_emit_thread_pool_shutdown(ctx, needs_thread_pool))
            goto restore_state;
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
    }
    llvm_mark_function_as_used(ctx, "main");

restore_state:
    if (scope_pushed)
        llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif
