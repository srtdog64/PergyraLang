/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_host_field.h"

#include "llvm_internal_api.h"

bool
llvm_mir_copy_host_field_to_versioned_local(LLVMGenCtx *ctx,
                                            const char *field_name,
                                            LLVMMirVar *target)
{
    const char *host_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr;
    LLVMValueRef gep;
    LLVMValueRef loaded;
    LLVMTypeRef field_type;
    int field_idx;

    if (ctx == NULL || field_name == NULL || target == NULL
        || target->alloca == NULL || target->type == NULL) {
        return false;
    }

    host_name = llvm_current_host_class_name(ctx);
    if (host_name == NULL)
        return false;
    cls = llvm_lookup_class(ctx, host_name);
    field_idx = cls != NULL ? llvm_class_field_index(cls, field_name) : -1;
    if (field_idx < 0)
        return false;
    field_type = llvm_class_field_type_at_index(cls, field_idx);
    if (field_type == NULL)
        return false;
    base_ptr = llvm_current_self_base_ptr(ctx, cls);
    if (base_ptr == NULL)
        return false;

    gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
    loaded = LLVMBuildLoad2(ctx->builder, field_type, gep, llvm_tmp_name(ctx));
    if (LLVMTypeOf(loaded) != target->type) {
        if ((target->type == ctx->type_i32 || target->type == ctx->type_i64)
            && (LLVMTypeOf(loaded) == ctx->type_i32
                || LLVMTypeOf(loaded) == ctx->type_i64)) {
            loaded = LLVMGetIntTypeWidth(target->type)
                    > LLVMGetIntTypeWidth(LLVMTypeOf(loaded))
                ? LLVMBuildSExt(ctx->builder, loaded, target->type,
                    llvm_tmp_name(ctx))
                : LLVMBuildTrunc(ctx->builder, loaded, target->type,
                    llvm_tmp_name(ctx));
        } else {
            return false;
        }
    }
    LLVMBuildStore(ctx->builder, loaded, target->alloca);
    return true;
}

#endif
