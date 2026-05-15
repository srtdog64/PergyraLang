#include <stdbool.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static Type *
generic_validation_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
}

void
validate_where_clause_bounds(WhereClause *wc, SemanticContext *ctx, ASTNode *owner)
{
    if (wc == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < wc->count; i++) {
        TypeConstraint *tc = wc->constraints[i];
        if (tc == NULL)
            continue;
        for (size_t b = 0; b < tc->bound_count; b++) {
            if (tc->bounds[b] != NULL) {
                Symbol *bound_sym = NULL;
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    tc->bounds[b],
                    "where-bound lookup");
                size_t saved_diag = ctx->diagnostic_count;
                bool saved_err = ctx->has_error;
                Type *bound_type = generic_validation_resolve_type_ref(
                    tc->bounds[b], ctx);
                if (ast_type_name(tc->bounds[b]) != NULL) {
                    bound_sym = scope_lookup(ctx->scope, ast_type_name(tc->bounds[b]));
                }
                if (ctx->diagnostic_count > saved_diag
                    || bound_type == NULL
                    || bound_type == TYPE_UNKNOWN) {
                    ctx->diagnostic_count = saved_diag;
                    ctx->has_error = saved_err;
                    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
                        || (ast_type_name(tc->bounds[b]) != NULL
                            && ctx->program_root != NULL
                            && find_ability_decl_by_name(
                                   ctx->program_root,
                                   ast_type_name(tc->bounds[b])) != NULL)) {
                        continue;
                    }
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
                        PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_DECLARE_OR_IMPORT_TYPE,
                        owner != NULL ? owner : tc->bounds[b],
                        "Unknown constraint type '%s' in where clause.\n"
                        "Reason:\n"
                        "- generic where-clause validation could not resolve this bound\n"
                        "- every bound in a multi-bound contract must resolve before specialization can be trusted\n"
                        "Fix:\n"
                        "- declare or import '%s'\n"
                        "- or remove the unresolved bound from the where-clause",
                        ast_type_name(tc->bounds[b]) != NULL
                            ? ast_type_name(tc->bounds[b]) : "<type>",
                        ast_type_name(tc->bounds[b]) != NULL
                            ? ast_type_name(tc->bounds[b]) : "<type>");
                }
            }
        }
    }
}

void
validate_generic_param_defaults(GenericParams *gp, SemanticContext *ctx,
                                ASTNode *owner, const char *kind_name)
{
    bool saw_default = false;

    if (gp == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *param = gp->params[i];
        if (param == NULL)
            continue;
        if (param->default_type != NULL) {
            saw_default = true;
        } else if (saw_default) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_NON_TRAILING_DEFAULT,
                PGY_FIX_MOVE_DEFAULTS_TO_TRAILING,
                owner != NULL ? owner : (ASTNode *)gp,
                "Non-trailing default generic parameter '%s' in %s declaration.\n"
                "Reason:\n"
                "- a required generic parameter appears after a parameter with a default\n"
                "- generic defaults are only closed for trailing parameters\n"
                "Fix:\n"
                "- move '%s' before all defaulted generic parameters\n"
                "- or give '%s' a default type argument too",
                param->name != NULL ? param->name : "<type-param>",
                kind_name != NULL ? kind_name : "generic",
                param->name != NULL ? param->name : "<type-param>",
                param->name != NULL ? param->name : "<type-param>");
        }
        if (param == NULL || param->default_type == NULL)
            continue;
        {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                owner != NULL ? owner : param->default_type,
                param->name != NULL ? param->name : "<type-param>",
                param->default_type,
                "default-type lookup");
            size_t saved_diag = ctx->diagnostic_count;
            bool saved_err = ctx->has_error;
            Type *resolved = generic_validation_resolve_type_ref(
                param->default_type, ctx);
            if (resolved == NULL || resolved == TYPE_UNKNOWN
                || ctx->diagnostic_count > saved_diag) {
                ctx->diagnostic_count = saved_diag;
                ctx->has_error = saved_err;
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                    PGY_CAUSE_GENERIC_DEFAULT_UNRESOLVED,
                    PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                    owner != NULL ? owner : param->default_type,
                    "Invalid default generic type argument '%s' in %s declaration (parameter '%s').\n"
                    "Reason:\n"
                    "- the declared default type could not be resolved as a valid concrete type\n"
                    "- generic defaults must be fully valid before they can participate in effective-argument derivation\n"
                    "Fix:\n"
                    "- replace '%s' with a resolvable concrete type\n"
                    "- or remove the default and require the caller to supply it",
                    ast_type_name(param->default_type) != NULL
                        ? ast_type_name(param->default_type)
                        : "<type>",
                    kind_name != NULL ? kind_name : "generic",
                    param->name != NULL ? param->name : "<type-param>",
                    ast_type_name(param->default_type) != NULL
                        ? ast_type_name(param->default_type)
                        : "<type>");
            }
        }
    }
}
