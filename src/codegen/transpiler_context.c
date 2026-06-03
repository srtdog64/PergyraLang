/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend context and output-buffer ownership.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "transpiler_context.h"
#include "../semantic/diag_codes.h"

#define CODEBUF_INITIAL_CAP 4096

static bool
codebuf_reserve(CodeBuf *buf, size_t additional)
{
    size_t required;
    size_t new_cap;
    char *grown;

    if (buf == NULL || buf->data == NULL)
        return false;
    if (buf->len > SIZE_MAX - additional
        || buf->len + additional > SIZE_MAX - 1) {
        return false;
    }

    required = buf->len + additional + 1;
    if (required <= buf->cap)
        return true;

    new_cap = buf->cap == 0 ? CODEBUF_INITIAL_CAP : buf->cap;
    while (new_cap < required) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = required;
            break;
        }
        new_cap *= 2;
    }
    if (new_cap < required)
        return false;

    grown = realloc(buf->data, new_cap);
    if (grown == NULL)
        return false;
    buf->data = grown;
    buf->cap = new_cap;
    return true;
}

CodeBuf *
codebuf_create(void)
{
    CodeBuf *b = calloc(1, sizeof(CodeBuf));
    if (b == NULL)
        return NULL;
    b->data = malloc(CODEBUF_INITIAL_CAP);
    if (b->data == NULL) {
        free(b);
        return NULL;
    }
    b->data[0] = '\0';
    b->cap     = CODEBUF_INITIAL_CAP;
    b->len     = 0;
    return b;
}

void
codebuf_destroy(CodeBuf *buf)
{
    if (buf == NULL)
        return;
    free(buf->data);
    free(buf);
}

void
codebuf_write(CodeBuf *buf, const char *fmt, ...)
{
    static const char null_text[] = "(null)";
    va_list ap;
    const char *s;
    char ch;

    if (buf == NULL || fmt == NULL)
        return;

    if (strchr(fmt, '%') == NULL) {
        codebuf_write_raw(buf, fmt, strlen(fmt));
        return;
    }

    va_start(ap, fmt);

    if (strcmp(fmt, "%s") == 0) {
        s = va_arg(ap, const char *);
        if (s != NULL)
            codebuf_write_raw(buf, s, strlen(s));
        else
            codebuf_write_raw(buf, null_text, sizeof(null_text) - 1);
        va_end(ap);
        return;
    }

    if (strcmp(fmt, "%c") == 0) {
        ch = (char)va_arg(ap, int);
        codebuf_write_raw(buf, &ch, 1);
        va_end(ap);
        return;
    }

    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed < 0) {
        va_end(ap);
        return;
    }

    if (!codebuf_reserve(buf, (size_t)needed)) {
        va_end(ap);
        return;
    }

    vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, ap);
    buf->len += (size_t)needed;
    va_end(ap);
}

void
codebuf_write_raw(CodeBuf *buf, const char *s, size_t n)
{
    if (buf == NULL || (s == NULL && n > 0))
        return;
    if (!codebuf_reserve(buf, n))
        return;
    if (n > 0)
        memcpy(buf->data + buf->len, s, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
}

void
codebuf_truncate(CodeBuf *buf, size_t len)
{
    if (buf == NULL || buf->data == NULL)
        return;
    if (len > buf->len)
        return;
    buf->len = len;
    buf->data[buf->len] = '\0';
}

bool
codebuf_dump_file(const CodeBuf *buf, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return false;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
    return true;
}

static void
transpiler_set_backend_error_v(TranspilerCtx *ctx, const char *code,
                               const char *fmt, va_list ap)
{
    va_list ap2;
    int needed;
    char *msg;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0)
        return;

    msg = malloc((size_t)needed + 1);
    if (msg == NULL)
        return;

    vsnprintf(msg, (size_t)needed + 1, fmt, ap);
    ctx->backend_error = msg;
    if (code != NULL && ctx->backend_error_code == NULL)
        ctx->backend_error_code = code;
}

void
transpiler_set_backend_error(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, NULL, fmt, ap);
    va_end(ap);
}

