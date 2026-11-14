#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

bool
llvm_param_is_implicit_self_local(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

static bool
llvm_domain_provenance_field_name(char *out,
                                  size_t out_size,
                                  const char *suffix,
                                  const char *prefix,
                                  const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL
        || prefix == NULL || name == NULL)
        return false;
    written = snprintf(out, out_size, "__%s_%s_%s", prefix, suffix, name);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_stamp_domain_provenance(LLVMGenCtx *ctx,
                             LLVMClassTypeEntry *decl_cls,
                             LLVMValueRef self_ptr,
                             const char *prefix,
                             const char *name,
                             unsigned cause)
{
    char field_name[256];
    int field_idx;

    if (ctx == NULL || decl_cls == NULL || self_ptr == NULL
        || prefix == NULL || name == NULL) {
        return;
    }

    if (!llvm_domain_provenance_field_name(field_name, sizeof(field_name),
            "epoch", prefix, name))
        return;
    field_idx = llvm_class_field_index(decl_cls, field_name);
    if (field_idx >= 0) {
        LLVMValueRef epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
            decl_cls->struct_type, self_ptr, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMValueRef epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            epoch_ptr, llvm_tmp_name(ctx));
        LLVMValueRef next_epoch = LLVMBuildAdd(ctx->builder, epoch_val,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, next_epoch, epoch_ptr);
    }

    if (!llvm_domain_provenance_field_name(field_name, sizeof(field_name),
            "cause", prefix, name))
        return;
    field_idx = llvm_class_field_index(decl_cls, field_name);
    if (field_idx >= 0) {
        LLVMValueRef cause_ptr = LLVMBuildStructGEP2(ctx->builder,
            decl_cls->struct_type, self_ptr, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, cause, 0),
            cause_ptr);
    }
}

#endif /* PGY_LLVM_ENABLED */
