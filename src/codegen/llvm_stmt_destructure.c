#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

void
llvm_emit_let_destructure_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    /* let (a, b, c) = expr;
     * Two shapes supported:
     *   1) Tuple: struct { T0, T1, ... } -> ExtractValue per field.
     *   2) Array-like: struct { T* data, i64 size, i64 cap } -> GEP + load. */
    ASTNode *init = ast_let_destructure_initializer(node);
    if (init == NULL)
        return;
    LLVMValueRef rhs_val = llvm_emit_expression(init, ctx);
    if (rhs_val == NULL)
        return;
    LLVMTypeRef rhs_ty = LLVMTypeOf(rhs_val);
    if (LLVMGetTypeKind(rhs_ty) != LLVMStructTypeKind) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "destructuring requires an Array-like or tuple struct initializer");
        return;
    }

    /* Heuristic: tuple if struct field count equals the binding count
     * AND the first field is not a pointer (array-like has pointer as
     * the first field for `data`). */
    unsigned field_count = LLVMCountStructElementTypes(rhs_ty);
    bool is_tuple = false;
    if (field_count == (unsigned)ast_let_destructure_name_count(node)) {
        LLVMTypeRef f0 = LLVMStructGetTypeAtIndex(rhs_ty, 0);
        if (f0 != NULL && LLVMGetTypeKind(f0) != LLVMPointerTypeKind)
            is_tuple = true;
    }

    if (is_tuple) {
        for (size_t i = 0; i < ast_let_destructure_name_count(node); i++) {
            const char *bname = ast_let_destructure_name(node, i);
            if (bname == NULL) continue;
            LLVMTypeRef ft = LLVMStructGetTypeAtIndex(rhs_ty, (unsigned)i);
            LLVMValueRef v = LLVMBuildExtractValue(ctx->builder, rhs_val,
                (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef alloca = llvm_create_entry_alloca(ctx, ft, bname);
            LLVMBuildStore(ctx->builder, v, alloca);
            llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, ft);
        }
        return;
    }

    /* Array-like path (unchanged) */
    LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder,
        rhs_val, 0, llvm_tmp_name(ctx));
    LLVMTypeRef elem_type = llvm_stmt_resolve_array_elem_type(
        ctx, init, data_ptr);
    if (elem_type == NULL)
        return;
    for (size_t i = 0; i < ast_let_destructure_name_count(node); i++) {
        const char *bname = ast_let_destructure_name(node, i);
        if (bname == NULL)
            continue;
        LLVMValueRef idx = LLVMConstInt(ctx->type_i64,
            (unsigned long long)i, 0);
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            elem_type, data_ptr, &idx, 1, llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildLoad2(ctx->builder, elem_type,
            gep, llvm_tmp_name(ctx));
        LLVMValueRef alloca = llvm_create_entry_alloca(
            ctx, elem_type, bname);
        LLVMBuildStore(ctx->builder, val, alloca);
        llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, elem_type);
    }
}
#endif /* PGY_LLVM_ENABLED */
