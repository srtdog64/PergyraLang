#include "type_checker_internal.h"
#include "type_checker_ability_fields_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>

Type *
ability_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_ability_name(node);
    GenericParams *ability_generics = ast_ability_generic_params(node);
    WhereClause *ability_where = ast_ability_where_clause(node);
    bool has_generics = (ast_generic_param_count(ability_generics) > 0);

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        bool is_self_predecl = existing->kind == SYMBOL_ABILITY
            && existing->decl_line == node->line
            && existing->decl_col == node->column;
        if (!is_self_predecl) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_REDECLARATION,
                PGY_CAUSE_ABILITY_DUPLICATE_NAME,
                PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                node, "Redeclaration of ability '%s'", name);
            return false;
        }
    } else {
        /* Register ability as a symbol so roles can reference it. */
        Symbol *sym = calloc(1, sizeof(Symbol));
        sym->name = pergyra_strdup(name);
        sym->kind = SYMBOL_ABILITY;
        sym->type = TYPE_VOID; /* Abilities don't have a concrete type */
        sym->decl_line = node->line;
        sym->decl_col = node->column;
        scope_declare(ctx->scope, sym);
    }

    if (has_generics) {
        validate_generic_param_defaults(ability_generics, ctx, node, "ability");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = ability_generics;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count; gi++) {
            GenericParam *param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(param);
            if (param_name == NULL)
                continue;
            Type *tp = type_create_generic(param_name);
            Symbol *s = symbol_create_variable(
                param_name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_TYPE_PARAM;
            scope_declare(ctx->scope, s);
        }
    }

    validate_where_clause_bounds(ability_where, ctx, node);
    validate_generic_param_default_bounds(
        ability_generics,
        ability_where,
        ctx,
        node,
        "ability",
        name);
    validate_ability_require_fields(node, ctx);

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < ast_ability_method_count(node); i++) {
        ASTNode *method = ast_ability_method(node, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        /* Only type-check methods that have a body */
        if (ast_func_body(method) != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (ast_func_return_type(method) != NULL)
                ability_resolve_type_ref(
                    ast_func_return_type(method), ctx);
            for (size_t j = 0; j < ast_func_param_count(method); j++) {
                FuncParam *param = ast_func_param(method, j);
                if (param != NULL && param->type != NULL)
                    ability_resolve_type_ref(
                        param->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);
    if (has_generics)
        scope_exit(&ctx->scope);

    return !ctx->has_error;
}
