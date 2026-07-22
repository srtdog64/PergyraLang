#include "transpiler_expr_core_emit.h"

#include <stdlib.h>
#include <string.h>

#include "codegen_match_variant_policy.h"
#include "codegen_scalar_arithmetic_policy.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_operator.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

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

static char *
transpiler_binary_emit_operand(TranspilerCtx *ctx,
                               ASTNode *operand,
                               const char *role)
{
    char *lowered = emit_expression(operand, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: binary expression could not lower %s operand",
        role != NULL ? role : "unknown");
    return NULL;
}

char *
emit_binary(ASTNode *expr, TranspilerCtx *ctx)
{
    ASTNode *left_expr = ast_binary_left(expr);
    ASTNode *right_expr = ast_binary_right(expr);
    PgyTokenType op_type = ast_binary_operator(expr).type;

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
                const char *left_leaf_t =
                    infer_expression_type_name(ctx, cursor_left);
                const char *right_leaf_t =
                    infer_expression_type_name(ctx, cursor_right);
                if ((cursor_left != NULL && cursor_left->type == AST_STRING)
                    || (cursor_right != NULL
                        && cursor_right->type == AST_STRING)) {
                    is_string_literal_chain = true;
                }
                if ((left_leaf_t != NULL && strcmp(left_leaf_t, "String") == 0)
                    || (right_leaf_t != NULL
                        && strcmp(right_leaf_t, "String") == 0)) {
                    is_string = true;
                    break;
                }
                cursor = cursor_left;
            }
        }
        if (is_string || is_string_literal_chain) {
            char *left = transpiler_binary_emit_operand(ctx, left_expr, "left");
            if (left == NULL)
                return NULL;
            char *right = transpiler_binary_emit_operand(ctx, right_expr, "right");
            if (right == NULL) {
                free(left);
                return NULL;
            }
            char *result = transpiler_region_concat(ctx, expr, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (op_type == TOKEN_EQUAL || op_type == TOKEN_NOT_EQUAL) {
        const char *lt = infer_expression_type_name(ctx, left_expr);
        const char *rt = infer_expression_type_name(ctx, right_expr);
        if ((lt != NULL && strcmp(lt, "String") == 0)
            || (rt != NULL && strcmp(rt, "String") == 0)) {
            char *left = transpiler_binary_emit_operand(ctx, left_expr, "left");
            if (left == NULL)
                return NULL;
            char *right = transpiler_binary_emit_operand(ctx, right_expr, "right");
            if (right == NULL) {
                free(left);
                return NULL;
            }
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

        overload = find_operator_overload_decl(ctx, stable_lt, op_type);
        if (overload != NULL) {
            char *left = transpiler_binary_emit_operand(ctx, left_expr, "left");
            if (left == NULL)
                return NULL;
            char *right = transpiler_binary_emit_operand(ctx, right_expr, "right");
            if (right == NULL) {
                free(left);
                return NULL;
            }
            const char *suffix = operator_overload_suffix(op_type);
            char *result = strdup_fmt("operator_%s_%s(%s, %s)",
                suffix, stable_lt, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    if (op_type == TOKEN_COALESCE) {
        char *left = transpiler_binary_emit_operand(ctx, left_expr, "left");
        if (left == NULL)
            return NULL;
        char *right = transpiler_binary_emit_operand(ctx, right_expr, "right");
        if (right == NULL) {
            free(left);
            return NULL;
        }
        const char *some_tag =
            pgy_codegen_match_variant_c_option_tag(PGY_MATCH_VARIANT_SOME);
        char *result = strdup_fmt(
            "(({ __auto_type _pgy_coalesce = %s; "
            "_pgy_coalesce.tag == %s ? _pgy_coalesce.value : (%s); }))",
            left, some_tag, right);
        free(left);
        free(right);
        return result;
    }

    char *left = transpiler_binary_emit_operand(ctx, left_expr, "left");
    if (left == NULL)
        return NULL;
    char *right = transpiler_binary_emit_operand(ctx, right_expr, "right");
    if (right == NULL) {
        free(left);
        return NULL;
    }
    const char *op = binary_op_to_c(op_type);
    char *result;
    if (op_type == TOKEN_SLASH || op_type == TOKEN_PERCENT) {
        const char *lt = infer_expression_type_name(ctx, left_expr);
        const char *rt = infer_expression_type_name(ctx, right_expr);
        bool is_float_div = (lt != NULL
                && (strcmp(lt, "Float") == 0 || strcmp(lt, "Double") == 0))
            || (rt != NULL
                && (strcmp(rt, "Float") == 0 || strcmp(rt, "Double") == 0));
        /* Duration rides the i64 lane like Long (docs/181 SS2.3); the
         * i32 checked helpers would silently truncate the operands. */
        bool is_long_div = (lt != NULL && (strcmp(lt, "Long") == 0
                || strcmp(lt, "Duration") == 0))
            || (rt != NULL && (strcmp(rt, "Long") == 0
                || strcmp(rt, "Duration") == 0));
        bool rhs_is_safe_divisor_literal =
            !is_long_div
            && pgy_codegen_ast_number_is_safe_divisor_i32_literal(right_expr);
        if (!is_float_div && !rhs_is_safe_divisor_literal) {
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
