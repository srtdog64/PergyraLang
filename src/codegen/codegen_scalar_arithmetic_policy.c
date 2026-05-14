/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared scalar arithmetic lowering policy for C and LLVM backends.
 */

#include "codegen_scalar_arithmetic_policy.h"

#include <stdint.h>

#include "../parser/ast.h"

bool
pgy_codegen_ast_number_is_nonzero_i32_literal(const ASTNode *node)
{
    if (node == NULL || node->type != AST_NUMBER || node->data.number.is_long)
        return false;

    double value = node->data.number.value;
    int64_t as_int = (int64_t)value;
    return value != 0.0
        && value == (double)as_int
        && as_int >= INT32_MIN
        && as_int <= INT32_MAX;
}
