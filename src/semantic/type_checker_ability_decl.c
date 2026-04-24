#include "type_checker_internal.h"
#include "type_checker_ability_fields_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>

static Type *
ability_decl_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_resolve_or_fallback(ctx, type_ref);
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.ability_decl.name;
    bool has_generics = (node->data.ability_decl.generic_params != NULL
                         && node->data.ability_decl.generic_params->count > 0);

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
        validate_generic_param_defaults(node->data.ability_decl.generic_params,
            ctx, node, "ability");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.ability_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    validate_where_clause_bounds(node->data.ability_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.ability_decl.generic_params,
        node->data.ability_decl.where_clause,
        ctx,
        node,
        "ability",
        name);
    validate_ability_require_fields(node, ctx);

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        /* Only type-check methods that have a body */
        if (method->data.func_decl.body != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (method->data.func_decl.return_type != NULL)
                ability_decl_resolve_type_ref(
                    method->data.func_decl.return_type, ctx);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                if (method->data.func_decl.params[j]->type != NULL)
                    ability_decl_resolve_type_ref(
                        method->data.func_decl.params[j]->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);
    if (has_generics)
        scope_exit(&ctx->scope);

    return !ctx->has_error;
}
