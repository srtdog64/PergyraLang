/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM parallel / async generated-name policy.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_stmt_parallel_names.h"

bool
llvm_parallel_counter_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                           const char *prefix, int counter)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL)
        return false;
    written = snprintf(out, out_size, "%s%d", prefix, counter);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error(ctx, "LLVM parallel generated name is too long");
    return false;
}

bool
llvm_parallel_task_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                        int counter, size_t index)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    written = snprintf(out, out_size, "_pgy_par_%d_%zu", counter, index);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error(ctx, "LLVM parallel task wrapper name is too long");
    return false;
}

bool
llvm_async_wrapper_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                        int counter)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    written = snprintf(out, out_size, "_pgy_async_%d_0", counter);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error(ctx, "LLVM async wrapper name is too long");
    return false;
}

bool
llvm_select_channel_runtime_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                                 const char *prefix, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "%s%s", prefix, inner);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error(ctx, "LLVM select channel runtime name is too long");
    return false;
}

#endif
