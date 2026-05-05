/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM aggregate expression utilities.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const char *
llvm_call_arg_device_inner(LLVMGenCtx *ctx, ASTNode *node)
{
    if (node != NULL && node->type == AST_IDENTIFIER)
        return llvm_lookup_device_slot_inner(ctx, node->data.identifier.name);
    return NULL;
}

LLVMValueRef
llvm_array_data_ptr(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 0,
                                 llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_array_length_i64(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 1,
                                 llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_build_option_value(LLVMGenCtx *ctx, LLVMTypeRef inner_ty,
                        LLVMValueRef has_value, LLVMValueRef value)
{
    LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
        (LLVMTypeRef[]){ ctx->type_i32, inner_ty }, 2, 0);
    LLVMValueRef tag = LLVMBuildSelect(ctx->builder, has_value,
        LLVMConstInt(ctx->type_i32, 0, 0),
        LLVMConstInt(ctx->type_i32, 1, 0),
        llvm_tmp_name(ctx));
    LLVMValueRef option = LLVMGetUndef(option_ty);
    option = LLVMBuildInsertValue(ctx->builder, option, tag, 0,
                                  llvm_tmp_name(ctx));
    option = LLVMBuildInsertValue(ctx->builder, option, value, 1,
                                  llvm_tmp_name(ctx));
    return option;
}

#endif /* PGY_LLVM_ENABLED */
