#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

void
llvm_defer_scope_push(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return;
    if (ctx->defer_scope_depth >= MAX_SCOPE_DEPTH) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_SCOPE_LIMIT,
            PGY_CAUSE_LLVM_SCOPE_CAPACITY,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM defer scope registry exceeded MAX_SCOPE_DEPTH");
        return;
    }
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

void
llvm_defer_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

void
llvm_register_defer(ASTNode *body, LLVMGenCtx *ctx)
{
    if (ctx == NULL || body == NULL)
        return;
    if (ctx->defer_scope_depth <= 0) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_SCOPE_LIMIT,
            PGY_CAUSE_LLVM_SCOPE_CAPACITY,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM defer statement has no active defer scope");
        return;
    }
    int scope = ctx->defer_scope_depth - 1;
    int count = ctx->defer_body_counts[scope];
    if (count >= MAX_DEFER_PER_SCOPE) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_SCOPE_LIMIT,
            PGY_CAUSE_LLVM_SCOPE_CAPACITY,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM defer registry exceeded MAX_DEFER_PER_SCOPE");
        return;
    }
    ctx->defer_bodies[scope][count] = body;
    ctx->defer_body_counts[scope]++;
}

void
llvm_emit_defers_from(LLVMGenCtx *ctx, int start_depth)
{
    if (start_depth < 0)
        start_depth = 0;
    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            if (body != NULL)
                llvm_emit_statement(body, ctx);
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
