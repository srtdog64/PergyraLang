#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

void
llvm_expr_set_missing_type_error(LLVMGenCtx *ctx, ASTNode *node,
                                 const char *surface)
{
    if (ctx == NULL || ctx->has_error)
        return;
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM %s requires concrete type metadata; add an explicit type annotation before using it in expression context",
        surface != NULL ? surface : "expression");
}

LLVMValueRef
llvm_emit_checked_collection_get(LLVMGenCtx *ctx, LLVMValueRef aggregate,
                                 LLVMTypeRef aggregate_type,
                                 LLVMValueRef index,
                                 const char *struct_name)
{
    const char *fn_prefix = NULL;
    const char *suffix = NULL;
    char fn_name[64];
    LLVMFuncEntry *fn;
    LLVMValueRef tmp;
    LLVMValueRef index64;
    LLVMValueRef args[2];

    if (ctx == NULL || aggregate == NULL || aggregate_type == NULL
        || index == NULL || struct_name == NULL)
        return NULL;

    if (strncmp(struct_name, "PgyArray_", 9) == 0) {
        fn_prefix = "pgy_array_get_";
        suffix = struct_name + 9;
    } else if (strncmp(struct_name, "PgySlice_", 9) == 0) {
        fn_prefix = "pgy_slice_get_";
        suffix = struct_name + 9;
    } else {
        return NULL;
    }

    snprintf(fn_name, sizeof(fn_name), "%s%s", fn_prefix, suffix);
    fn = llvm_lookup_function(ctx, fn_name);
    if (fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM indexed collection access requires registered runtime function '%s'",
            fn_name);
        return NULL;
    }

    tmp = llvm_create_entry_alloca(ctx, aggregate_type, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, aggregate, tmp);
    index64 = index;
    if (LLVMTypeOf(index64) != ctx->type_i64)
        index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
            ctx->type_i64, llvm_tmp_name(ctx));
    args[0] = tmp;
    args[1] = index64;
    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2,
        llvm_tmp_name(ctx));
}

#endif /* PGY_LLVM_ENABLED */
