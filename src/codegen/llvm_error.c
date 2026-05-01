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
#include <string.h>

static LLVMGenResult *
llvm_result_alloc(void)
{
    LLVMGenResult *res = malloc(sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;
    memset(res, 0, sizeof(LLVMGenResult));
    pgy_arena_init(&res->owned_arena, 0);
    return res;
}

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

/* Full variant that records the `cause_ir` + `fix_source` routing tags
 * alongside the diagnostic code. `code`, `cause_ir`, `fix_source` must be
 * string literals (non-owning). Passing NULL leaves the corresponding
 * ctx field untouched. First call wins (subsequent errors in the same
 * pass are dropped, matching has_error). */
void
llvm_set_error_with_hints(LLVMGenCtx *ctx, const char *code,
                          const char *cause_ir, const char *fix_source,
                          const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = 0;
    ctx->error_column = 0;
    if (code != NULL)
        ctx->error_code = code;
    if (cause_ir != NULL)
        ctx->error_cause_ir = cause_ir;
    if (fix_source != NULL)
        ctx->error_fix_source = fix_source;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

void
llvm_set_error_at_with_hints(LLVMGenCtx *ctx, ASTNode *node, const char *code,
                              const char *cause_ir, const char *fix_source,
                              const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = (node != NULL) ? node->line : 0;
    ctx->error_column = (node != NULL) ? node->column : 0;
    if (code != NULL)
        ctx->error_code = code;
    if (cause_ir != NULL)
        ctx->error_cause_ir = cause_ir;
    if (fix_source != NULL)
        ctx->error_fix_source = fix_source;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

void
llvm_set_mir_inventory_missing(LLVMGenCtx *ctx, const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = 0;
    ctx->error_column = 0;
    ctx->error_code = PGY_CODE_LLVM_MIR_ROUTINE_MISSING;
    ctx->error_cause_ir = PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING;
    ctx->error_fix_source = PGY_FIX_INSPECT_MIR_INVENTORY;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

LLVMGenResult *
llvm_result_error(const char *message)
{
    LLVMGenResult *res = llvm_result_alloc();
    if (res == NULL)
        return NULL;

    res->success = false;
    res->error_message = pgy_arena_strdup(&res->owned_arena, message);
    return res;
}

LLVMGenResult *
llvm_result_error_fmt(const char *fmt, ...)
{
    LLVMGenResult *res = llvm_result_alloc();
    va_list args;

    if (res == NULL)
        return NULL;

    res->success = false;
    va_start(args, fmt);
    res->error_message = pgy_arena_vfmt(&res->owned_arena, fmt, args);
    va_end(args);
    return res;
}

LLVMGenResult *
llvm_result_success(char *ir_text)
{
    LLVMGenResult *res = llvm_result_alloc();
    if (res == NULL)
        return NULL;

    res->success = true;
    if (ir_text != NULL) {
        res->ir_text = pgy_arena_strdup(&res->owned_arena, ir_text);
        free(ir_text);
    }
    return res;
}

#endif
