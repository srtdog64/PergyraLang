/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM expression emission support routines.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_emit_support.h"

LLVMValueRef
llvm_expression_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx == NULL)
        return NULL;
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM expression lowering requires complete metadata");
    }
    return NULL;
}

LLVMValueRef
llvm_zero_value_for_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    if (ctx == NULL || type == NULL)
        return NULL;

    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind == LLVMVoidTypeKind)
        return NULL;
    if (kind == LLVMIntegerTypeKind)
        return LLVMConstInt(type, 0, 0);
    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind)
        return LLVMConstReal(type, 0.0);
    return LLVMConstNull(type);
}

bool
llvm_expr_runtime_name(LLVMGenCtx *ctx,
                       ASTNode *node,
                       char *out,
                       size_t out_size,
                       const char *prefix,
                       const char *type_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s%s",
        prefix != NULL ? prefix : "",
        type_name != NULL ? type_name : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM expression runtime symbol is too long for type '%s'",
            type_name != NULL ? type_name : "<type>");
    }
    return false;
}

bool
llvm_expr_lambda_name(LLVMGenCtx *ctx,
                      ASTNode *node,
                      char *out,
                      size_t out_size,
                      int lambda_id)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "pgy_lambda_%d", lambda_id);
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM lambda function name is too long for id %d",
            lambda_id);
    }
    return false;
}

#endif
