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
#include "type_checker_flow_internal.h"

bool
type_check_unsafe_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return true;

    /* The flow owner also closes the lexical body scope and its resources. */
    (void)type_check_unsafe_block_flow(node, ctx, NULL);
    return ctx == NULL || !ctx->has_error;
}
