/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_string_coerce.h"

#include "llvm_internal_api.h"

LLVMValueRef
llvm_coerce_value_to_string(LLVMValueRef value, LLVMGenCtx *ctx)
{
    LLVMTypeRef value_type;
    LLVMFuncEntry *fn;
    LLVMValueRef args[1];

    if (value == NULL || ctx == NULL)
        return NULL;

    value_type = LLVMTypeOf(value);
    if (value_type == ctx->type_i8ptr)
        return value;

    if (LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
        unsigned width = LLVMGetIntTypeWidth(value_type);
        if (width == 1) {
            LLVMValueRef true_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "true", llvm_tmp_name(ctx));
            LLVMValueRef false_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "false", llvm_tmp_name(ctx));
            return LLVMBuildSelect(ctx->builder, value, true_str, false_str,
                llvm_tmp_name(ctx));
        } else if (width < 32) {
            value = LLVMBuildSExt(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        } else if (width > 32) {
            value = LLVMBuildTrunc(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        }

        fn = llvm_lookup_function(ctx, "pgy_int_to_string");
        if (fn == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, NULL,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM string coercion requires registered runtime function '%s'",
                    "pgy_int_to_string");
            }
            return NULL;
        }
        args[0] = value;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
            llvm_tmp_name(ctx));
    }

    if (value_type == ctx->type_f32 || value_type == ctx->type_f64) {
        if (value_type == ctx->type_f64) {
            value = LLVMBuildFPTrunc(ctx->builder, value, ctx->type_f32,
                llvm_tmp_name(ctx));
        }
        fn = llvm_lookup_function(ctx, "pgy_float_to_string");
        if (fn == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, NULL,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM string coercion requires registered runtime function '%s'",
                    "pgy_float_to_string");
            }
            return NULL;
        }
        args[0] = value;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
            llvm_tmp_name(ctx));
    }

    return NULL;
}

#endif
