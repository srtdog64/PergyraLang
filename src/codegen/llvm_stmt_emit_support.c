/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM statement emission support routines.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_stmt_emit_support.h"

bool
llvm_mir_base_name_from_versioned(const char *mir_name,
                                  char *base_out,
                                  size_t base_out_size)
{
    const char *last_dot;
    size_t len;

    if (base_out != NULL && base_out_size > 0)
        base_out[0] = '\0';
    if (mir_name == NULL || base_out == NULL || base_out_size == 0)
        return false;

    last_dot = strrchr(mir_name, '.');
    len = last_dot != NULL ? (size_t)(last_dot - mir_name) : strlen(mir_name);
    if (len == 0 || len >= base_out_size)
        return false;

    memcpy(base_out, mir_name, len);
    base_out[len] = '\0';
    return true;
}

bool
llvm_stmt_format_runtime_name(LLVMGenCtx *ctx,
                              ASTNode *node,
                              char *out,
                              size_t out_size,
                              const char *prefix,
                              const char *type_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s%s",
        prefix != NULL ? prefix : "",
        type_name != NULL ? type_name : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM statement runtime symbol is too long for type '%s'",
            type_name != NULL ? type_name : "<type>");
    }
    return false;
}

bool
llvm_stmt_format_bind_name(LLVMGenCtx *ctx,
                           ASTNode *node,
                           char *out,
                           size_t out_size,
                           const char *prefix,
                           const char *suffix,
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
            "LLVM bind %s name is too long for '%s'",
            label != NULL ? label : "generated",
            prefix != NULL ? prefix : "<name>");
    }
    return false;
}

#endif
