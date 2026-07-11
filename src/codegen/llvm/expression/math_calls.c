/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM scalar math call lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "codegen/llvm_internal.h"
#include "math_calls.h"
#include "parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

typedef enum LLVMMathOp {
    LLVM_MATH_OP_NONE = 0,
    LLVM_MATH_OP_ABS,
    LLVM_MATH_OP_CHECKED_ADD,
    LLVM_MATH_OP_CHECKED_MUL,
    LLVM_MATH_OP_CLAMP,
    LLVM_MATH_OP_E,
    LLVM_MATH_OP_MAX,
    LLVM_MATH_OP_MIN,
    LLVM_MATH_OP_PI,
} LLVMMathOp;

typedef struct LLVMMathSpec {
    const char *name;
    size_t argc;
    LLVMMathOp op;
} LLVMMathSpec;

static int
llvm_math_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMMathSpec *spec = (const LLVMMathSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMMathOp
llvm_math_lookup(const char *callee_name, size_t argc)
{
    static const LLVMMathSpec kLLVMMathSpecs[] = {
        { "Abs", 1, LLVM_MATH_OP_ABS },
        { "CheckedAdd", 2, LLVM_MATH_OP_CHECKED_ADD },
        { "CheckedMul", 2, LLVM_MATH_OP_CHECKED_MUL },
        { "Clamp", 3, LLVM_MATH_OP_CLAMP },
        { "E", 0, LLVM_MATH_OP_E },
        { "Max", 2, LLVM_MATH_OP_MAX },
        { "Min", 2, LLVM_MATH_OP_MIN },
        { "PI", 0, LLVM_MATH_OP_PI },
    };
    const LLVMMathSpec *match;

    if (callee_name == NULL)
        return LLVM_MATH_OP_NONE;

    match = (const LLVMMathSpec *)bsearch(&callee_name, kLLVMMathSpecs,
        sizeof(kLLVMMathSpecs) / sizeof(kLLVMMathSpecs[0]),
        sizeof(kLLVMMathSpecs[0]), llvm_math_spec_compare);
    if (match == NULL || match->argc != argc)
        return LLVM_MATH_OP_NONE;
    return match->op;
}

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
    size_t argc;
    LLVMMathOp op;

    if (out == NULL)
        return false;

    argc = ast_call_arg_count(node);
    op = llvm_math_lookup(callee_name, argc);

    if (op == LLVM_MATH_OP_ABS) {
        LLVMValueRef x = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (x == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Abs could not lower operand expression");
        LLVMTypeRef ty = LLVMTypeOf(x);
        LLVMTypeKind tk = LLVMGetTypeKind(ty);
        if (tk == LLVMFloatTypeKind || tk == LLVMDoubleTypeKind) {
            LLVMValueRef zero = LLVMConstReal(ty, 0.0);
            LLVMValueRef neg = LLVMBuildFNeg(ctx->builder, x, llvm_tmp_name(ctx));
            LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, x, zero,
                                              llvm_tmp_name(ctx));
            *out = LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef zero = LLVMConstInt(ty, 0, 0);
            LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                              llvm_tmp_name(ctx));
            *out = LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
        }
        return true;
    }

    if (op == LLVM_MATH_OP_MIN) {
        LLVMValueRef a = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef b = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (a == NULL || b == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Min could not lower operand expression");
        LLVMTypeKind tk = LLVMGetTypeKind(LLVMTypeOf(a));
        LLVMValueRef cmp;
        if (tk == LLVMFloatTypeKind || tk == LLVMDoubleTypeKind) {
            cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, a, b,
                                llvm_tmp_name(ctx));
        } else {
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                llvm_tmp_name(ctx));
        }
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_MATH_OP_MAX) {
        LLVMValueRef a = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef b = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (a == NULL || b == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Max could not lower operand expression");
        LLVMTypeKind tk = LLVMGetTypeKind(LLVMTypeOf(a));
        LLVMValueRef cmp;
        if (tk == LLVMFloatTypeKind || tk == LLVMDoubleTypeKind) {
            cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, a, b,
                                llvm_tmp_name(ctx));
        } else {
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                llvm_tmp_name(ctx));
        }
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_MATH_OP_CHECKED_ADD || op == LLVM_MATH_OP_CHECKED_MUL) {
        LLVMValueRef a = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef b = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (a == NULL || b == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM CheckedAdd/CheckedMul could not lower operand expression");
        /* The semantic layer requires Int operands (the builtins lower to the
         * i32 overflow-checked helpers). Anything else reaching this point is a
         * front-end regression: fail closed with a diagnostic instead of the
         * silent i64->i32 truncation this branch used to perform. */
        if (LLVMTypeOf(a) != ctx->type_i32 || LLVMTypeOf(b) != ctx->type_i32)
            return llvm_math_error_out(ctx, node, out,
                "LLVM CheckedAdd/CheckedMul requires Int (i32) operands; "
                "a non-Int operand leaked past the semantic layer");
        const char *helper = (op == LLVM_MATH_OP_CHECKED_ADD)
            ? "pgy_checked_add_i32_export" : "pgy_checked_mul_i32_export";
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, helper);
        if (fn == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM checked arithmetic requires registered runtime helper");
        LLVMValueRef args[2];
        args[0] = a;
        args[1] = b;
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_MATH_OP_CLAMP) {
        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef lo = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        LLVMValueRef hi = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (val == NULL || lo == NULL || hi == NULL)
            return llvm_math_error_out(ctx, node, out,
                "LLVM Clamp could not lower operand expression");
        LLVMTypeKind tk = LLVMGetTypeKind(LLVMTypeOf(val));
        LLVMValueRef below;
        LLVMValueRef above;
        if (tk == LLVMFloatTypeKind || tk == LLVMDoubleTypeKind) {
            below = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, val, lo,
                                  llvm_tmp_name(ctx));
            above = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, val, hi,
                                  llvm_tmp_name(ctx));
        } else {
            below = LLVMBuildICmp(ctx->builder, LLVMIntSLT, val, lo,
                                  llvm_tmp_name(ctx));
            above = LLVMBuildICmp(ctx->builder, LLVMIntSGT, val, hi,
                                  llvm_tmp_name(ctx));
        }
        LLVMValueRef bounded_hi = LLVMBuildSelect(ctx->builder, above, hi, val,
                                                  llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, below, lo, bounded_hi,
                               llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_MATH_OP_PI) {
        *out = LLVMConstReal(ctx->type_f32, 3.14159265358979323846);
        return true;
    }

    if (op == LLVM_MATH_OP_E) {
        *out = LLVMConstReal(ctx->type_f32, 2.71828182845904523536);
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
