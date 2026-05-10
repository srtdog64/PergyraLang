/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM let-binding diagnostic and generated-name policy.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_stmt_let_names.h"

bool
llvm_stmt_require_let_type_arg(LLVMGenCtx *ctx, ASTNode *node,
                               const char *binding_name,
                               const char *container_name)
{
    if (ctx == NULL)
        return false;
    if (!ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s let-binding for '%s' requires an explicit concrete %s<T> annotation",
            container_name != NULL ? container_name : "typed",
            binding_name != NULL ? binding_name : "<binding>",
            container_name != NULL ? container_name : "type");
    }
    return false;
}

bool
llvm_let_with_token_name(LLVMGenCtx *ctx, ASTNode *node,
                         char *out, size_t out_size,
                         const char *binding_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_token",
        binding_name != NULL ? binding_name : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM secure slot token name is too long for binding '%s'",
            binding_name != NULL ? binding_name : "<binding>");
    }
    return false;
}

bool
llvm_let_with_slot_write_name(LLVMGenCtx *ctx, ASTNode *node,
                              char *out, size_t out_size,
                              const char *inner_type, bool is_secure)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size,
        is_secure ? "pgy_secure_write_%s" : "pgy_write_%s",
        inner_type != NULL ? inner_type : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM slot write runtime symbol is too long for type '%s'",
            inner_type != NULL ? inner_type : "<type>");
    }
    return false;
}

#endif
