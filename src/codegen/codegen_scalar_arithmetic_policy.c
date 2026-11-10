/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared scalar arithmetic lowering policy for C and LLVM backends.
 */

#include "codegen_scalar_arithmetic_policy.h"

#include <stdint.h>

#include "../parser/ast.h"
#include "../parser/ast_api.h"

bool
pgy_codegen_i32_divisor_is_safe(int64_t value)
{
    return value != 0 && value != -1
        && value >= INT32_MIN && value <= INT32_MAX;
}

bool
pgy_codegen_ast_number_is_safe_divisor_i32_literal(const ASTNode *node)
{
    if (node == NULL || node->type != AST_NUMBER || ast_number_is_long(node))
        return false;
    if (ast_number_is_float(node))
        return false;

    double value = ast_number_value(node);
    int64_t as_int = (int64_t)value;
    /* Safe to skip the checked-division helper only when the literal divisor
     * can neither divide by zero nor trigger INT_MIN / -1 signed overflow.
     * A literal -1 is nonzero but still unsafe (INT_MIN / -1 is UB), so it
     * must go through the checked helper. */
    return value == (double)as_int && pgy_codegen_i32_divisor_is_safe(as_int);
}
