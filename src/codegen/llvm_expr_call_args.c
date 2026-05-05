/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM expression call argument lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef
llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                             ASTNode **arg_nodes, size_t argc)
{
    LLVMValueRef *args = NULL;
    LLVMValueRef result;

    if (ctx == NULL || func == NULL)
        return NULL;

    if (argc > 0) {
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(arg_nodes[i], ctx);
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
