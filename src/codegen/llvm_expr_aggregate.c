#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_aggregate.h"

#include <string.h>

#include "llvm_expr_emit_support.h"

LLVMValueRef
llvm_emit_tuple_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t n = ast_tuple_literal_count(node);
    if (n == 0)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
        n * sizeof(LLVMValueRef));
    LLVMTypeRef *tys = pgy_arena_calloc(&ctx->scratch,
        n * sizeof(LLVMTypeRef));
    if (vals == NULL || tys == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM tuple literal allocation failed");

    for (size_t i = 0; i < n; i++) {
        vals[i] = llvm_emit_expression(ast_tuple_literal_element(node, i), ctx);
        if (vals[i] == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM tuple literal could not lower element %zu",
                    i);
            }
            return NULL;
        }
        tys[i] = LLVMTypeOf(vals[i]);
    }

    LLVMTypeRef tup_ty = LLVMStructTypeInContext(ctx->context, tys,
        (unsigned)n, 0);
    LLVMValueRef agg = LLVMGetUndef(tup_ty);
    for (size_t i = 0; i < n; i++)
        agg = LLVMBuildInsertValue(ctx->builder, agg, vals[i],
            (unsigned)i, llvm_tmp_name(ctx));
    return agg;
}

LLVMValueRef
llvm_emit_array_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = ast_array_literal_count(node);
    const char *inner_name = NULL;
    char inner_name_buf[256];
    LLVMTypeRef elem_type = ctx->type_i32;
    LLVMValueRef first_value = NULL;

    if (count > 0) {
        first_value = llvm_emit_expression(ast_array_literal_element(node, 0),
            ctx);
        if (first_value == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM array literal could not lower element 0");
        elem_type = LLVMTypeOf(first_value);
        const char *suffix = llvm_type_to_suffix(ctx, elem_type);
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            inner_name = suffix;
    } else if (ctx->expected_type_name != NULL
               && pgy_classify_type(ctx->expected_type_name)
                    == PGY_TK_ARRAY) {
        if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
                inner_name_buf, sizeof(inner_name_buf))) {
            inner_name = inner_name_buf;
        }
    }

    if (inner_name == NULL || inner_name[0] == '\0'
        || strcmp(inner_name, "Unknown") == 0) {
        llvm_expr_set_missing_type_error(ctx, node,
            "array literal expression");
        return NULL;
    }

    LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
    if (ctx->has_error || array_type == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array literal could not lower Array<T> type");

    LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type,
        llvm_tmp_name(ctx));
    if (tmp == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array literal could not allocate array temporary");

    char push_fn_name[64];
    if (!llvm_expr_runtime_name(ctx, node, push_fn_name,
            sizeof(push_fn_name), "pgy_array_push_", inner_name))
        return NULL;
    LLVMFuncEntry *push_fn = llvm_lookup_function(ctx, push_fn_name);
    if (push_fn == NULL && count > 0) {
        llvm_required_runtime_function(ctx, node,
            "array literal expression", "ArrayPush", push_fn_name);
        return NULL;
    }

    LLVMBuildStore(ctx->builder, LLVMConstNull(array_type), tmp);
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef elem = i == 0 ? first_value
            : llvm_emit_expression(ast_array_literal_element(node, i), ctx);
        if (elem == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM array literal could not lower element %zu",
                    i);
            }
            return NULL;
        }
        LLVMValueRef args[] = { tmp, elem };
        LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn, args, 2, "");
    }
    return LLVMBuildLoad2(ctx->builder, array_type, tmp, llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_emit_array_access_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *array_node = ast_array_access_array(node);
    LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
    LLVMValueRef idx = llvm_emit_expression(ast_array_access_index(node), ctx);
    if (arr == NULL || idx == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array access could not lower receiver or index expression");

    if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(array_node);
        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
        if (arr_var != NULL && entry != NULL) {
            const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
            if (suffix != NULL && strcmp(suffix, "Unknown") != 0) {
                const char *struct_name = LLVMGetStructName(arr_var->type);
                const char *fn_prefix = "pgy_array_get_";
                char fn_name[64];
                if (struct_name != NULL
                    && strncmp(struct_name, "PgySlice_", 9) == 0) {
                    fn_prefix = "pgy_slice_get_";
                }
                if (!llvm_expr_runtime_name(ctx, node, fn_name,
                        sizeof(fn_name), fn_prefix, suffix))
                    return NULL;
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    LLVMValueRef index64 = idx;
                    if (LLVMTypeOf(index64) != ctx->type_i64)
                        index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                            ctx->type_i64, llvm_tmp_name(ctx));
                    LLVMValueRef args[] = { arr_var->alloca, index64 };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
                llvm_required_runtime_function(ctx, node,
                    "indexed collection access",
                    struct_name != NULL
                        && strncmp(struct_name, "PgySlice_", 9) == 0
                        ? "SliceGet" : "ArrayGet",
                    fn_name);
                return NULL;
            }
            return llvm_expression_error(ctx, node,
                "LLVM indexed collection access requires concrete Array<T>/Slice<T> element metadata");
        }
    }

    LLVMTypeRef arr_ty = LLVMTypeOf(arr);
    if (arr_ty == ctx->type_i8ptr) {
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            LLVMInt8TypeInContext(ctx->context),
            arr, &idx, 1, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder,
            LLVMInt8TypeInContext(ctx->context),
            gep, llvm_tmp_name(ctx));
    }

    if (LLVMGetTypeKind(arr_ty) == LLVMPointerTypeKind) {
        LLVMTypeRef elem_ty = LLVMGetElementType(arr_ty);
        if (elem_ty != NULL) {
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, arr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
    }

    if (LLVMGetTypeKind(arr_ty) == LLVMStructTypeKind) {
        const char *struct_name = LLVMGetStructName(arr_ty);
        LLVMValueRef checked = llvm_emit_checked_collection_get(
            ctx, arr, arr_ty, idx, struct_name);
        if (checked != NULL)
            return checked;
        if (ctx->has_error)
            return NULL;

        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
        LLVMTypeRef elem_ty = llvm_stmt_resolve_array_elem_type(
            ctx, array_node, data_ptr);
        if (elem_ty == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM aggregate array access requires concrete element metadata");
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, elem_ty,
            gep, llvm_tmp_name(ctx));
    }
    return llvm_expression_error(ctx, node,
        "LLVM array access receiver is not an array, slice, string, or pointer");
}

#endif /* PGY_LLVM_ENABLED */
