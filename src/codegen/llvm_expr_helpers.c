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
    LLVMTypeRef elem_type;
    LLVMValueRef inlined;

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

    /*
     * Prefer the inline element load for arrays so the optimizer can hoist the
     * invariant base and length out of loops; fall through to the runtime call
     * for slices or when the element type cannot be resolved.
     */
    if (strncmp(struct_name, "PgyArray_", 9) == 0) {
        elem_type = pergyra_type_to_llvm(ctx, suffix);
        if (elem_type != NULL) {
            inlined = llvm_emit_inline_array_get(ctx, aggregate, elem_type,
                index, struct_name);
            if (inlined != NULL)
                return inlined;
            if (ctx->has_error)
                return NULL;
        }
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

/*
 * Emit a bounds-checked array element load as inline IR instead of calling the
 * opaque pgy_array_get_T runtime entrypoint. That runtime function is an
 * external symbol the LLVM optimizer cannot inline, so it blocks hoisting of
 * the invariant base/length and leaves the bounds branch in every iteration.
 * The inline form lets LICM and SCEV reason about the access and matches the
 * runtime semantics: an out-of-range index (unsigned compare against the
 * length) traps through the cold out-of-bounds panic and does not return.
 *
 * The aggregate must be the loaded array struct value. Returns NULL without
 * setting an error when it cannot inline (non-array struct, missing panic
 * runtime, or no active insertion block) so the caller can fall back to the
 * runtime call.
 */
LLVMValueRef
llvm_emit_inline_array_get(LLVMGenCtx *ctx, LLVMValueRef aggregate,
                          LLVMTypeRef elem_type, LLVMValueRef index,
                          const char *struct_name)
{
    LLVMValueRef current_fn;
    LLVMFuncEntry *panic_fn;
    LLVMBasicBlockRef insert_block;
    LLVMValueRef data_ptr;
    LLVMValueRef length;
    LLVMValueRef index64;
    LLVMValueRef oob;
    LLVMValueRef elem_ptr;
    LLVMValueRef reason_arg;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef ok_bb;

    if (ctx == NULL || aggregate == NULL || elem_type == NULL || index == NULL)
        return NULL;
    if (struct_name != NULL && strncmp(struct_name, "PgyArray_", 9) != 0)
        return NULL;

    insert_block = LLVMGetInsertBlock(ctx->builder);
    if (insert_block == NULL)
        return NULL;
    current_fn = LLVMGetBasicBlockParent(insert_block);
    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_out_of_bounds_export");
    if (current_fn == NULL || panic_fn == NULL)
        return NULL;

    data_ptr = llvm_array_data_ptr(ctx, aggregate);
    length = llvm_array_length_i64(ctx, aggregate);
    if (data_ptr == NULL || length == NULL)
        return NULL;

    index64 = index;
    if (LLVMTypeOf(index64) != ctx->type_i64)
        index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64, ctx->type_i64,
            llvm_tmp_name(ctx));

    oob = LLVMBuildICmp(ctx->builder, LLVMIntUGE, index64, length,
        llvm_tmp_name(ctx));
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "array.index.panic");
    ok_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "array.index.ok");
    LLVMBuildCondBr(ctx->builder, oob, fail_bb, ok_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
        "array index out of bounds", llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
        &reason_arg, 1, "");
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    elem_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, data_ptr,
        &index64, 1, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, elem_type, elem_ptr, llvm_tmp_name(ctx));
}

#endif /* PGY_LLVM_ENABLED */
