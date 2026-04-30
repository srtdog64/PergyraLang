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
    /* String concatenation: detect String anywhere in the + chain so
     * parameter/local-backed cases like `name + "..."` lower correctly. */
    if (expr->data.binary.op.type == TOKEN_PLUS) {
        bool is_string_literal_chain = false;
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, expr->data.binary.right);
        bool is_string =
            (lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0);
        if (expr->data.binary.left != NULL && expr->data.binary.left->type == AST_STRING)
            is_string_literal_chain = true;
        if (expr->data.binary.right != NULL && expr->data.binary.right->type == AST_STRING)
            is_string_literal_chain = true;

        if (!is_string) {
            ASTNode *cursor = expr;
            while (cursor != NULL && cursor->type == AST_BINARY
                   && cursor->data.binary.op.type == TOKEN_PLUS) {
                const char *left_leaf_t = infer_expression_type_name(ctx, cursor->data.binary.left);
                const char *right_leaf_t = infer_expression_type_name(ctx, cursor->data.binary.right);
                if ((cursor->data.binary.left != NULL
                        && cursor->data.binary.left->type == AST_STRING)
                    || (cursor->data.binary.right != NULL
                        && cursor->data.binary.right->type == AST_STRING)) {
                    is_string_literal_chain = true;
                }
                if ((left_leaf_t != NULL && strcmp(left_leaf_t, "String") == 0)
                    || (right_leaf_t != NULL && strcmp(right_leaf_t, "String") == 0)) {
                    is_string = true;
                    break;
                }
                cursor = cursor->data.binary.left;
            }
        }
        if (is_string || is_string_literal_chain) {
            char *left  = emit_expression(expr->data.binary.left,  ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (expr->data.binary.op.type == TOKEN_EQUAL
        || expr->data.binary.op.type == TOKEN_NOT_EQUAL) {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, expr->data.binary.right);
        if ((lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0)) {
            char *left = emit_expression(expr->data.binary.left, ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            char *result = NULL;
            if (expr->data.binary.op.type == TOKEN_EQUAL)
                result = strdup_fmt("pgy_string_equals(%s, %s)", left, right);
            else
                result = strdup_fmt("(!pgy_string_equals(%s, %s))", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        char lt_buf[128];
        const char *stable_lt = lt;
        ASTNode *overload;

        if (lt != NULL) {
            snprintf(lt_buf, sizeof(lt_buf), "%s", lt);
            stable_lt = lt_buf;
        }

        overload = find_operator_overload_decl(ctx, stable_lt,
            expr->data.binary.op.type);
        if (overload != NULL) {
            char *left  = emit_expression(expr->data.binary.left, ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
            char *result = strdup_fmt("operator_%s_%s(%s, %s)",
                suffix, stable_lt, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    char *left  = emit_expression(expr->data.binary.left,  ctx);
    char *right = emit_expression(expr->data.binary.right, ctx);
    const char *op = binary_op_to_c(expr->data.binary.op.type);
    char *result;
    if (expr->data.binary.op.type == TOKEN_SLASH
        || expr->data.binary.op.type == TOKEN_PERCENT) {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *rt = infer_expression_type_name(ctx, expr->data.binary.right);
        bool is_float_div = (lt != NULL
                && (strcmp(lt, "Float") == 0 || strcmp(lt, "Double") == 0))
            || (rt != NULL
                && (strcmp(rt, "Float") == 0 || strcmp(rt, "Double") == 0));
        bool is_long_div = (lt != NULL && strcmp(lt, "Long") == 0)
            || (rt != NULL && strcmp(rt, "Long") == 0);
        if (!is_float_div) {
            const char *helper = expr->data.binary.op.type == TOKEN_SLASH
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
