/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared surface between the scalar-builtin dispatch and its string half.
 *
 * The op enum and the two argument emitters are the only facts the string
 * owner needs from the dispatcher. They are declared here rather than in the
 * public header because no caller outside this pair may reach them.
 */

#ifndef PGY_TRANSPILER_EXPR_STDLIB_SCALAR_INTERNAL_H
#define PGY_TRANSPILER_EXPR_STDLIB_SCALAR_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

typedef enum {
    TRANSPILER_SCALAR_OP_NONE = 0,
    TRANSPILER_SCALAR_OP_ABS,
    TRANSPILER_SCALAR_OP_ATAN2,
    TRANSPILER_SCALAR_OP_CHECKED_ADD,
    TRANSPILER_SCALAR_OP_CHECKED_MUL,
    TRANSPILER_SCALAR_OP_CLAMP,
    TRANSPILER_SCALAR_OP_CONCAT,
    TRANSPILER_SCALAR_OP_E,
    TRANSPILER_SCALAR_OP_EXIT,
    TRANSPILER_SCALAR_OP_LOWER,
    TRANSPILER_SCALAR_OP_MAX,
    TRANSPILER_SCALAR_OP_MIN,
    TRANSPILER_SCALAR_OP_PI,
    TRANSPILER_SCALAR_OP_POW,
    TRANSPILER_SCALAR_OP_RANDOM,
    TRANSPILER_SCALAR_OP_REPLACE,
    TRANSPILER_SCALAR_OP_SEED_RANDOM,
    TRANSPILER_SCALAR_OP_SPLIT,
    TRANSPILER_SCALAR_OP_SQRT,
    TRANSPILER_SCALAR_OP_STRING_CONTAINS,
    TRANSPILER_SCALAR_OP_STRING_INDEX_OF,
    TRANSPILER_SCALAR_OP_STRING_JOIN,
    TRANSPILER_SCALAR_OP_STRING_LENGTH,
    TRANSPILER_SCALAR_OP_STRING_TRIM,
    TRANSPILER_SCALAR_OP_SUBSTRING,
    TRANSPILER_SCALAR_OP_SUBSTRING_WITH_LEN,
    TRANSPILER_SCALAR_OP_SUB_INDEX_OF,
    TRANSPILER_SCALAR_OP_SUB_INDEX_OF_WITH_LEN,
    TRANSPILER_SCALAR_OP_SUB_EQUALS,
    TRANSPILER_SCALAR_OP_SUB_EQUALS_WITH_LEN,
    TRANSPILER_SCALAR_OP_SUB_CONTAINS,
    TRANSPILER_SCALAR_OP_SUB_CONTAINS_WITH_LEN,
    TRANSPILER_SCALAR_OP_SUB_STARTS_WITH,
    TRANSPILER_SCALAR_OP_SUB_STARTS_WITH_LEN,
    TRANSPILER_SCALAR_OP_CHAR_AT_N,
    TRANSPILER_SCALAR_OP_CHAR_CODE,
    TRANSPILER_SCALAR_OP_TO_FLOAT,
    TRANSPILER_SCALAR_OP_TO_INT,
    TRANSPILER_SCALAR_OP_UPPER,
} TranspilerScalarOp;

char *transpiler_scalar_emit_arg(TranspilerCtx *ctx,
                                 ASTNode *arg,
                                 const char *fn,
                                 const char *role);

char *transpiler_scalar_emit_sub_with_explicit_source_len(
    TranspilerCtx *ctx,
    ASTNode *call,
    const char *builtin_name,
    const char *runtime_name,
    const char *tail_role,
    bool has_window_len);

/* True for the ops lowered by transpiler_expr_stdlib_scalar_string.c. The
 * dispatcher asks before delegating, so a missing case is a compile-time
 * omission in one place instead of a silent fallthrough to NULL. */
bool transpiler_scalar_op_is_string(TranspilerScalarOp op);

char *transpiler_scalar_emit_string(TranspilerScalarOp op,
                                    const char *fn,
                                    ASTNode *call,
                                    TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_SCALAR_INTERNAL_H */
