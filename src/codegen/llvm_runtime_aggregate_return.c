/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM runtime aggregate-return ABI owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_internal_api.h"

#include <stdint.h>
#include <string.h>

static bool
llvm_runtime_aggregate_return_is_array_string_sret_name(
    const char *runtime_name)
{
    static const char *const names[] = {
        "StringSplit",
        "pgy_args",
        "pgy_dir_walk",
    };

    if (runtime_name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(runtime_name, names[i]) == 0)
            return true;
    }
    return false;
}

bool
llvm_runtime_aggregate_return_is_sret_function(const char *runtime_name)
{
    return llvm_runtime_aggregate_return_is_array_string_sret_name(runtime_name);
}

bool
llvm_runtime_aggregate_return_apply_decl_shape(LLVMGenCtx *ctx,
                                               const char *runtime_name,
                                               LLVMTypeRef *ret_type,
                                               LLVMTypeRef params[],
                                               unsigned *param_count,
                                               unsigned param_capacity)
{
    if (!llvm_runtime_aggregate_return_is_array_string_sret_name(runtime_name))
        return false;
    if (ctx == NULL || ret_type == NULL || params == NULL
        || param_count == NULL) {
        return false;
    }
    if (*ret_type != ctx->array_type_String) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM runtime aggregate-return ABI for '%s' expects Array<String>",
            runtime_name);
        return true;
    }
    if (*param_count >= param_capacity) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM runtime aggregate-return ABI for '%s' exceeds declaration arity",
            runtime_name);
        return true;
    }
    for (unsigned i = *param_count; i > 0; i--)
        params[i] = params[i - 1];
    params[0] = LLVMPointerType(ctx->array_type_String, 0);
    *param_count = *param_count + 1;
    *ret_type = ctx->type_void;
    return true;
}

LLVMValueRef
llvm_emit_runtime_aggregate_return_call(ASTNode *node, LLVMGenCtx *ctx,
                                        const char *family_name,
                                        const char *callee_name,
                                        const char *runtime_name,
                                        size_t source_arg_count)
{
    LLVMFuncEntry *fn;
    LLVMValueRef *args;
    LLVMValueRef slot;

    if (!llvm_runtime_aggregate_return_is_array_string_sret_name(runtime_name))
        return NULL;
    if (ctx == NULL) {
        return NULL;
    }
    if (source_arg_count > SIZE_MAX / sizeof(LLVMValueRef) - 1) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM runtime aggregate-return call '%s' exceeds argument limits",
            callee_name != NULL ? callee_name : runtime_name);
        return NULL;
    }

    fn = llvm_required_runtime_function(ctx, node, family_name, callee_name,
        runtime_name);
    if (fn == NULL)
        return NULL;
    if (fn->ret_type != ctx->type_void) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM runtime aggregate-return call '%s' is not declared as sret",
            runtime_name);
        return NULL;
    }

    args = pgy_arena_calloc(&ctx->scratch,
        (source_arg_count + 1) * sizeof(LLVMValueRef));
    if (args == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM runtime aggregate-return call '%s' could not allocate arguments",
            callee_name != NULL ? callee_name : runtime_name);
        return NULL;
    }

    slot = LLVMBuildAlloca(ctx->builder, ctx->array_type_String,
        llvm_tmp_name(ctx));
    args[0] = slot;
    for (size_t i = 0; i < source_arg_count; i++) {
        args[i + 1] = llvm_emit_expression(ast_call_argument(node, i), ctx);
        if (args[i + 1] == NULL) {
            llvm_set_error_at_with_hints(ctx, ast_call_argument(node, i),
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM runtime aggregate-return call '%s' could not lower argument %zu",
                callee_name != NULL ? callee_name : runtime_name, i);
            return NULL;
        }
    }

    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args,
        (unsigned)(source_arg_count + 1), "");
    return LLVMBuildLoad2(ctx->builder, ctx->array_type_String, slot,
        llvm_tmp_name(ctx));
}

#endif /* PGY_LLVM_ENABLED */
