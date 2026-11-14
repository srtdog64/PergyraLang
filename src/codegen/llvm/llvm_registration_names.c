/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM registration ABI-name construction and bounded failure diagnostics.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_registration_names.h"

#include <stdio.h>

#include "llvm_internal.h"

bool
llvm_register_join_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                        const char *left, const char *sep,
                        const char *right, const char *surface)
{
    int written;

    if (out == NULL || out_size == 0 || left == NULL || sep == NULL
        || right == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s%s%s", left, sep, right);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM registration generated name is too long for %s",
        surface != NULL ? surface : "declaration");
    return false;
}

bool
llvm_register_payload_field_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                                 size_t index)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    written = snprintf(out, out_size, "_%zu", index);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM enum payload field name is too long");
    return false;
}

#endif /* PGY_LLVM_ENABLED */
