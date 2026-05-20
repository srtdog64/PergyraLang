/*
 * Copyright (c) 2026 Pergyra Language Project
 * Loop-control statement semantic checks.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"

static int
semantic_find_labeled_loop_depth(SemanticContext *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

bool
type_check_break_stmt(ASTNode *node, SemanticContext *ctx)
{
    if (ctx == NULL)
        return true;

    if (ctx->loop_depth <= 0) {
        semantic_error_with_hints(
            ctx,
            PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL,
            PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL,
            node,
            "'break' used outside of loop");
        return false;
    }

    if (ast_break_label(node) != NULL
        && semantic_find_labeled_loop_depth(ctx, ast_break_label(node)) < 0) {
        semantic_error_with_hints(
            ctx,
            PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL,
            PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL,
            node,
            "Unknown loop label '%s' in break",
            ast_break_label(node));
        return false;
    }

    return true;
}

bool
type_check_continue_stmt(ASTNode *node, SemanticContext *ctx)
{
    if (ctx == NULL)
        return true;

    if (ctx->loop_depth <= 0) {
        semantic_error_with_hints(
            ctx,
            PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL,
            PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL,
            node,
            "'continue' used outside of loop");
        return false;
    }

    if (ast_continue_label(node) != NULL
        && semantic_find_labeled_loop_depth(ctx,
            ast_continue_label(node)) < 0) {
        semantic_error_with_hints(
            ctx,
            PGY_CODE_SEM_LOOP_CONTROL_INVALID,
            PGY_CAUSE_LOOP_CONTROL,
            PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL,
            node,
            "Unknown loop label '%s' in continue",
            ast_continue_label(node));
        return false;
    }

    return true;
}
