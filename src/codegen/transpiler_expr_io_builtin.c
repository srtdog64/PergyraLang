/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend I/O and time builtins.
 */

#include "transpiler_expr_io_builtin.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"

static char *
io_builtin_heap_fmt(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    s = (char *)malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

static char *
io_builtin_unsupported(TranspilerCtx *ctx, const char *message)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        message);
    return pergyra_strdup("0");
}

static char *
emit_builtin_file_open(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: FileOpen requires path and mode");

    char *path = emit_expression(ast_call_argument(call, 0), ctx);
    char *mode = emit_expression(ast_call_argument(call, 1), ctx);
    char *result = io_builtin_heap_fmt("pgy_file_open(%s, %s)", path, mode);
    free(path);
    free(mode);
    return result;
}

static char *
emit_builtin_file_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: FileRead requires file descriptor");

    char *fd = emit_expression(ast_call_argument(call, 0), ctx);
    char *result = io_builtin_heap_fmt("pgy_file_read(%s)", fd);
    free(fd);
    return result;
}

static char *
emit_builtin_file_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: FileWrite requires file descriptor and data");

    char *fd = emit_expression(ast_call_argument(call, 0), ctx);
    char *data = emit_expression(ast_call_argument(call, 1), ctx);
    char *result = io_builtin_heap_fmt("pgy_file_write(%s, %s)", fd, data);
    free(fd);
    free(data);
    return result;
}

static char *
emit_builtin_file_close(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: FileClose requires file descriptor");

    char *fd = emit_expression(ast_call_argument(call, 0), ctx);
    char *result = io_builtin_heap_fmt("pgy_file_close(%s)", fd);
    free(fd);
    return result;
}

static char *
emit_builtin_read_file(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: ReadFile requires path");

    char *path = emit_expression(ast_call_argument(call, 0), ctx);
    char *result = io_builtin_heap_fmt("pgy_read_file(%s)", path);
    free(path);
    return result;
}

static char *
emit_builtin_write_file(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: WriteFile requires path and data");

    char *path = emit_expression(ast_call_argument(call, 0), ctx);
    char *data = emit_expression(ast_call_argument(call, 1), ctx);
    char *result = io_builtin_heap_fmt("pgy_write_file(%s, %s)", path, data);
    free(path);
    free(data);
    return result;
}

static char *
emit_builtin_input(ASTNode *call, TranspilerCtx *ctx)
{
    char *prompt = ast_call_arg_count(call) >= 1
        ? emit_expression(ast_call_argument(call, 0), ctx)
        : pergyra_strdup("\"\"");
    char *result = io_builtin_heap_fmt("pgy_input(%s)", prompt);
    free(prompt);
    return result;
}

static char *
emit_builtin_print(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: Print requires message");

    char *msg = emit_expression(ast_call_argument(call, 0), ctx);
    char *result = io_builtin_heap_fmt("pgy_print(%s)", msg);
    free(msg);
    return result;
}

static char *
emit_builtin_sleep(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: Sleep requires milliseconds");

    char *ms = emit_expression(ast_call_argument(call, 0), ctx);
    char *result = io_builtin_heap_fmt("pgy_sleep_ms(%s)", ms);
    free(ms);
    return result;
}

char *
emit_builtin_io(ASTNode *call, BuiltinKind bk, TranspilerCtx *ctx)
{
    switch (bk) {
    case BUILTIN_FILE_OPEN:
        return emit_builtin_file_open(call, ctx);
    case BUILTIN_FILE_READ:
        return emit_builtin_file_read(call, ctx);
    case BUILTIN_FILE_WRITE:
        return emit_builtin_file_write(call, ctx);
    case BUILTIN_FILE_CLOSE:
        return emit_builtin_file_close(call, ctx);
    case BUILTIN_READ_FILE:
        return emit_builtin_read_file(call, ctx);
    case BUILTIN_WRITE_FILE:
        return emit_builtin_write_file(call, ctx);
    case BUILTIN_INPUT:
        return emit_builtin_input(call, ctx);
    case BUILTIN_PRINT:
        return emit_builtin_print(call, ctx);
    case BUILTIN_READ_LINE:
        return pergyra_strdup("pgy_input(\"\")");
    case BUILTIN_NOW:
        return pergyra_strdup("pgy_now_ms()");
    case BUILTIN_SLEEP:
        return emit_builtin_sleep(call, ctx);
    default:
        return io_builtin_unsupported(ctx,
            "C backend: unsupported I/O builtin");
    }
}
