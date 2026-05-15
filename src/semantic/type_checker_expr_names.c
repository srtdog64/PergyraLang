/*
 * Expression name/path helpers shared by the semantic expression pipeline.
 */

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"

char *
flatten_static_member_access(const ASTNode *expr, char separator)
{
    if (expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER)
        return ast_identifier_name(expr) != NULL
            ? pergyra_strdup(ast_identifier_name(expr)) : NULL;

    if (expr->type != AST_MEMBER_ACCESS
        || ast_member_object(expr) == NULL
        || ast_member_name(expr) == NULL) {
        return NULL;
    }

    char *lhs = flatten_static_member_access(ast_member_object(expr), separator);
    if (lhs == NULL)
        return NULL;

    size_t lhs_len = strlen(lhs);
    const char *member_name = ast_member_name(expr);
    size_t rhs_len = strlen(member_name);
    char *result = malloc(lhs_len + rhs_len + 2);
    if (result == NULL) {
        free(lhs);
        return NULL;
    }

    memcpy(result, lhs, lhs_len);
    result[lhs_len] = separator;
    memcpy(result + lhs_len + 1, member_name, rhs_len);
    result[lhs_len + rhs_len + 1] = '\0';
    free(lhs);
    return result;
}

Symbol *
lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || expr->type != AST_IDENTIFIER
        || ast_identifier_name(expr) == NULL) {
        return NULL;
    }
    return scope_lookup(ctx->scope, ast_identifier_name(expr));
}

bool
consume_qubit_value(ASTNode *expr, SemanticContext *ctx, const char *action)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL
        || !type_is_general_boundary_type(sym->type, ctx))
        return false;

    if (sym->is_consumed) {
        return false;
    }

    sym->is_consumed = true;
    sym->is_used = true;
    (void)action;
    return true;
}
