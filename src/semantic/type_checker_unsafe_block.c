/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Unsafe block statement validation.
 *
 * An unsafe block records the EFFECT_UNSAFE effect on the enclosing
 * callable. Because the effect system already propagates callee effects to
 * callers, the unsafe surface of a program becomes a queryable property of
 * the effect graph (the "where is the danger" DAG). Raw-memory unsafe work
 * is additionally forbidden inside a parallel task, mirroring the existing
 * parallel/secure restriction: concurrent unchecked memory access compounds
 * the danger and must be serialized.
 */

#include "type_checker.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

bool
type_check_unsafe_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return true;

    semantic_record_effect(ctx, EFFECT_UNSAFE);

    if (ctx != NULL && ctx->in_parallel) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN,
            PGY_CAUSE_PARALLEL_SECURE_IN_TASK,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
            node,
            "Unsafe block is not permitted inside a parallel task; raw memory "
            "operations across concurrent tasks compound undefined behavior. "
            "Serialize the unsafe work outside the parallel block.");
        return false;
    }

    if (ast_unsafe_block_body(node) != NULL)
        type_check_block(ast_unsafe_block_body(node), ctx);
    return ctx == NULL || !ctx->has_error;
}
