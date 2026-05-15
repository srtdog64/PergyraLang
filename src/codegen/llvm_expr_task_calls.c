/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_task_calls.h"

#include <string.h>

#include "llvm_internal_api.h"

bool
llvm_is_task_runtime_builtin_name(const char *callee_name)
{
    return callee_name != NULL
        && (strcmp(callee_name, "Cancel") == 0
            || strcmp(callee_name, "IsCancelled") == 0);
}

static LLVMFuncEntry *
llvm_required_task_function(LLVMGenCtx *ctx, ASTNode *node,
                            const char *callee_name,
                            const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;
    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s requires registered task runtime function '%s'",
            callee_name != NULL ? callee_name : "task operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMValueRef
llvm_task_runtime_error(LLVMGenCtx *ctx, ASTNode *node,
                        const char *callee_name,
                        const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s %s",
            callee_name != NULL ? callee_name : "task operation",
            message != NULL ? message : "could not be lowered");
    }
    return NULL;
}

LLVMValueRef
llvm_emit_task_runtime_call(ASTNode *node, LLVMGenCtx *ctx,
                            const char *callee_name)
{
    size_t argc = ast_call_arg_count(node);

    if (strcmp(callee_name, "Cancel") == 0 && argc == 1) {
        LLVMFuncEntry *fn = llvm_required_task_function(ctx, node, callee_name,
            "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (fn == NULL)
            return NULL;
        if (task == NULL)
            return llvm_task_runtime_error(ctx, node, callee_name,
                "could not lower task handle expression");

        LLVMValueRef args[] = { task };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && argc == 0) {
        LLVMFuncEntry *fn = llvm_required_task_function(ctx, node, callee_name,
            "pgy_task_is_cancelled_export");
        if (fn == NULL)
            return NULL;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            NULL, 0, llvm_tmp_name(ctx));
    }

    return llvm_task_runtime_error(ctx, node, callee_name,
        "has unsupported arity for the LLVM task builtin");
}

#endif
