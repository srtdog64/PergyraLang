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
io_builtin_heap_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: I/O builtin expression formatting failed");
        return NULL;
    }

    s = (char *)malloc((size_t)n + 1);
    if (s == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: I/O builtin expression allocation failed");
        return NULL;
    }

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
    return NULL;
}

static char *
io_builtin_emit_arg(TranspilerCtx *ctx,
                    ASTNode *arg,
                    const char *builtin_name,
                    const char *role)
{
    char *lowered = emit_expression(arg, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "I/O builtin",
        role != NULL ? role : "value");
    return NULL;
}

static char *
emit_builtin_file_open(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: FileOpen requires path and mode");

    char *path = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "FileOpen", "path");
    if (path == NULL)
        return NULL;
    char *mode = io_builtin_emit_arg(ctx, ast_call_argument(call, 1),
        "FileOpen", "mode");
    if (mode == NULL) {
        free(path);
        return NULL;
    }
    char *result = io_builtin_heap_fmt(ctx, "pgy_file_open(%s, %s)", path, mode);
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

    char *fd = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "FileRead", "file descriptor");
    if (fd == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_file_read(%s)", fd);
    free(fd);
    return result;
}

static char *
emit_builtin_file_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: FileWrite requires file descriptor and data");

    char *fd = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "FileWrite", "file descriptor");
    if (fd == NULL)
        return NULL;
    char *data = io_builtin_emit_arg(ctx, ast_call_argument(call, 1),
        "FileWrite", "data");
    if (data == NULL) {
        free(fd);
        return NULL;
    }
    char *result = io_builtin_heap_fmt(ctx, "pgy_file_write(%s, %s)", fd, data);
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

    char *fd = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "FileClose", "file descriptor");
    if (fd == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_file_close(%s)", fd);
    free(fd);
    return result;
}

static char *
emit_builtin_read_file(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: ReadFile requires path");

    char *path = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "ReadFile", "path");
    if (path == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_read_file(%s)", path);
    free(path);
    return result;
}

static char *
emit_builtin_read_stdin(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: ReadStdin requires max byte count");

    char *max_bytes = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "ReadStdin", "max byte count");
    if (max_bytes == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_read_stdin(%s)", max_bytes);
    free(max_bytes);
    return result;
}

static char *
emit_builtin_file_exists(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: FileExists requires path");

    char *path = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "FileExists", "path");
    if (path == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_file_exists(%s)", path);
    free(path);
    return result;
}

static char *
emit_builtin_write_file(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2)
        return io_builtin_unsupported(ctx,
            "C backend: WriteFile requires path and data");

    char *path = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "WriteFile", "path");
    if (path == NULL)
        return NULL;
    char *data = io_builtin_emit_arg(ctx, ast_call_argument(call, 1),
        "WriteFile", "data");
    if (data == NULL) {
        free(path);
        return NULL;
    }
    char *result = io_builtin_heap_fmt(ctx, "pgy_write_file(%s, %s)", path, data);
    free(path);
    free(data);
    return result;
}

static char *
emit_builtin_dir_walk(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: DirWalk requires root path");

    char *root = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "DirWalk", "root path");
    if (root == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_dir_walk(%s)", root);
    free(root);
    return result;
}

static char *
emit_builtin_input(ASTNode *call, TranspilerCtx *ctx)
{
    char *prompt = ast_call_arg_count(call) >= 1
        ? io_builtin_emit_arg(ctx, ast_call_argument(call, 0), "Input", "prompt")
        : pergyra_strdup("\"\"");
    if (prompt == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_input(%s)", prompt);
    free(prompt);
    return result;
}

static char *
emit_builtin_print(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: Print requires message");

    char *msg = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "Print", "message");
    if (msg == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_print(%s)", msg);
    free(msg);
    return result;
}

static char *
emit_builtin_sleep(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1)
        return io_builtin_unsupported(ctx,
            "C backend: Sleep requires milliseconds");

    char *ms = io_builtin_emit_arg(ctx, ast_call_argument(call, 0),
        "Sleep", "milliseconds");
    if (ms == NULL)
        return NULL;
    char *result = io_builtin_heap_fmt(ctx, "pgy_sleep_ms(%s)", ms);
    free(ms);
    return result;
}

char *
emit_builtin_io(ASTNode *call, BuiltinKind bk, TranspilerCtx *ctx)
{
    switch (bk) {
    case BUILTIN_DIR_WALK:
        return emit_builtin_dir_walk(call, ctx);
    case BUILTIN_FILE_EXISTS:
        return emit_builtin_file_exists(call, ctx);
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
    case BUILTIN_READ_STDIN:
        return emit_builtin_read_stdin(call, ctx);
    case BUILTIN_WRITE_FILE:
        return emit_builtin_write_file(call, ctx);
    case BUILTIN_INPUT:
        return emit_builtin_input(call, ctx);
    case BUILTIN_ARGS:
        return pergyra_strdup("pgy_args()");
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
