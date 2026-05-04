/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend: intent observability trace emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

void
llvm_emit_intent_trace_step(LLVMGenCtx *ctx,
                            LLVMFuncEntry *trace_step_fn,
                            LLVMValueRef handle_alloca,
                            const char *step_name,
                            const char *zone_type_name)
{
    LLVMValueRef handle;
    LLVMValueRef args[3];

    if (ctx == NULL || trace_step_fn == NULL)
        return;

    handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
        handle_alloca, llvm_tmp_name(ctx));
    args[0] = handle;
    args[1] = LLVMBuildGlobalStringPtr(ctx->builder,
        step_name != NULL ? step_name : "<step>",
        llvm_tmp_name(ctx));
    args[2] = LLVMBuildGlobalStringPtr(ctx->builder,
        zone_type_name != NULL ? zone_type_name : "<zone>",
        llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, trace_step_fn->fn_type,
        trace_step_fn->fn, args, 3, "");
}

void
llvm_emit_intent_trace_bindings(LLVMGenCtx *ctx,
                                ASTNode *intent,
                                LLVMFuncEntry *trace_bind_fn,
                                LLVMValueRef handle_alloca,
                                const LLVMIntentStepContext *step_ctx)
{
    if (ctx == NULL || intent == NULL || trace_bind_fn == NULL || step_ctx == NULL)
        return;

    for (size_t j = 0; j < step_ctx->who_alias_count; j++) {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        const char *alias = step_ctx->who_aliases[j];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
            ctx, intent, step_ctx->zone_type_name, alias);
        LLVMValueRef args[] = {
            handle,
            LLVMBuildGlobalStringPtr(ctx->builder,
                alias != NULL ? alias : "<participant>",
                llvm_tmp_name(ctx)),
            LLVMBuildGlobalStringPtr(ctx->builder,
                slot_name != NULL ? slot_name : "<unbound>",
                llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, trace_bind_fn->fn_type,
            trace_bind_fn->fn, args, 3, "");
    }
}

void
llvm_emit_intent_trace_step_ok(LLVMGenCtx *ctx,
                               LLVMFuncEntry *trace_step_ok_fn,
                               LLVMValueRef handle_alloca,
                               const char *step_name)
{
    LLVMValueRef handle;
    LLVMValueRef args[2];

    if (ctx == NULL || trace_step_ok_fn == NULL)
        return;

    handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
        handle_alloca, llvm_tmp_name(ctx));
    args[0] = handle;
    args[1] = LLVMBuildGlobalStringPtr(ctx->builder,
        step_name != NULL ? step_name : "<step>",
        llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, trace_step_ok_fn->fn_type,
        trace_step_ok_fn->fn, args, 2, "");
}

void
llvm_emit_intent_trace_failure(LLVMGenCtx *ctx,
                               LLVMFuncEntry *trace_fail_fn,
                               LLVMValueRef handle_alloca,
                               LLVMValueRef fail_reason_alloca)
{
    LLVMValueRef handle;
    LLVMValueRef reason;
    LLVMValueRef args[2];

    if (ctx == NULL || trace_fail_fn == NULL)
        return;

    handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
        handle_alloca, llvm_tmp_name(ctx));
    reason = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
        fail_reason_alloca, llvm_tmp_name(ctx));
    args[0] = handle;
    args[1] = reason;
    LLVMBuildCall2(ctx->builder, trace_fail_fn->fn_type,
        trace_fail_fn->fn, args, 2, "");
}

#endif
