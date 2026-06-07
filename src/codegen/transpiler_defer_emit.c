/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend lexical defer support.
 */

#include "transpiler_defer_emit.h"

#include "../semantic/diag_codes.h"
#include "transpiler_context.h"

void
transpiler_defer_scope_push(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    if (ctx->defer_scope_depth >= TRANSPILE_MAX_SCOPE_DEPTH) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer scope registry exceeded TRANSPILE_MAX_SCOPE_DEPTH");
        return;
    }
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

void
transpiler_defer_scope_pop(TranspilerCtx *ctx)
{
    if (ctx == NULL || ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

void
transpiler_register_defer(ASTNode *body, TranspilerCtx *ctx)
{
    int scope;
    int count;

    if (ctx == NULL || body == NULL)
        return;
    if (ctx->defer_scope_depth <= 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer statement has no active defer scope");
        return;
    }

    scope = ctx->defer_scope_depth - 1;
    count = ctx->defer_body_counts[scope];
    if (count >= TRANSPILE_MAX_DEFER_PER_SCOPE) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend defer registry exceeded TRANSPILE_MAX_DEFER_PER_SCOPE");
        return;
    }

    ctx->defer_bodies[scope][count] = body;
    ctx->defer_body_counts[scope]++;
}

void
transpiler_emit_defers_from(TranspilerCtx *ctx, int start_depth)
{
    if (ctx == NULL)
        return;
    if (start_depth < 0)
        start_depth = 0;

    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            if (body != NULL)
                emit_statement(body, ctx);
        }
    }
}
