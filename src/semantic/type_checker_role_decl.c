#include "type_checker_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_ability_where_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static Type *
role_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

bool
type_check_role_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.role_decl.name;

    /* Register role as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROLE;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_ROLE_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of role '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    if (node->data.role_decl.generic_params != NULL
        && node->data.role_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.role_decl.generic_params,
            ctx, node, "role");
    }

    /* Check for_type exists */
    if (node->data.role_decl.for_type != NULL) {
        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            node,
            name != NULL ? name : "<role>",
            node->data.role_decl.for_type,
            "role host-type lookup");
        Type *bound_type = role_resolve_type_ref(
            node->data.role_decl.for_type, ctx);
        if (bound_type != NULL
            && node->data.role_decl.for_type->type == AST_TYPE
            && node->data.role_decl.for_type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, node->data.role_decl.for_type->data.type.name);
            if (type_decl != NULL && !decl_is_subject_host(type_decl)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, node->data.role_decl.for_type,
                    "Role '%s' must be bound to a subject or primitive domain; '%s' is not a subject",
                    name,
                    node->data.role_decl.for_type->data.type.name);
            }
        }
    }

    validate_where_clause_bounds(node->data.role_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.role_decl.generic_params,
        node->data.role_decl.where_clause,
        ctx,
        node,
        "role",
        name);

    /* Check includes reference existing roles */
    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        const char *role_name = inc->data.include_stmt.role_name;
        semantic_type_resolution_record_named_dependency(
            ctx,
            inc,
            name != NULL ? name : "<role>",
            TYPE_RES_NODE_DECL,
            NULL,
            role_name,
            "role include lookup");
        Symbol *role_sym = scope_lookup(ctx->scope, role_name);
        if (role_sym == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, inc,
                "Role '%s' includes unknown role '%s'.\n"
                "Reason:\n"
                "- include contracts must reference a visible role declaration\n"
                "- no role named '%s' is visible in the current module/scope\n"
                "Fix:\n"
                "- declare role '%s'\n"
                "- or import/export the module that defines it",
                name != NULL ? name : "<role>",
                role_name != NULL ? role_name : "<role>",
                role_name != NULL ? role_name : "<role>",
                role_name != NULL ? role_name : "<role>");
        } else if (ctx->program_root != NULL && role_name != NULL) {
            ASTNode *included_role_decl = NULL;
            if (ctx->program_root->type == AST_PROGRAM) {
                for (size_t ri = 0; ri < ctx->program_root->data.program.count; ri++) {
                    ASTNode *candidate = ctx->program_root->data.program.statements[ri];
                    if (candidate != NULL
                        && candidate->type == AST_ROLE_DECL
                        && candidate->data.role_decl.name != NULL
                        && strcmp(candidate->data.role_decl.name, role_name) == 0) {
                        included_role_decl = candidate;
                        break;
                    }
                }
            }

            if (included_role_decl != NULL
                && included_role_decl->data.role_decl.generic_params != NULL
                && included_role_decl->data.role_decl.generic_params->count > 0) {
                size_t effective_count = 0;
                ASTNode **effective_args = collect_effective_generic_arg_nodes(
                    included_role_decl->data.role_decl.generic_params,
                    inc->data.include_stmt.type_args,
                    inc,
                    ctx,
                    "role include",
                    role_name,
                    &effective_count);
                if (effective_args != NULL
                    && included_role_decl->data.role_decl.where_clause != NULL) {
                    char *expected_text = format_generic_subject_signature(
                        role_name,
                        included_role_decl->data.role_decl.generic_params);
                    WhereClause *wc = included_role_decl->data.role_decl.where_clause;
                    for (size_t ci = 0; ci < wc->count; ci++) {
                        TypeConstraint *tc = wc->constraints[ci];
                        int param_index;
                        Type *concrete_type;
                        if (tc == NULL || tc->type_param == NULL)
                            continue;
                        param_index = find_generic_param_index(
                            included_role_decl->data.role_decl.generic_params,
                            tc->type_param);
                        if (param_index < 0 || (size_t)param_index >= effective_count)
                            continue;
                        concrete_type = role_resolve_type_ref(
                            effective_args[param_index], ctx);
                        if (concrete_type == NULL)
                            continue;
                        for (size_t bi = 0; bi < tc->bound_count; bi++) {
                            ASTNode *bound_node = tc->bounds[bi];
                            char *bounds_text = format_type_constraint_bounds(tc);
                            const char *bound_name =
                                (bound_node != NULL
                                 && bound_node->type == AST_TYPE
                                 && bound_node->data.type.name != NULL)
                                    ? bound_node->data.type.name
                                    : "<constraint>";
                            if (!concrete_type_satisfies_bound(
                                    concrete_type, bound_node, ctx)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, inc,
                                    "Role '%s' includes role '%s' with generic argument '%s' that does not satisfy bound '%s'.\n"
                                    "Reason:\n"
                                    "- include consumer path is role '%s'\n"
                                    "- included role '%s' requires '%s: %s'\n"
                                    "- full bound set is '%s: %s'\n"
                                    "- expected type args are '%s'\n"
                                    "- actual type args are specialized through include '%s'\n"
                                    "Fix:\n"
                                    "- include role '%s' with a type that satisfies '%s'\n"
                                    "- or relax the included role where-clause",
                                    name != NULL ? name : "<role>",
                                    role_name,
                                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                                    bound_name,
                                    name != NULL ? name : "<role>",
                                    role_name,
                                    tc->type_param,
                                    bound_name,
                                    tc->type_param,
                                    bounds_text != NULL ? bounds_text : "<constraint>",
                                    expected_text != NULL ? expected_text : role_name,
                                    role_name,
                                    role_name,
                                    bound_name);
                            }
                            free(bounds_text);
                        }
                    }
                    free(expected_text);
                }
                free(effective_args);
            }
        }
    }

    /* Check impl ability blocks */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                impl,
                name != NULL ? name : "<role>",
                impl->data.impl_ability.ability_ref,
                "role impl ability lookup");
            /* Check that the ability exists */
            if (impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
                const char *ability_name = impl->data.impl_ability.ability_ref->data.type.name;
                ASTNode *ability_decl = find_ability_decl_by_name(
                    ctx->program_root, ability_name);
                Symbol *ab = scope_lookup(ctx->scope,
                    ability_name);
                if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL
                           && !explicit_type_reference_allowed(ability_decl, node, ctx)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, impl,
                        "Role '%s' cannot implement non-exported ability '%s' from another module.\n"
                        "Reason:\n"
                        "- impl block references ability '%s'\n"
                        "- the declaration exists but is not visible across the current module boundary\n"
                        "Fix:\n"
                        "- export ability '%s' from its defining module\n"
                        "- or move role '%s' into the same module\n"
                        "- or implement a public ability instead",
                        name != NULL ? name : "<role>",
                        ability_name,
                        ability_name,
                        ability_name,
                        name != NULL ? name : "<role>");
                } else if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, impl,
                        "Role '%s' implements unknown ability '%s'.\n"
                        "Reason:\n"
                        "- impl block references '%s'\n"
                        "- no visible ability declaration named '%s' was found\n"
                        "Fix:\n"
                        "- declare ability '%s'\n"
                        "- or import/export the module that defines it",
                        name != NULL ? name : "<role>",
                        ability_name,
                        ability_name,
                        ability_name,
                        ability_name);
                } else if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL
                           && ability_decl->data.ability_decl.is_innate
                           && ability_decl->origin_path != NULL
                           && node->origin_path != NULL
                           && strcmp(ability_decl->origin_path,
                                     node->origin_path) != 0) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, impl,
                        "innate ability '%s' cannot be implemented outside its declaring module",
                        ability_name);
                } else if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL) {
                    size_t arg_count = impl->data.impl_ability.ability_ref->data.type.generic_args != NULL
                        ? impl->data.impl_ability.ability_ref->data.type.generic_args->count : 0;
                    size_t param_count = ability_decl->data.ability_decl.generic_params != NULL
                        ? ability_decl->data.ability_decl.generic_params->count : 0;
                    bool malformed_impl_args = false;

                    if (arg_count > 0 && param_count == 0) {
                        char *impl_text =
                            ability_ref_display(impl->data.impl_ability.ability_ref);
                        char *expected_text =
                            ability_decl_signature_display(
                                ability_name,
                                ability_decl->data.ability_decl.generic_params);
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, impl,
                            "Ability '%s' does not accept generic type arguments in impl clauses.\n"
                            "Reason:\n"
                            "- consumer path is impl ability '%s'\n"
                            "- generic subject is ability '%s'\n"
                            "- impl ability uses '%s'\n"
                            "- ability declaration '%s' is non-generic\n"
                            "- expected type args are '%s'\n"
                            "- actual type args are '%s'\n"
                            "Fix:\n"
                            "- remove the generic arguments from '%s'\n"
                            "- or declare '%s' with matching generic parameters",
                            ability_name,
                            ability_name,
                            ability_name,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name,
                            expected_text != NULL ? expected_text : ability_name,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name,
                            ability_name);
                        free(expected_text);
                        free(impl_text);
                    } else if (arg_count > param_count
                               || arg_count < generic_params_required_count(
                                      ability_decl->data.ability_decl.generic_params)) {
                        size_t required_count = generic_params_required_count(
                            ability_decl->data.ability_decl.generic_params);
                        char *impl_text =
                            ability_ref_display(impl->data.impl_ability.ability_ref);
                        char *expected_text =
                            ability_decl_signature_display(
                                ability_name,
                                ability_decl->data.ability_decl.generic_params);
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, impl,
                            "Ability '%s' requires between %llu and %llu generic argument(s) in impl clauses, got %llu.\n"
                            "Reason:\n"
                            "- consumer path is impl ability '%s'\n"
                            "- impl ability uses '%s'\n"
                            "- actual type args are '%s'\n"
                            "Fix:\n"
                            "- pass enough generic argument(s) to cover non-default parameters of '%s'\n"
                            "- or rely on declaration defaults for trailing parameters only\n"
                            "- or change the ability declaration to match the intended arity",
                            ability_name != NULL ? ability_name : "<ability>",
                            (unsigned long long) required_count, (unsigned long long) param_count, (unsigned long long) arg_count,
                            ability_name != NULL ? ability_name : "<ability>",
                            impl_text != NULL ? impl_text : ability_name,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name != NULL ? ability_name : "<ability>");
                        free(expected_text);
                        free(impl_text);
                    } else {
                        for (size_t k = 0; k < arg_count; k++) {
                            GenericParam *gp =
                                impl->data.impl_ability.ability_ref->data.type.generic_args->params[k];
                            ASTNode *arg = gp != NULL ? gp->constraint : NULL;
                            if (arg == NULL) {
                                char *impl_text =
                                    ability_ref_display(impl->data.impl_ability.ability_ref);
                                char *expected_text =
                                    ability_decl_signature_display(
                                        ability_name,
                                        ability_decl->data.ability_decl.generic_params);
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_ABILITY_CONTRACT_INVALID, PGY_CAUSE_ABILITY_CONTRACT, PGY_FIX_ALIGN_ABILITY_GENERICS_OR_FIELDS, impl,
                                    "Ability '%s' has an invalid generic argument in impl clause.\n"
                                    "Reason:\n"
                                    "- consumer path is impl ability '%s'\n"
                                    "- generic subject is ability '%s'\n"
                                    "- generic argument %llu in '%s' is missing or malformed\n"
                                    "- impl contract cannot derive effective type arguments from an incomplete surface\n"
                                    "- expected type args are '%s'\n"
                                    "- actual type args are '%s'\n"
                                    "Fix:\n"
                                    "- provide a concrete type argument for each generic parameter\n"
                                    "- or remove the malformed generic argument from '%s'",
                                    ability_name,
                                    ability_name,
                                    ability_name,
                                    (unsigned long long) (k + 1),
                                    impl_text != NULL ? impl_text : ability_name,
                                    expected_text != NULL ? expected_text : ability_name,
                                    impl_text != NULL ? impl_text : ability_name,
                                    ability_name);
                                free(expected_text);
                                free(impl_text);
                                malformed_impl_args = true;
                                continue;
                            }
                            role_resolve_type_ref(arg, ctx);
                        }
                    }
                    if (!malformed_impl_args) {
                        validate_ability_decl_where_clause_reference(
                            ability_decl,
                            impl->data.impl_ability.ability_ref,
                            impl,
                            ctx,
                            "role",
                            name != NULL ? name : "<role>");
                        validate_ability_require_fields_for_role(
                            node,
                            ability_decl,
                            impl->data.impl_ability.ability_ref,
                            ctx);
                    }
                }
            }

            /* Type-check each method implementation */
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                type_check_func_decl(impl->data.impl_ability.methods[j], ctx);
            }
        } else if (impl->type == AST_OVERRIDE_FUNC) {
            /* Type-check the overridden function */
            if (impl->data.override_func.func_decl != NULL)
                type_check_func_decl(impl->data.override_func.func_decl, ctx);
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}
