#include <stdlib.h>

#include "diag_codes.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_ability_where_internal.h"
#include "type_checker_module_contract_diag_internal.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_visibility.h"

static Type *
module_contract_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx,
                                                                   type_ref);
}

ASTNode *
resolve_required_ability_decl(ASTNode *ability_ref,
                              const ASTNode *site,
                              SemanticContext *ctx,
                              const char *owner_label,
                              const char *owner_name)
{
    const char *ability = ability_ref_name(ability_ref);
    ASTNode *ability_decl;
    char *required_text = ability_ref_display(ability_ref);
    const char *site_module = NULL;

    semantic_type_resolution_record_type_ref_dependency(
        ctx,
        site != NULL ? site : ability_ref,
        owner_name != NULL ? owner_name : "<consumer>",
        ability_ref,
        "required ability resolution");

    module_contract_resolve_type_ref(ability_ref, ctx);
    if (ability == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, site,
            "%s '%s' requires a valid ability type reference.\n"
            "Reason:\n"
            "- this contract path expects an ability type\n"
            "- the current reference is missing, malformed, or not an ability surface\n"
            "Fix:\n"
            "- replace it with a visible ability type such as 'AbilityName' or 'AbilityName<T>'\n"
            "- or remove the invalid requirement from %s '%s'",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>");
        free(required_text);
        return NULL;
    }

    ability_decl = find_ability_decl_by_name(ctx->program_root, ability);
    if (ability_decl == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, site,
            "%s '%s' requires unknown ability '%s'.\n"
            "Reason:\n"
            "- no visible ability declaration named '%s' was found\n"
            "Fix:\n"
            "- declare ability '%s'\n"
            "- or import/export the module that defines it",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability,
            ability,
            ability);
        free(required_text);
        return NULL;
    }

    {
        char *effective_required_text =
            ability_ref_effective_display(ability_decl, ability_ref);
        if (effective_required_text != NULL) {
            free(required_text);
            required_text = effective_required_text;
        }
    }

    site_module = (site != NULL && site->origin_path != NULL)
        ? site->origin_path
        : (ctx != NULL ? ctx->current_module_path : NULL);
    if (ability_decl->type == AST_ABILITY_DECL
        && ((site_module != NULL
             && ability_decl->origin_path != NULL
             && !same_module_origin(site_module, ability_decl->origin_path)
             && !ability_decl->is_exported)
            || ((site_module == NULL
                 || ability_decl->origin_path == NULL
                || !same_module_origin(site_module, ability_decl->origin_path))
                && ability_decl->data.ability_decl.has_explicit_access
                && ability_decl->data.ability_decl.access != ACCESS_PUBLIC))) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY,
            PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER,
            site,
            "%s '%s' cannot require non-exported ability '%s' from another module.\n"
            "Reason:\n"
            "- consumer path is %s '%s'\n"
            "- ability reference is '%s'\n"
            "- the declaration exists but is not visible across the current module boundary\n"
            "Fix:\n"
            "- export ability '%s' from its defining module\n"
            "- or move this contract to the same module\n"
            "- or require a public ability instead",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability,
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            required_text != NULL ? required_text : ability,
            ability);
        free(required_text);
        return NULL;
    }

    if (ability_ref->type == AST_TYPE
        && ability_ref->data.type.generic_args != NULL) {
        size_t arg_count = ability_ref->data.type.generic_args->count;
        size_t param_count = ability_decl->data.ability_decl.generic_params != NULL
            ? ability_decl->data.ability_decl.generic_params->count : 0;
        char *expected_text = ability_decl_signature_display(
            ability, ability_decl->data.ability_decl.generic_params);

        if (arg_count > 0 && param_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, site,
                "Ability '%s' does not accept generic type arguments in requires clauses.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- generic subject is ability '%s'\n"
                "- requires uses '%s'\n"
                "- ability declaration '%s' is non-generic\n"
                "- expected type args are '%s'\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- remove the generic arguments from '%s'\n"
                "- or declare '%s' with matching generic parameters",
                ability,
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                ability,
                required_text != NULL ? required_text : ability,
                ability,
                expected_text != NULL ? expected_text : ability,
                required_text != NULL ? required_text : ability,
                ability,
                ability);
            free(expected_text);
            free(required_text);
            return NULL;
        }

        if (arg_count > param_count
            || arg_count < generic_params_required_count(
                ability_decl->data.ability_decl.generic_params)) {
            size_t required_count = generic_params_required_count(
                ability_decl->data.ability_decl.generic_params);
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, site,
                "Ability '%s' requires between %llu and %llu generic argument(s) in requires clauses, got %llu.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- requires uses '%s'\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- pass enough generic argument(s) to cover non-default parameters of '%s'\n"
                "- or rely on declaration defaults for trailing parameters only\n"
                "- or change the ability declaration to match the intended arity",
                ability,
                (unsigned long long) required_count, (unsigned long long) param_count, (unsigned long long) arg_count,
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                required_text != NULL ? required_text : ability,
                required_text != NULL ? required_text : ability,
                ability);
            free(expected_text);
            free(required_text);
            return NULL;
        }

        for (size_t i = 0; i < arg_count; i++) {
            GenericParam *gp = ability_ref->data.type.generic_args->params[i];
            ASTNode *arg = gp != NULL ? gp->constraint : NULL;
            if (arg == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, site,
                    "Ability '%s' has an invalid generic argument in requires clause.\n"
                    "Reason:\n"
                    "- consumer path is %s '%s'\n"
                    "- generic subject is ability '%s'\n"
                    "- generic argument %llu in '%s' is missing or malformed\n"
                    "- expected type args are '%s'\n"
                    "- actual type args are '%s'\n"
                    "Fix:\n"
                    "- provide a concrete type argument for each generic parameter",
                    ability,
                    owner_label != NULL ? owner_label : "construct",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    ability, (unsigned long long) (i + 1),
                    required_text != NULL ? required_text : ability,
                    expected_text != NULL ? expected_text : ability,
                    required_text != NULL ? required_text : ability);
                free(expected_text);
                free(required_text);
                return NULL;
            }
            module_contract_resolve_type_ref(arg, ctx);
        }

        free(expected_text);
    }

    if (!validate_ability_decl_where_clause_reference(
            ability_decl, ability_ref, site, ctx, owner_label, owner_name)) {
        free(required_text);
        return NULL;
    }

    free(required_text);
    return ability_decl;
}

