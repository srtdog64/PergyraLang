/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM spawn expression generated-name policy.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_names.h"

bool
llvm_spawn_copy_name(LLVMGenCtx *ctx, ASTNode *node,
                     char *out, size_t out_size,
                     const char *name, const char *label)
{
    size_t len;

    if (out == NULL || out_size == 0)
        return false;
    if (name == NULL) {
        out[0] = '\0';
        return true;
    }

    len = strlen(name);
    if (len < out_size) {
        memcpy(out, name, len + 1);
        return true;
    }

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM spawn %s name is too long for '%s'",
            label != NULL ? label : "generated",
            name);
    }
    return false;
}

bool
llvm_spawn_format_name(LLVMGenCtx *ctx, ASTNode *node,
                       char *out, size_t out_size,
                       const char *prefix, const char *suffix,
                       const char *label)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s%s",
        prefix != NULL ? prefix : "",
        suffix != NULL ? suffix : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM spawn %s name is too long for '%s'",
            label != NULL ? label : "generated",
            suffix != NULL ? suffix : "<suffix>");
    }
    return false;
}

bool
llvm_spawn_wrapper_name(LLVMGenCtx *ctx, ASTNode *node,
                        char *out, size_t out_size, int wrapper_id)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "_pgy_spawn_expr_%d", wrapper_id);
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM spawn wrapper name is too long for id %d",
            wrapper_id);
    }
    return false;
}

#endif