void
transpiler_set_backend_error_with_hints(TranspilerCtx *ctx,
                                        const char *code,
                                        const char *cause_ir,
                                        const char *fix_source,
                                        const char *fmt, ...)
{
    va_list ap;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    if (cause_ir != NULL)
        ctx->backend_error_cause_ir = cause_ir;
    if (fix_source != NULL)
        ctx->backend_error_fix_source = fix_source;

    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, code, fmt, ap);
    va_end(ap);
}

void
transpiler_set_mir_inventory_missing(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    ctx->backend_error_cause_ir = PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING;
    ctx->backend_error_fix_source = PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING;

    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, PGY_CODE_MIR_TOPOLOGY_INVALID, fmt, ap);
    va_end(ap);
}

void
transpiler_set_mir_topology_invalid(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    ctx->backend_error_cause_ir = PGY_CAUSE_MIR_TOPOLOGY_INVALID;
    ctx->backend_error_fix_source = PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING;

    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, PGY_CODE_MIR_TOPOLOGY_INVALID, fmt, ap);
    va_end(ap);
}

void
transpiler_set_mir_intent_carrier_missing(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    ctx->backend_error_cause_ir = PGY_CAUSE_MIR_INTENT_CARRIER_MISSING;
    ctx->backend_error_fix_source = PGY_FIX_CHECK_INTENT_STEP_LOWERING;

    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, PGY_CODE_MIR_INTENT_CARRIER_MISSING, fmt, ap);
    va_end(ap);
}

char *
transpiler_scratch_strdup(TranspilerCtx *ctx, const char *s)
{
    if (ctx == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->arena, s);
}

char *
transpiler_scratch_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    char *result;

    if (ctx == NULL || fmt == NULL)
        return NULL;

    va_start(ap, fmt);
    result = pgy_arena_vfmt(&ctx->arena, fmt, ap);
    va_end(ap);
    return result;
}

TranspilerCtx *
transpiler_ctx_create(void)
{
    TranspilerCtx *ctx = calloc(1, sizeof(TranspilerCtx));
    if (ctx == NULL)
        return NULL;
    pgy_arena_init(&ctx->arena, 0);
    ctx->out   = codebuf_create();
    ctx->decls = codebuf_create();
    ctx->helpers = codebuf_create();
    if (ctx->out == NULL || ctx->decls == NULL || ctx->helpers == NULL) {
        codebuf_destroy(ctx->out);
        codebuf_destroy(ctx->decls);
        codebuf_destroy(ctx->helpers);
        pgy_arena_destroy(&ctx->arena);
        free(ctx);
        return NULL;
    }
    return ctx;
}

void
transpiler_write_indent(TranspilerCtx *ctx)
{
    static const char indent_chunk[] =
        "                                                                ";
    size_t remaining;

    if (ctx == NULL || ctx->out == NULL || ctx->indent <= 0)
        return;

    remaining = (size_t)ctx->indent * 4;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > sizeof(indent_chunk) - 1)
            chunk = sizeof(indent_chunk) - 1;
        codebuf_write_raw(ctx->out, indent_chunk, chunk);
        remaining -= chunk;
    }
}

void
transpiler_write_indent_to(CodeBuf *buf, int indent)
{
    static const char indent_chunk[] =
        "                                                                ";
    size_t remaining;

    if (buf == NULL || indent <= 0)
        return;

    remaining = (size_t)indent * 4;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > sizeof(indent_chunk) - 1)
            chunk = sizeof(indent_chunk) - 1;
        codebuf_write_raw(buf, indent_chunk, chunk);
        remaining -= chunk;
    }
}

void
write_indent(TranspilerCtx *ctx)
{
    transpiler_write_indent(ctx);
}

void
write_indent_to(CodeBuf *buf, int indent)
{
    transpiler_write_indent_to(buf, indent);
}

void
transpiler_ctx_destroy(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    codebuf_destroy(ctx->out);
    codebuf_destroy(ctx->decls);
    codebuf_destroy(ctx->helpers);
    free(ctx->backend_error);
    pgy_arena_destroy(&ctx->arena);
    free(ctx);
}
