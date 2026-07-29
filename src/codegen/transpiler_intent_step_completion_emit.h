/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent step completion emission declarations.
 */

#ifndef PGY_TRANSPILER_INTENT_STEP_COMPLETION_EMIT_H
#define PGY_TRANSPILER_INTENT_STEP_COMPLETION_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

void transpiler_emit_intent_step_completion(
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
    size_t cleanup_block);

#endif /* PGY_TRANSPILER_INTENT_STEP_COMPLETION_EMIT_H */
