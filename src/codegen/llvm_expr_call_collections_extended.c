#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <string.h>

#include "codegen_hashmap_key_policy.h"
#include "codegen_slot_type_policy.h"
#include "llvm_expr_call_queue_extended.h"
#include "llvm_internal_api.h"
#include "llvm_expr_call_collections_map_exports.h"

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

    if (llvm_emit_list_extended_call(node, ctx, callee_name, out))
        return true;
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
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
        value_name = llvm_lookup_map_value(ctx, ast_identifier_name(map_arg));
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            ast_identifier_name(map_arg), value_name, out);
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
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
        value_name = llvm_lookup_map_value(ctx, ast_identifier_name(map_arg));
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            ast_identifier_name(map_arg), value_name, out);
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
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
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
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
        value_name = llvm_lookup_map_value(ctx, ast_identifier_name(map_arg));
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            ast_identifier_name(map_arg), value_name, out);
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
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
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
