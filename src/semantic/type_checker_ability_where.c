#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_ability_where_internal.h"
#include "type_checker_generic_diag_internal.h"

static Type *
ability_where_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

static int
ability_decl_generic_param_index(GenericParams *gp, const char *param_name)
{
    if (gp == NULL || param_name == NULL)
        return -1;

    for (size_t i = 0; i < gp->count; i++) {
        if (gp->params[i] != NULL
            && gp->params[i]->name != NULL
            && strcmp(gp->params[i]->name, param_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
ability_generic_arg_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                                    SemanticContext *ctx)
{
    Type *bound_type;
    Symbol *bound_sym;

    if (concrete_type == NULL || bound_node == NULL || ctx == NULL)
        return false;

    bound_type = ability_where_resolve_type_ref(bound_node, ctx);
    if (bound_type != NULL
        && bound_type != TYPE_UNKNOWN
        && type_satisfies_constraint(concrete_type, bound_type)) {
        return true;
    }

    if (ctx->program_root == NULL
        || bound_node->type != AST_TYPE
        || bound_node->data.type.name == NULL
        || concrete_type->name == NULL) {
        return false;
    }

    bound_sym = scope_lookup(ctx->scope, bound_node->data.type.name);
    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
        || find_ability_decl_by_name(ctx->program_root,
                                     bound_node->data.type.name) != NULL) {
        return subject_type_has_ability(ctx->program_root,
                                        concrete_type->name,
                                        bound_node);
    }

    return false;
}

bool
validate_ability_decl_where_clause_reference(ASTNode *ability_decl,
                                             ASTNode *ability_ref,
                                             const ASTNode *site,
                                             SemanticContext *ctx,
                                             const char *owner_label,
                                             const char *owner_name)
{
    GenericParams *decl_params;
    WhereClause *wc;
    char *required_text;
    ASTNode **effective_args;
    size_t effective_count = 0;

    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ability_ref == NULL || ability_ref->type != AST_TYPE
        || ctx == NULL) {
        return true;
    }

    semantic_type_resolution_record_type_ref_dependency(
        ctx,
        site != NULL ? site : ability_ref,
        owner_name != NULL ? owner_name : "<consumer>",
        ability_ref,
        "ability consumer lookup");

    decl_params = ability_decl->data.ability_decl.generic_params;
    wc = ability_decl->data.ability_decl.where_clause;
    if (decl_params == NULL || decl_params->count == 0
        || wc == NULL || wc->count == 0) {
        return true;
    }

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        ability_ref->data.type.generic_args,
        site,
        ctx,
        "Ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name
            : "<ability>",
        &effective_count);
    if (effective_args == NULL)
        return !ctx->has_error;

    required_text = ability_ref_effective_display(ability_decl, ability_ref);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = ability_decl_generic_param_index(decl_params, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= effective_count) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_BOUND_VALIDATION_FAILED,
                PGY_FIX_ALIGN_GENERIC_BOUND_OR_ANNOTATE,
                site,
                "%s '%s' could not validate generic bound '%s' on ability '%s'.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- ability where-clause references type parameter '%s'\n"
                "- effective generic arguments were not materialized for that parameter\n"
                "Fix:\n"
                "- pass/supply generic arguments for '%s'\n"
                "- or fix the ability declaration so where-clause parameters match its generic parameter list",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>");
            free(required_text);
            free(effective_args);
            return false;
        }

        concrete_type = ability_where_resolve_type_ref(
            effective_args[param_index], ctx);
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' could not resolve generic argument for bound '%s' on ability '%s'.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- effective generic argument for '%s' did not resolve to a concrete type\n"
                "Fix:\n"
                "- provide a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so the generic argument resolves",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                tc->type_param);
            free(required_text);
            free(effective_args);
            return false;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (!ability_generic_arg_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_report_ability_generic_bound_failure(
                    ctx,
                    site,
                    owner_label,
                    owner_name,
                    ability_decl->data.ability_decl.name != NULL
                        ? ability_decl->data.ability_decl.name : "<ability>",
                    tc->type_param,
                    bound_name,
                    bounds_text,
                    required_text,
                    concrete_type->name != NULL ? concrete_type->name : "<type>");
                free(bounds_text);
                free(required_text);
                free(effective_args);
                return false;
            }
            free(bounds_text);
        }
    }

    free(required_text);
    free(effective_args);
    return true;
}
