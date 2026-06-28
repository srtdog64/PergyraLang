#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_aggregate.h"

#include <string.h>

#include "llvm_expr_emit_support.h"
#include "llvm_internal_api.h"

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
        LLVMVarEntry arr_var;
        bool has_arr_var = llvm_scope_lookup_snapshot(ctx, name, &arr_var);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
        if (has_arr_var && entry != NULL) {
            const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
            bool use_raw_nominal = (suffix == NULL
                || strcmp(suffix, "Unknown") == 0)
                && entry->elem_name != NULL && entry->elem_name[0] != '\0';
            if ((suffix != NULL && strcmp(suffix, "Unknown") != 0)
                || use_raw_nominal) {
                const char *struct_name = LLVMGetStructName(arr_var.type);
                LLVMValueRef aggregate = LLVMBuildLoad2(ctx->builder,
                    arr_var.type, arr_var.alloca, llvm_tmp_name(ctx));
                LLVMValueRef inlined = llvm_emit_inline_array_get(ctx,
                    aggregate, entry->elem_type, idx, struct_name);
                if (inlined != NULL)
                    return inlined;
                if (ctx->has_error)
                    return NULL;
                if (use_raw_nominal) {
                    LLVMFuncEntry *raw_get_fn = llvm_required_runtime_function(
                        ctx, node, "indexed collection access", "ArrayGet",
                        "pgy_array_get_raw_export");
                    LLVMValueRef index64 = idx;
                    LLVMValueRef elem_size;
                    LLVMValueRef out_alloca;
                    LLVMValueRef args[4];
                    if (raw_get_fn == NULL)
                        return NULL;
                    if (LLVMTypeOf(index64) != ctx->type_i64)
                        index64 = LLVMBuildSExtOrBitCast(ctx->builder,
                            index64, ctx->type_i64, llvm_tmp_name(ctx));
                    elem_size = LLVMSizeOf(entry->elem_type);
                    if (LLVMTypeOf(elem_size) != ctx->type_i64)
                        elem_size = LLVMBuildZExtOrBitCast(ctx->builder,
                            elem_size, ctx->type_i64, llvm_tmp_name(ctx));
                    out_alloca = llvm_create_entry_alloca(ctx,
                        entry->elem_type, llvm_tmp_name(ctx));
                    args[0] = LLVMBuildBitCast(ctx->builder, arr_var.alloca,
                        ctx->type_i8ptr, llvm_tmp_name(ctx));
                    args[1] = index64;
                    args[2] = LLVMBuildBitCast(ctx->builder, out_alloca,
                        ctx->type_i8ptr, llvm_tmp_name(ctx));
                    args[3] = elem_size;
                    LLVMBuildCall2(ctx->builder, raw_get_fn->fn_type,
                        raw_get_fn->fn, args, 4, "");
                    return LLVMBuildLoad2(ctx->builder, entry->elem_type,
                        out_alloca, llvm_tmp_name(ctx));
                }
                {
                    const char *fn_prefix = "pgy_array_get_";
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    LLVMValueRef index64 = idx;
                    LLVMValueRef args[2];
                    if (struct_name != NULL
                        && strncmp(struct_name, "PgySlice_", 9) == 0) {
                        fn_prefix = "pgy_slice_get_";
                    }
                    if (!llvm_expr_runtime_name(ctx, node, fn_name,
                            sizeof(fn_name), fn_prefix, suffix))
                        return NULL;
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        if (LLVMTypeOf(index64) != ctx->type_i64)
                            index64 = LLVMBuildSExtOrBitCast(ctx->builder,
                                index64, ctx->type_i64, llvm_tmp_name(ctx));
                        args[0] = arr_var.alloca;
                        args[1] = index64;
                        return LLVMBuildCall2(ctx->builder, fn->fn_type,
                            fn->fn, args, 2, llvm_tmp_name(ctx));
                    }
                    llvm_required_runtime_function(ctx, node,
                        "indexed collection access",
                        struct_name != NULL
                            && strncmp(struct_name, "PgySlice_", 9) == 0
                            ? "SliceGet" : "ArrayGet",
                        fn_name);
                    return NULL;
                }
            }
            return llvm_expression_error(ctx, node,
                "LLVM indexed collection access requires concrete Array<T>/Slice<T> element metadata");
        }
    }

    {
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
            LLVMValueRef data_ptr;
            LLVMTypeRef elem_ty;
            LLVMValueRef gep;
            if (checked != NULL)
                return checked;
            if (ctx->has_error)
                return NULL;

            data_ptr = llvm_array_data_ptr(ctx, arr);
            elem_ty = llvm_stmt_resolve_array_elem_type(
                ctx, array_node, data_ptr);
            if (elem_ty == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM aggregate array access requires concrete element metadata");
            gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
    }
    return llvm_expression_error(ctx, node,
        "LLVM array access receiver is not an array, slice, string, or pointer");
}

#endif /* PGY_LLVM_ENABLED */
