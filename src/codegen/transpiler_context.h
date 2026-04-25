/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend context/output helpers.
 */

#ifndef PERGYRA_TRANSPILER_CONTEXT_H
#define PERGYRA_TRANSPILER_CONTEXT_H

#include "transpiler.h"

void transpiler_write_indent(TranspilerCtx *ctx);
void transpiler_write_indent_to(CodeBuf *buf, int indent);
void transpiler_set_backend_error(TranspilerCtx *ctx, const char *fmt, ...);
void transpiler_set_backend_error_with_hints(TranspilerCtx *ctx,
                                             const char *code,
                                             const char *cause_ir,
                                             const char *fix_source,
                                             const char *fmt, ...);
char *transpiler_scratch_strdup(TranspilerCtx *ctx, const char *s);
char *transpiler_scratch_fmt(TranspilerCtx *ctx, const char *fmt, ...);

static inline void
write_indent(TranspilerCtx *ctx)
{
    transpiler_write_indent(ctx);
}

static inline void
write_indent_to(CodeBuf *buf, int indent)
{
    transpiler_write_indent_to(buf, indent);
}

#endif /* PERGYRA_TRANSPILER_CONTEXT_H */
