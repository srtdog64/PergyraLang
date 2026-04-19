/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend error/result helpers.
 *
 * This file is only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

void
llvm_set_error(LLVMGenCtx *ctx, const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = 0;
    ctx->error_column = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

/* Attach a stable diagnostic code (e.g. "PGY_LLVM_SPEC_LIMIT") to the
 * emitted error. `code` must have static lifetime (string literal).
 * First error wins — further calls are no-ops. */
void
llvm_set_error_with_code(LLVMGenCtx *ctx, const char *code,
                         const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = 0;
    ctx->error_column = 0;
    if (code != NULL)
        ctx->error_code = code;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

void
llvm_set_error_at(LLVMGenCtx *ctx, ASTNode *node, const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = (node != NULL) ? node->line : 0;
    ctx->error_column = (node != NULL) ? node->column : 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

void
llvm_set_error_at_with_code(LLVMGenCtx *ctx, ASTNode *node, const char *code,
                            const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = (node != NULL) ? node->line : 0;
    ctx->error_column = (node != NULL) ? node->column : 0;
    if (code != NULL)
        ctx->error_code = code;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

LLVMGenResult *
llvm_result_error(const char *message)
{
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;

    res->success = false;
    res->error_message = pergyra_strdup(message);
    return res;
}

LLVMGenResult *
llvm_result_success(char *ir_text)
{
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;

    res->success = true;
    res->ir_text = ir_text;
    return res;
}

#endif
