#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <stdlib.h>
#include <string.h>

#include "codegen_hashmap_key_policy.h"
#include "codegen_slot_type_policy.h"
#include "llvm_expr_call_queue_extended.h"
#include "llvm_internal_api.h"
#include "llvm_expr_call_collections_map_exports.h"

typedef enum {
    LLVM_MAP_EXT_NONE = 0,
    LLVM_MAP_EXT_GET,
    LLVM_MAP_EXT_HAS,
    LLVM_MAP_EXT_KEYS,
    LLVM_MAP_EXT_REMOVE,
    LLVM_MAP_EXT_SET,
    LLVM_MAP_EXT_SIZE,
} LLVMMapExtendedOp;

typedef struct {
    const char *name;
    unsigned argc;
    LLVMMapExtendedOp op;
} LLVMMapExtendedSpec;

static const LLVMMapExtendedSpec kMapExtendedSpecs[] = {
    {"MapGet", 2, LLVM_MAP_EXT_GET},
    {"MapHas", 2, LLVM_MAP_EXT_HAS},
    {"MapKeys", 1, LLVM_MAP_EXT_KEYS},
    {"MapRemove", 2, LLVM_MAP_EXT_REMOVE},
    {"MapSet", 3, LLVM_MAP_EXT_SET},
    {"MapSize", 1, LLVM_MAP_EXT_SIZE},
};

static int
llvm_map_extended_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMMapExtendedSpec *spec = (const LLVMMapExtendedSpec *)entry;
    return strcmp(name, spec->name);
}

static LLVMMapExtendedOp
llvm_map_extended_lookup(const char *callee_name, unsigned argc)
{
    const LLVMMapExtendedSpec *spec;

    if (callee_name == NULL)
        return LLVM_MAP_EXT_NONE;
    spec = (const LLVMMapExtendedSpec *)bsearch(
        callee_name,
        kMapExtendedSpecs,
        sizeof(kMapExtendedSpecs) / sizeof(kMapExtendedSpecs[0]),
        sizeof(kMapExtendedSpecs[0]),
        llvm_map_extended_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return LLVM_MAP_EXT_NONE;
    return spec->op;
}

bool
llvm_emit_collection_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                                   const char *callee_name,
                                   LLVMValueRef *out)
{
    LLVMMapExtendedOp op;
    size_t argc;

    if (out == NULL)
        return false;
    *out = NULL;

    if (llvm_emit_queue_extended_call(node, ctx, callee_name, out))
        return true;

    argc = ast_call_arg_count(node);

    if (llvm_emit_list_extended_call(node, ctx, callee_name, out))
        return true;

    op = llvm_map_extended_lookup(callee_name, (unsigned)argc);
    if (op == LLVM_MAP_EXT_SET) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
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
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM MapSet requires registered string-value runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapSet could not allocate value temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "set", key_name);
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapSet requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            key,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
    }
    if (op == LLVM_MAP_EXT_GET) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
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
                "LLVM MapGet could not lower key expression");
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapGet could not allocate result temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);
        if (value_name != NULL && strcmp(value_name, "String") == 0) {
            fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
                callee_name, "get", key_name);
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM MapGet requires registered string-value runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx)); return true; }
        }
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "get", key_name);
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapGet requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            key,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (op == LLVM_MAP_EXT_HAS) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        const char *key_name;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
            return true;
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
        key = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "has", key_name);
        if (key == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapHas could not lower key expression");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapHas requires registered runtime function");
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (op == LLVM_MAP_EXT_REMOVE) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
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
                "LLVM MapRemove could not lower key expression");
        if (value_name != NULL && strcmp(value_name, "String") == 0) {
            fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
                callee_name, "remove", key_name);
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM MapRemove requires registered string-value runtime function");
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            }
            { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
        }
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "remove", key_name);
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapRemove requires registered runtime function");
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
    }
    if (op == LLVM_MAP_EXT_SIZE) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_map_size_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapSize requires registered runtime function");
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (op == LLVM_MAP_EXT_KEYS) {
        ASTNode *map_arg = ast_call_argument(node, 0);
        LLVMVarEntry map_var;
        const char *key_name;
        LLVMTypeRef array_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, map_arg,
                callee_name, "HashMap", &map_var, out))
            return true;
        key_name = llvm_lookup_map_key(ctx, ast_identifier_name(map_arg));
        array_ty = llvm_hashmap_key_array_type(ctx, key_name);
        if (array_ty == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapKeys requires stable HashMap<Bool|Int|Long|String, T> key metadata");
        tmp = llvm_create_entry_alloca(ctx, array_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapKeys could not allocate key array temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_ty), tmp);
        fn = llvm_required_hashmap_raw_export(ctx, node, callee_name, "keys", key_name);
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM MapKeys requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        { *out = LLVMBuildLoad2(ctx->builder, array_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (pgy_codegen_call_name_is_slot_source(callee_name)
        && argc == 1) {
        *out = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (*out == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM collection slot source could not lower source expression");
        return true;
    }
    return false;
}
#endif
