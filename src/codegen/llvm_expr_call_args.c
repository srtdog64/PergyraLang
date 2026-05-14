/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM expression call argument lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <limits.h>
#include <stdint.h>

LLVMValueRef
llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                             ASTNode **arg_nodes, size_t argc)
{
    LLVMValueRef *args = NULL;
    LLVMValueRef result;

    if (ctx == NULL || func == NULL)
        return NULL;

    if (argc > (size_t)UINT_MAX || argc > SIZE_MAX / sizeof(LLVMValueRef)) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            "LLVM call helper argument count exceeds backend ABI limits for '%s'",
            LLVMGetValueName(func->fn));
        return NULL;
    }

    if (argc > 0) {
        if (arg_nodes == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM call helper requires argument AST payloads for '%s'",
                LLVMGetValueName(func->fn));
            return NULL;
        }
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        if (args == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM call helper could not allocate argument storage for '%s'",
                LLVMGetValueName(func->fn));
            return NULL;
        }
        for (size_t i = 0; i < argc; i++) {
            args[i] = llvm_emit_expression(arg_nodes[i], ctx);
            if (args[i] == NULL) {
                llvm_set_error_at_with_hints(ctx, arg_nodes[i],
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM call helper could not lower argument %zu for '%s'",
                    i, LLVMGetValueName(func->fn));
                return NULL;
            }
        }
    }

    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, (unsigned)argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, (unsigned)argc, llvm_tmp_name(ctx));
    }

    return result;
}

#endif /* PGY_LLVM_ENABLED */
