#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_store_coercion.h"

#include "llvm_internal_api.h"

LLVMValueRef
llvm_mir_coerce_value_for_store(LLVMGenCtx *ctx,
                                LLVMValueRef value,
                                LLVMTypeRef target_type)
{
    LLVMTypeRef value_type;
    bool target_is_int;
    bool value_is_int;
    bool target_is_fp;
    bool value_is_fp;

    if (ctx == NULL || value == NULL || target_type == NULL)
        return value;
    value_type = LLVMTypeOf(value);
    if (value_type == target_type)
        return value;

    target_is_int = target_type == ctx->type_i1
        || target_type == ctx->type_i32
        || target_type == ctx->type_i64;
    value_is_int = value_type == ctx->type_i1
        || value_type == ctx->type_i32
        || value_type == ctx->type_i64;
    target_is_fp = target_type == ctx->type_f32
        || target_type == ctx->type_f64;
    value_is_fp = value_type == ctx->type_f32
        || value_type == ctx->type_f64;

    if (target_is_int && value_is_int)
        return LLVMBuildIntCast(ctx->builder, value, target_type,
                                llvm_tmp_name(ctx));
    if (target_is_fp && value_is_int)
        return LLVMBuildSIToFP(ctx->builder, value, target_type,
                               llvm_tmp_name(ctx));
    if (target_is_int && value_is_fp)
        return llvm_build_checked_fptosi(ctx, value, target_type,
                               llvm_tmp_name(ctx));
    if (target_is_fp && value_is_fp)
        return target_type == ctx->type_f64
            ? LLVMBuildFPExt(ctx->builder, value, target_type,
                             llvm_tmp_name(ctx))
            : LLVMBuildFPTrunc(ctx->builder, value, target_type,
                               llvm_tmp_name(ctx));
    return value;
}

#endif