void
validate_action_required_abilities(ASTNode *node,
                                   ASTNode *enclosing_nominal,
                                   SemanticContext *ctx)
{
    const char *name;
    const char *subject_name = NULL;

    if (node == NULL || node->type != AST_FUNC_DECL || ctx == NULL)
        return;

    name = node->data.func_decl.name;

    if (enclosing_nominal == NULL
        || enclosing_nominal->type != AST_CLASS_DECL
        || enclosing_nominal->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
            "action '%s' is only supported inside subject declarations",
            name != NULL ? name : "<anonymous>");
    } else {
        subject_name = enclosing_nominal->data.class_decl.name;
    }

    for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
        ASTNode *ability_ref = node->data.func_decl.required_abilities[i];
        const char *ability = ability_ref_name(ability_ref);
        char *required_text = ability_ref_display(ability_ref);
        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            node,
            name != NULL ? name : "<action>",
            ability_ref,
            "action ability consumer lookup");
        ASTNode *ability_decl = resolve_required_ability_decl(
            ability_ref, node, ctx, "action", name);
        if (ability_decl == NULL)
        {
            free(required_text);
            continue;
        }
        {
            char *effective_required_text =
                ability_ref_effective_display(ability_decl, ability_ref);
            if (effective_required_text != NULL) {
                free(required_text);
                required_text = effective_required_text;
            }
        }
        if (ability_decl->type == AST_ABILITY_DECL
            && !explicit_type_reference_allowed(ability_decl, node, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' cannot require non-exported ability '%s' from another module",
                name != NULL ? name : "<anonymous>",
                ability != NULL ? ability : "<ability>");
            free(required_text);
            continue;
        }
        if (subject_name != NULL
            && !subject_type_has_ability(ctx->program_root, subject_name, ability_ref)) {
            ASTNode *actual_impl = subject_type_find_base_ability_impl(
                ctx->program_root, subject_name, ability);
            char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
            report_subject_ability_requirement_mismatch(
                ctx, node, "action", name != NULL ? name : "<anonymous>",
                "subject", subject_name,
                required_text != NULL ? required_text
                                      : (ability != NULL ? ability : "<ability>"),
                actual_text,
                "change/remove the action requirement");
            free(actual_text);
        }
        free(required_text);
    }
}
