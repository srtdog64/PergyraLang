/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent step completion emission.
 */

#include "transpiler_intent_step_completion_emit.h"

#include <stdlib.h>

#include "transpiler_context.h"
#include "transpiler_intent_failure_emit.h"

void
transpiler_emit_intent_step_completion(
    TranspilerCtx *ctx,
    size_t step_index,
    bool has_compensate_steps,
    ASTNode *guard_expr,
    ASTNode *expect_expr,
    ASTNode *post_expr,
    ASTNode *invariant_post_expr,
    const char *step_name,
    const char *intent_name,
    bool emit_cleanup_from_mir,
    size_t cleanup_block)
{
    if (has_compensate_steps) {
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_step_completed[%zu] = true;\n",
            step_index);
    }

    if (guard_expr != NULL) {
        char *guard = emit_expression(guard_expr, ctx);
        emit_intent_step_condition_failure(ctx->out, ctx, guard,
            "guard", step_name, intent_name,
            emit_cleanup_from_mir, cleanup_block);
        free(guard);
    }

    if (expect_expr != NULL) {
        char *expect = emit_expression(expect_expr, ctx);
        emit_intent_step_condition_failure(ctx->out, ctx, expect,
            "expect", step_name, intent_name,
            emit_cleanup_from_mir, cleanup_block);
        free(expect);
    }

    if (post_expr != NULL) {
        char *post = emit_expression(post_expr, ctx);
        emit_intent_step_condition_failure(ctx->out, ctx, post,
            "post", step_name, intent_name,
            emit_cleanup_from_mir, cleanup_block);
        free(post);
    }

    if (invariant_post_expr != NULL) {
        char *invariant = emit_expression(invariant_post_expr, ctx);
        emit_intent_step_condition_failure(ctx->out, ctx, invariant,
            "invariant-post", step_name, intent_name,
            emit_cleanup_from_mir, cleanup_block);
        free(invariant);
    }
    if (ctx->uses_intent_observability) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "pgy_intent_trace_step_ok_export(__intent_handle, \"%s\");\n",
            step_name != NULL ? step_name : "<step>");
    }
}
