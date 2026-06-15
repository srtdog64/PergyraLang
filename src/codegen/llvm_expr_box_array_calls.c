#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_box_array_calls.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "../parser/ast_api.h"

static bool
llvm_box_array_error(ASTNode *node, LLVMGenCtx *ctx, const char *message,
                     LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s", message != NULL ? message
                : "LLVM BoxArray constructor could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

static bool
llvm_box_array_expected_suffix(LLVMGenCtx *ctx, ASTNode *node,
                               char *out, size_t out_size)
{
    char array_type[128];

    if (ctx == NULL || ctx->expected_type_name == NULL
        || strncmp(ctx->expected_type_name, "Box<Array<", 10) != 0
        || !llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
            array_type, sizeof(array_type))
        || strncmp(array_type, "Array<", 6) != 0
        || !llvm_constructed_arg_name_copy(array_type, 0, out, out_size)
        || out[0] == '\0'
        || strcmp(out, "Unknown") == 0) {
        llvm_box_array_error(node, ctx,
            "LLVM BoxArray requires expected Box<Array<T>> metadata", NULL);
        return false;
    }
    return true;
}

static bool
llvm_box_array_runtime_name(char *out, size_t out_size, const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "pgy_box_array_new_ptr_%s", suffix);
    return written >= 0 && (size_t)written < out_size;
}

static LLVMValueRef
llvm_box_array_allocator_arg(ASTNode *node, LLVMGenCtx *ctx, ASTNode *arg,
                             LLVMValueRef *out)
{
    LLVMVarEntry var;
    const char *name;

    if (arg == NULL)
        return LLVMConstNull(LLVMPointerType(ctx->type_allocator, 0));
    if (arg->type != AST_IDENTIFIER || ast_identifier_name(arg) == NULL) {
        llvm_box_array_error(node, ctx,
            "LLVM BoxArray allocator must be a named Allocator local", out);
        return NULL;
    }
    name = ast_identifier_name(arg);
    if (!llvm_scope_lookup_snapshot(ctx, name, &var)
        || var.type != ctx->type_allocator) {
        llvm_box_array_error(node, ctx,
            "LLVM BoxArray allocator local must have type Allocator", out);
        return NULL;
    }
    return var.alloca;
}

bool
llvm_emit_box_array_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name, LLVMValueRef *out)
{
    char suffix[64];
    char fn_name[96];
    LLVMFuncEntry *fn;
    LLVMValueRef capacity;
    LLVMValueRef allocator;
    LLVMValueRef args[2];
    size_t argc;

    if (out == NULL)
        return false;
    *out = NULL;
    if (callee_name == NULL || strcmp(callee_name, "BoxArray") != 0)
        return false;

    argc = ast_call_arg_count(node);
    if (argc < 1 || argc > 2)
        return llvm_box_array_error(node, ctx,
            "LLVM BoxArray accepts capacity and optional Allocator only", out);
    if (!llvm_box_array_expected_suffix(ctx, node, suffix, sizeof(suffix)))
        return true;
    if (!llvm_box_array_runtime_name(fn_name, sizeof(fn_name), suffix))
        return llvm_box_array_error(node, ctx,
            "LLVM BoxArray runtime function name is too long", out);

    capacity = llvm_emit_expression(ast_call_argument(node, 0), ctx);
    if (capacity == NULL)
        return llvm_box_array_error(node, ctx,
            "LLVM BoxArray could not lower capacity expression", out);
    if (LLVMTypeOf(capacity) != ctx->type_i64)
        capacity = LLVMBuildSExtOrBitCast(ctx->builder, capacity,
            ctx->type_i64, llvm_tmp_name(ctx));

    allocator = argc > 1
        ? llvm_box_array_allocator_arg(node, ctx, ast_call_argument(node, 1),
            out)
        : LLVMConstNull(LLVMPointerType(ctx->type_allocator, 0));
    if (allocator == NULL)
        return true;

    fn = llvm_required_runtime_function(ctx, node, "box-array",
        callee_name, fn_name);
    if (fn == NULL)
        return llvm_box_array_error(node, ctx,
            "LLVM BoxArray requires registered runtime function", out);

    args[0] = capacity;
    args[1] = allocator;
    *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2,
        llvm_tmp_name(ctx));
    return true;
}

#endif /* PGY_LLVM_ENABLED */
