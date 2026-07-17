#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include <stdlib.h>
#include <string.h>

#include "llvm_internal_api.h"

typedef enum {
    LLVM_LIST_EXT_NONE = 0,
    LLVM_LIST_EXT_GET,
    LLVM_LIST_EXT_PUSH,
    LLVM_LIST_EXT_REMOVE,
    LLVM_LIST_EXT_SET,
    LLVM_LIST_EXT_SIZE,
} LLVMListExtendedOp;

typedef struct {
    const char *name;
    unsigned argc;
    LLVMListExtendedOp op;
} LLVMListExtendedSpec;

static const LLVMListExtendedSpec kListExtendedSpecs[] = {
    {"ListGet", 2, LLVM_LIST_EXT_GET},
    {"ListPush", 2, LLVM_LIST_EXT_PUSH},
    {"ListRemove", 2, LLVM_LIST_EXT_REMOVE},
    {"ListSet", 3, LLVM_LIST_EXT_SET},
    {"ListSize", 1, LLVM_LIST_EXT_SIZE},
};

static int
llvm_list_extended_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMListExtendedSpec *spec = (const LLVMListExtendedSpec *)entry;
    return strcmp(name, spec->name);
}

static LLVMListExtendedOp
llvm_list_extended_lookup(const char *callee_name, unsigned argc)
{
    const LLVMListExtendedSpec *spec;

    if (callee_name == NULL)
        return LLVM_LIST_EXT_NONE;
    spec = (const LLVMListExtendedSpec *)bsearch(
        callee_name,
        kListExtendedSpecs,
        sizeof(kListExtendedSpecs) / sizeof(kListExtendedSpecs[0]),
        sizeof(kListExtendedSpecs[0]),
        llvm_list_extended_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return LLVM_LIST_EXT_NONE;
    return spec->op;
}

bool
llvm_emit_list_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                             const char *callee_name, LLVMValueRef *out)
{
    LLVMListExtendedOp op;

    if (out == NULL)
        return false;
    *out = NULL;

    op = llvm_list_extended_lookup(callee_name,
        (unsigned)ast_call_arg_count(node));

    if (op == LLVM_LIST_EXT_PUSH) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, list_arg,
                callee_name, "collection", &list_var, out))
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListPush could not lower value expression");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = llvm_build_checked_fptosi(ctx, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_push_string_raw_export");
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM ListPush requires registered string runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListPush could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_push_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListPush requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
    }
    if (op == LLVM_LIST_EXT_GET) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, list_arg,
                callee_name, "collection", &list_var, out))
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (idx == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListGet could not lower index expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListGet could not allocate result temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_get_string_raw_export");
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM ListGet requires registered string runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
        }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_get_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListGet requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }
    if (op == LLVM_LIST_EXT_SET) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, list_arg,
                callee_name, "collection", &list_var, out))
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
                "LLVM ListSet could not lower index or value expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = llvm_build_checked_fptosi(ctx, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_set_string_raw_export");
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM ListSet requires registered string runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListSet could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_set_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListSet requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
    }
    if (op == LLVM_LIST_EXT_SIZE) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry list_var;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, list_arg,
                callee_name, "collection", &list_var, out))
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_size_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListSize requires registered runtime function");
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }
    if (op == LLVM_LIST_EXT_REMOVE) {
        ASTNode *list_arg = ast_call_argument(node, 0);
        LLVMVarEntry list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMFuncEntry *fn;
        if (!llvm_collection_required_receiver_var(ctx, node, list_arg,
                callee_name, "collection", &list_var, out))
            return true;
        inner_name = llvm_lookup_list_inner(ctx, ast_identifier_name(list_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "List",
            ast_identifier_name(list_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (idx == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListRemove could not lower index expression");
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_list_remove_string_raw_export");
            if (fn == NULL)
                return llvm_collection_extended_error_out(ctx, node, out,
                    "LLVM ListRemove requires registered string runtime function");
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
        }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_remove_raw_export");
        if (fn == NULL)
            return llvm_collection_extended_error_out(ctx, node, out,
                "LLVM ListRemove requires registered runtime function");
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var.alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            idx,
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = llvm_void_expression_placeholder(ctx, node, callee_name); return true; }
    }
    return false;
}
#endif
