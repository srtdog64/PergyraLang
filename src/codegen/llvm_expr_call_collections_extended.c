#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <string.h>

#include "codegen_hashmap_key_policy.h"
#include "codegen_slot_type_policy.h"
#include "llvm_expr_call_queue_extended.h"
#include "llvm_internal_api.h"

static LLVMFuncEntry *
llvm_required_hashmap_raw_export(LLVMGenCtx *ctx,
                                 ASTNode *node,
                                 const char *callee_name,
                                 const char *operation,
                                 const char *key_name)
{
    char export_name[64];
    LLVMFuncEntry *fn;

    if (!pgy_hashmap_key_raw_export_name(operation, key_name,
            export_name, sizeof(export_name))) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM collection operation '%s' requires stable HashMap<Int|String, T> key metadata",
                callee_name != NULL ? callee_name : "HashMap operation");
        }
        return NULL;
    }

    fn = llvm_lookup_function(ctx, export_name);
    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "HashMap operation",
            export_name);
    }
    return fn;
}

LLVMTypeRef
llvm_collection_required_value_type(LLVMGenCtx *ctx, ASTNode *node,
                                    const char *collection_kind,
                                    const char *var_name,
                                    const char *type_name,
                                    LLVMValueRef *out)
{
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation requires concrete element/value type metadata for '%s'",
                collection_kind != NULL ? collection_kind : "collection",
                var_name != NULL ? var_name : "<collection>");
        }
        if (out != NULL && ctx != NULL)
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, type_name);
}

LLVMFuncEntry *
llvm_required_collection_function(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  const char *callee_name,
                                  const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "collection operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMTypeRef
llvm_hashmap_key_array_type(LLVMGenCtx *ctx, const char *key_name)
{
    switch (pgy_hashmap_key_kind_from_name(key_name)) {
    case PGY_HASHMAP_KEY_INT:
        return ctx->array_type_Int;
    case PGY_HASHMAP_KEY_LONG:
        return ctx->array_type_Long;
    case PGY_HASHMAP_KEY_BOOL:
        return ctx->array_type_Bool;
    case PGY_HASHMAP_KEY_STRING:
    case PGY_HASHMAP_KEY_UNKNOWN:
        return ctx->array_type_String;
    }
    return ctx->array_type_String;
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

    if (strcmp(callee_name, "ListPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
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
            "pgy_list_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (strcmp(callee_name, "ListGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_get_raw_export");
        if (fn == NULL)
            { *out = LLVMConstNull(elem_ty); return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }

    if (strcmp(callee_name, "ListSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (idx == NULL || value == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
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
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_set_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (strcmp(callee_name, "ListSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_size_raw_export");
        if (fn == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    if (strcmp(callee_name, "ListRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            list_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (strcmp(callee_name, "MapSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (key == NULL || value == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        if (LLVMTypeOf(value) != value_ty) {
            if ((value_ty == ctx->type_i32 || value_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
            else if ((value_ty == ctx->type_f32 || value_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "set", key_name);
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (strcmp(callee_name, "MapGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (key == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "get", key_name);
        if (fn == NULL)
            { *out = LLVMConstNull(value_ty); return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            key,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }

    if (strcmp(callee_name, "MapHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i1, 0, 0); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "has", key_name);
        if (map_var == NULL || key == NULL || fn == NULL)
            { *out = LLVMConstInt(ctx->type_i1, 0, 0); return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, llvm_tmp_name(ctx)); return true; }
        }
    }

    if (strcmp(callee_name, "MapRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        value_ty = llvm_collection_required_value_type(ctx, node, "HashMap",
            map_arg->data.identifier.name, value_name, out);
        if (value_ty == NULL)
            return true;
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "remove", key_name);
        if (key == NULL || fn == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
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

    if (strcmp(callee_name, "MapSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_map_size_raw_export");
        if (fn == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    if (strcmp(callee_name, "MapKeys") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMTypeRef array_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstNull(ctx->array_type_String); return true; }
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        array_ty = llvm_hashmap_key_array_type(ctx, key_name);
        if (map_var == NULL)
            { *out = LLVMConstNull(array_ty); return true; }
        tmp = llvm_create_entry_alloca(ctx, array_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_ty), tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "keys", key_name);
        if (fn == NULL)
            { *out = LLVMConstNull(array_ty); return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        { *out = LLVMBuildLoad2(ctx->builder, array_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }

    if (pgy_codegen_call_name_is_slot_source(callee_name)
        && node->data.call.arg_count == 1) {
        { *out = llvm_emit_expression(node->data.call.arguments[0], ctx); return true; }
    }

    /* Built-in: StringLength(s) -> call strlen */

    return false;
}

#endif
