#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H

#include "codegen_scalar_arithmetic_policy.h"
#include "transpiler_expr_core_builtins_emit.h"

static const char *
binary_op_to_c(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "+";
    case TOKEN_MINUS:         return "-";
    case TOKEN_STAR:          return "*";
    case TOKEN_SLASH:         return "/";
    case TOKEN_PERCENT:       return "%";
    case TOKEN_EQUAL:         return "==";
    case TOKEN_NOT_EQUAL:     return "!=";
    case TOKEN_LESS:          return "<";
    case TOKEN_LESS_EQUAL:    return "<=";
    case TOKEN_GREATER:       return ">";
    case TOKEN_GREATER_EQUAL: return ">=";
    case TOKEN_AND:           return "&&";
    case TOKEN_OR:            return "||";
    default:                  return "?";
    }
}

char *
emit_binary(ASTNode *expr, TranspilerCtx *ctx)
{
    ASTNode *left_expr = ast_binary_left(expr);
    ASTNode *right_expr = ast_binary_right(expr);
    PgyTokenType op_type = ast_binary_operator(expr).type;

    /* String concatenation: detect String anywhere in the + chain so
     * parameter/local-backed cases like `name + "..."` lower correctly. */
    if (op_type == TOKEN_PLUS) {
        bool is_string_literal_chain = false;
        const char *lt = infer_expression_type_name(ctx, left_expr);
        const char *rt = infer_expression_type_name(ctx, right_expr);
        bool is_string =
            (lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0);
        if (left_expr != NULL && left_expr->type == AST_STRING)
            is_string_literal_chain = true;
        if (right_expr != NULL && right_expr->type == AST_STRING)
            is_string_literal_chain = true;

        if (!is_string) {
            ASTNode *cursor = expr;
            while (cursor != NULL && cursor->type == AST_BINARY
                   && ast_binary_operator(cursor).type == TOKEN_PLUS) {
                ASTNode *cursor_left = ast_binary_left(cursor);
                ASTNode *cursor_right = ast_binary_right(cursor);
                const char *left_leaf_t = infer_expression_type_name(ctx, cursor_left);
                const char *right_leaf_t = infer_expression_type_name(ctx, cursor_right);
                if ((cursor_left != NULL
                        && cursor_left->type == AST_STRING)
                    || (cursor_right != NULL
                        && cursor_right->type == AST_STRING)) {
                    is_string_literal_chain = true;
                }
                if ((left_leaf_t != NULL && strcmp(left_leaf_t, "String") == 0)
                    || (right_leaf_t != NULL && strcmp(right_leaf_t, "String") == 0)) {
                    is_string = true;
                    break;
                }
                cursor = cursor_left;
            }
        }
        if (is_string || is_string_literal_chain) {
            char *left  = emit_expression(left_expr,  ctx);
            char *right = emit_expression(right_expr, ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (op_type == TOKEN_EQUAL
        || op_type == TOKEN_NOT_EQUAL) {
        const char *lt = infer_expression_type_name(ctx, left_expr);
        const char *rt = infer_expression_type_name(ctx, right_expr);
        if ((lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0)) {
            char *left = emit_expression(left_expr, ctx);
            char *right = emit_expression(right_expr, ctx);
            char *result = NULL;
            if (op_type == TOKEN_EQUAL)
                result = strdup_fmt("pgy_string_equals(%s, %s)", left, right);
            else
                result = strdup_fmt("(!pgy_string_equals(%s, %s))", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    {
        const char *lt = infer_expression_type_name(ctx, left_expr);
        char lt_buf[128];
        const char *stable_lt = lt;
        ASTNode *overload;

        if (lt != NULL) {
            snprintf(lt_buf, sizeof(lt_buf), "%s", lt);
            stable_lt = lt_buf;
        }

        overload = find_operator_overload_decl(ctx, stable_lt,
            op_type);
        if (overload != NULL) {
            char *left  = emit_expression(left_expr, ctx);
            char *right = emit_expression(right_expr, ctx);
            const char *suffix = operator_overload_suffix(op_type);
            char *result = strdup_fmt("operator_%s_%s(%s, %s)",
                suffix, stable_lt, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (op_type == TOKEN_COALESCE) {
        char *left = emit_expression(left_expr, ctx);
        char *right = emit_expression(right_expr, ctx);
        char *result = strdup_fmt(
            "(({ __auto_type _pgy_coalesce = %s; "
            "_pgy_coalesce.tag == PgyOptionSome ? _pgy_coalesce.value : (%s); }))",
            left, right);
        free(left);
        free(right);
        return result;
    }

    char *left  = emit_expression(left_expr,  ctx);
    char *right = emit_expression(right_expr, ctx);
    const char *op = binary_op_to_c(op_type);
    char *result;
    if (op_type == TOKEN_SLASH
        || op_type == TOKEN_PERCENT) {
        const char *lt = infer_expression_type_name(ctx, left_expr);
        const char *rt = infer_expression_type_name(ctx, right_expr);
        bool is_float_div = (lt != NULL
                && (strcmp(lt, "Float") == 0 || strcmp(lt, "Double") == 0))
            || (rt != NULL
                && (strcmp(rt, "Float") == 0 || strcmp(rt, "Double") == 0));
        bool is_long_div = (lt != NULL && strcmp(lt, "Long") == 0)
            || (rt != NULL && strcmp(rt, "Long") == 0);
        bool rhs_is_nonzero_literal =
            !is_long_div
            && pgy_codegen_ast_number_is_nonzero_i32_literal(
                right_expr);
        if (!is_float_div && !rhs_is_nonzero_literal) {
            const char *helper = op_type == TOKEN_SLASH
                ? (is_long_div ? "pgy_checked_div_i64_export"
                               : "pgy_checked_div_i32_export")
                : (is_long_div ? "pgy_checked_mod_i64_export"
                               : "pgy_checked_mod_i32_export");
            result = strdup_fmt("%s(%s, %s)", helper, left, right);
        } else {
            result = strdup_fmt("(%s %s %s)", left, op, right);
        }
    } else {
        result = strdup_fmt("(%s %s %s)", left, op, right);
    }
    free(left);
    free(right);
    return result;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H */
