/*
 * Expression operator and indexed-access type checkers.
 *
 * Kept out of type_checker_expr.inc so the expression dispatcher stays
 * readable while operator overload and array literal rules remain together.
 */

#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "diag_codes.h"

#include "type_checker_operator_expr.inc"

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = type_check_expression(expr->data.binary.left,  ctx);
    Type *right = type_check_expression(expr->data.binary.right, ctx);

    if (type_is_slot_handle(left) && left->data.slot.inner_type != NULL)
        left = left->data.slot.inner_type;
    if (type_is_slot_handle(right) && right->data.slot.inner_type != NULL)
        right = right->data.slot.inner_type;

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        PgyTokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    PgyTokenType op = expr->data.binary.op.type;
    if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
        || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
        || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL) {
        if (!type_equals(left, right)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
                PGY_CAUSE_BINOP_OPERAND_TYPES,
                PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
                expr,
                "Cannot compare '%s' and '%s'",
                left->name, right->name);
        }
        return TYPE_BOOL;
    }

    if (!type_equals(left, right)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BINOP_TYPE_MISMATCH,
            PGY_CAUSE_BINOP_OPERAND_TYPES,
            PGY_FIX_ALIGN_OPERAND_TYPES_OR_OVERLOAD,
            expr,
            "Type mismatch in binary operation: '%s' and '%s'",
            left->name, right->name);
        return TYPE_UNKNOWN;
    }

    return left;
}

Type *
type_check_unary(ASTNode *expr, SemanticContext *ctx)
{
    Type *operand = type_check_expression(expr->data.unary.operand, ctx);

    PgyTokenType op = expr->data.unary.op.type;
    if (op == TOKEN_NOT) {
        if (!type_equals(operand, TYPE_BOOL)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "'!' operator requires Bool, got '%s'", operand->name);
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_MINUS) {
        if (!type_equals(operand, TYPE_INT)
            && !type_equals(operand, TYPE_FLOAT)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "Unary '-' requires numeric type, got '%s'",
                operand->name);
        }
        return operand;
    }

    if (op == TOKEN_QUESTION) {
        if (!type_is_constructed_named(operand, "Result")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNOP_TYPE_MISMATCH,
                PGY_CAUSE_UNARY_OPERATOR_OPERAND, PGY_FIX_ALIGN_OPERAND_TYPE,
                expr,
                "'?' operator requires Result<T> or Result<T, E>, got '%s'",
                operand->name);
            return TYPE_UNKNOWN;
        }
        return type_get_constructed_arg(operand, 0);
    }

    return operand;
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = type_check_expression(expr->data.array_access.array, ctx);
    Type *index_type  = type_check_expression(expr->data.array_access.index, ctx);

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_ACCESS_INDEX_NON_INT, PGY_FIX_USE_INT_INDEX,
            expr->data.array_access.index,
            "Array index must be Int, got '%s'", index_type->name);
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return type_get_constructed_arg(object_type, 0);
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ARRAY_ACCESS_TARGET_NOT_INDEXABLE, PGY_FIX_USE_ARRAY_OR_SLICE,
        expr->data.array_access.array,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        object_type->name);
    return TYPE_UNKNOWN;
}
