#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <string.h>

#include "llvm_internal_api.h"

bool
llvm_emit_list_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                             const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    size_t argc = ast_call_arg_count(node);

    if (strcmp(callee_name, "ListPush") == 0 && argc == 2) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        list_var = llvm_collection_required_receiver_var(ctx, node, list_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (list_var == NULL)
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListPush could not lower value expression");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_push_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListPush could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_push_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }
    if (strcmp(callee_name, "ListGet") == 0 && argc == 2) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        list_var = llvm_collection_required_receiver_var(ctx, node, list_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (list_var == NULL)
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (idx == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListGet could not lower index expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstNull(elem_ty),
                "LLVM ListGet could not allocate result temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_get_string_raw_export");
            if (fn == NULL)
                { *out = NULL; return true; }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
        }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_get_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (strcmp(callee_name, "ListSet") == 0 && argc == 3) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        list_var = llvm_collection_required_receiver_var(ctx, node, list_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (list_var == NULL)
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        value = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (idx == NULL || value == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListSet could not lower index or value expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_set_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListSet could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_set_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }
    if (strcmp(callee_name, "ListSize") == 0 && argc == 1) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry *list_var;
        LLVMFuncEntry *fn;
        list_var = llvm_collection_required_receiver_var(ctx, node, list_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (list_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_size_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (strcmp(callee_name, "ListRemove") == 0 && argc == 2) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMFuncEntry *fn;
        list_var = llvm_collection_required_receiver_var(ctx, node, list_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (list_var == NULL)
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (idx == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListRemove could not lower index expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_remove_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_remove_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }
    return false;
}
#endif
