#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <string.h>

#include "codegen_hashmap_key_policy.h"
#include "codegen_slot_type_policy.h"
#include "llvm_expr_call_queue_extended.h"
#include "llvm_internal_api.h"
#include "llvm_expr_call_collections_map_exports.h"

static bool
llvm_collection_extended_error_out(LLVMGenCtx *ctx, ASTNode *node,
                                   LLVMValueRef *out, LLVMValueRef recovery,
                                   const char *message)
{
    (void)recovery;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM collection extended builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_collection_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                                   const char *callee_name,
                                   LLVMValueRef *out)
{
    if (out == NULL)
        return false;
    *out = NULL;

    if (llvm_emit_queue_extended_call(node, ctx, callee_name, out))
        return true;

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
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
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
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
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
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
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
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
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
    if (strcmp(callee_name, "MapSet") == 0 && argc == 3) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (map_var == NULL)
            return true;
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        value = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (key == NULL || value == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM MapSet could not lower key or value expression");
        if (LLVMTypeOf(value) != value_ty) {
            if ((value_ty == ctx->type_i32 || value_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
            else if ((value_ty == ctx->type_f32 || value_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
        }
        if (value_name != NULL && strcmp(value_name, "String") == 0) {
            fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
                callee_name, "set", key_name);
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM MapSet could not allocate value temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "set", key_name);
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            key,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }
    if (strcmp(callee_name, "MapGet") == 0 && argc == 2) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (map_var == NULL)
            return true;
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (key == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM MapGet could not lower key expression");
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstNull(value_ty),
                "LLVM MapGet could not allocate result temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);
        if (value_name != NULL && strcmp(value_name, "String") == 0) {
            fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
                callee_name, "get", key_name);
            if (fn == NULL)
                { *out = NULL; return true; }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx)); return true; }
        }
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "get", key_name);
        if (fn == NULL)
            { *out = NULL; return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            key,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (strcmp(callee_name, "MapHas") == 0 && argc == 2) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstInt(ctx->type_i1, 0, 0), out);
        if (map_var == NULL)
            return true;
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        key = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "has", key_name);
        if (key == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i1, 0, 0),
                "LLVM MapHas could not lower key expression");
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (strcmp(callee_name, "MapRemove") == 0 && argc == 2) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (map_var == NULL)
            return true;
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (key == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM MapRemove could not lower key expression");
        if (value_name != NULL && strcmp(value_name, "String") == 0) {
            fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
                callee_name, "remove", key_name);
            if (fn == NULL)
                { *out = NULL; return true; }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            }
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "remove", key_name);
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }
    if (strcmp(callee_name, "MapSize") == 0 && argc == 1) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (map_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_map_size_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (strcmp(callee_name, "MapKeys") == 0 && argc == 1) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMTypeRef array_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        map_var = llvm_collection_required_receiver_var(ctx, node, map_arg,
            callee_name, "HashMap", LLVMConstNull(ctx->array_type_String), out);
        if (map_var == NULL)
            return true;
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        array_ty = llvm_hashmap_key_array_type(ctx, key_name);
        tmp = llvm_create_entry_alloca(ctx, array_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                LLVMConstNull(array_ty),
                "LLVM MapKeys could not allocate key array temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_ty), tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "keys", key_name);
        if (fn == NULL)
            { *out = NULL; return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        { *out = LLVMBuildLoad2(ctx->builder, array_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (pgy_codegen_call_name_is_slot_source(callee_name)
        && argc == 1) {
        { *out = llvm_emit_expression(ast_call_argument(node, 0), ctx); return true; }
    }
    return false;
}
#endif
