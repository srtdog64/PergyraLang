/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM scalar math call lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_expr_math_calls.h"
#include "parser/ast_api.h"

#include <string.h>

static bool
llvm_math_error_out(LLVMGenCtx *ctx, ASTNode *node,
                    LLVMValueRef *out, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM math builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_scalar_math_call(ASTNode *node, LLVMGenCtx *ctx,
                           const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "Abs") == 0 && ast_call_arg_count(node) == 1) {
        LLVMValueRef x = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (x == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Abs could not lower operand expression");
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "Min") == 0 && ast_call_arg_count(node) == 2) {
        LLVMValueRef a = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef b = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (a == NULL || b == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Min could not lower operand expression");
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "Max") == 0 && ast_call_arg_count(node) == 2) {
        LLVMValueRef a = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef b = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (a == NULL || b == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Max could not lower operand expression");
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
