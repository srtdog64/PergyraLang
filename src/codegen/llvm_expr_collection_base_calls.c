#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_collection_base_calls.h"

#include <string.h>

#include "llvm_expr_call_collections_extended.h"
#include "llvm_internal_api.h"

bool
llvm_emit_collection_base_call(ASTNode *node, LLVMGenCtx *ctx,
                               const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "ListNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef list_ty;
        LLVMTypeRef elem_ty;
        const char *inner_name = NULL;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (ctx->current_ret_type == NULL
            || LLVMGetTypeKind(ctx->current_ret_type) != LLVMStructTypeKind) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM ListNew() requires contextual List<T>; implicit i32 fallback is disabled");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "List<", 5) == 0) {
            inner_name = llvm_constructed_arg_name_at(ctx->expected_type_name, 0);
        }
        if (inner_name == NULL || inner_name[0] == '\0'
            || strcmp(inner_name, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM ListNew() requires concrete List<T> type metadata");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        list_ty = ctx->current_ret_type;
        elem_ty = pergyra_type_to_llvm(ctx, inner_name);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_new_raw_export");
        tmp = llvm_create_entry_alloca(ctx, list_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(list_ty), tmp);
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMBuildLoad2(ctx->builder, list_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "SetNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef set_ty;
        LLVMTypeRef elem_ty;
        const char *inner_name = NULL;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (ctx->current_ret_type == NULL
            || LLVMGetTypeKind(ctx->current_ret_type) != LLVMStructTypeKind) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM SetNew() requires contextual Set<T>; implicit i32 fallback is disabled");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "Set<", 4) == 0) {
            inner_name = llvm_constructed_arg_name_at(ctx->expected_type_name, 0);
        }
        if (inner_name == NULL || inner_name[0] == '\0'
            || strcmp(inner_name, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM SetNew() requires concrete Set<T> type metadata");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        set_ty = ctx->current_ret_type;
        elem_ty = pergyra_type_to_llvm(ctx, inner_name);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_new_raw_export");
        tmp = llvm_create_entry_alloca(ctx, set_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(set_ty), tmp);
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "SetAdd") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_add_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "SetHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i1, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_has_raw_export");
        if (fn == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    if (strcmp(callee_name, "SetRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "SetSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_size_raw_export");
        if (fn == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    return false;
}

#endif
