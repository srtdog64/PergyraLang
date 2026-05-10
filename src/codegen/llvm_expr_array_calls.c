#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_array_calls.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"

static LLVMVarEntry *
llvm_array_required_receiver_var(LLVMGenCtx *ctx, ASTNode *node,
                                 ASTNode *receiver, const char *callee_name,
                                 LLVMArrayVarEntry **entry_out)
{
    if (entry_out != NULL)
        *entry_out = NULL;
    if (receiver == NULL || receiver->type != AST_IDENTIFIER
        || receiver->data.identifier.name == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM array operation '%s' requires an identifier receiver",
            callee_name);
        return NULL;
    }

    const char *name = receiver->data.identifier.name;
    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
    if (var == NULL || entry == NULL) {
        llvm_set_error_at_with_hints(ctx, receiver,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM array operation '%s' requires registered Array<T> local '%s'",
            callee_name, name);
        return NULL;
    }

    if (entry_out != NULL)
        *entry_out = entry;
    return var;
}

static const char *
llvm_array_required_elem_suffix(LLVMGenCtx *ctx, ASTNode *node,
                                LLVMArrayVarEntry *entry,
                                const char *callee_name)
{
    const char *suffix = entry != NULL
        ? llvm_type_to_suffix(ctx, entry->elem_type)
        : NULL;
    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
        return suffix;
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM array operation '%s' requires concrete Array<T> element metadata",
        callee_name);
    return NULL;
}

static bool
llvm_array_format_runtime_name(char *out, size_t out_size,
                               const char *prefix, const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, suffix);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_array_runtime_name_error(ASTNode *node, LLVMGenCtx *ctx,
                              const char *callee_name, LLVMValueRef *out)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM array operation '%s' runtime function name is too long",
        callee_name != NULL ? callee_name : "<unknown>");
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_array_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                             const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "ArrayLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef arr = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            *out = LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
            return true;
        }
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM ArrayLength requires concrete Array<T> aggregate operand");
        *out = NULL;
        return true;
    }

    if (strcmp(callee_name, "ArrayPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        LLVMArrayVarEntry *entry = NULL;
        LLVMVarEntry *arr_var = llvm_array_required_receiver_var(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_var == NULL) {
            *out = NULL;
            return true;
        }
        const char *suffix = llvm_array_required_elem_suffix(
            ctx, node, entry, callee_name);
        if (suffix == NULL)
            return true;

        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = NULL;
            return true;
        }
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }

        char fn_name[64];
        if (!llvm_array_format_runtime_name(fn_name, sizeof(fn_name),
                "pgy_array_push", suffix))
            return llvm_array_runtime_name_error(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "array", callee_name, fn_name);
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = { arr_var->alloca, value };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ArraySet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        LLVMArrayVarEntry *entry = NULL;
        LLVMVarEntry *arr_var = llvm_array_required_receiver_var(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_var == NULL) {
            *out = NULL;
            return true;
        }
        const char *suffix = llvm_array_required_elem_suffix(
            ctx, node, entry, callee_name);
        if (suffix == NULL)
            return true;

        LLVMValueRef idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (idx == NULL || value == NULL) {
            *out = NULL;
            return true;
        }

        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        char fn_name[64];
        if (!llvm_array_format_runtime_name(fn_name, sizeof(fn_name),
                "pgy_array_set", suffix))
            return llvm_array_runtime_name_error(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "array", callee_name, fn_name);
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef index64 = idx;
        if (LLVMTypeOf(index64) != ctx->type_i64)
            index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                ctx->type_i64, llvm_tmp_name(ctx));
        LLVMValueRef args[] = { arr_var->alloca, index64, value };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ArrayPop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        LLVMArrayVarEntry *entry = NULL;
        LLVMVarEntry *arr_var = llvm_array_required_receiver_var(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_var == NULL) {
            *out = NULL;
            return true;
        }
        const char *suffix = llvm_array_required_elem_suffix(
            ctx, node, entry, callee_name);
        if (suffix == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (!llvm_array_format_runtime_name(fn_name, sizeof(fn_name),
                "pgy_array_pop", suffix))
            return llvm_array_runtime_name_error(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "array", callee_name, fn_name);
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = { arr_var->alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    return false;
}

#endif
