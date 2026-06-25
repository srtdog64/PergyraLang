#include "type_checker_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

static Type *
intent_role_resolve_field_type(ClassField *field, SemanticContext *ctx)
{
    if (field == NULL)
        return NULL;
    return intent_normalize_type(intent_resolve_type_ref(field->type, ctx));
}

static ClassField *
find_nominal_field_by_name(ASTNode *decl, const char *field_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || field_name == NULL)
        return NULL;

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static ClassField *
find_subject_surface_field_by_name(SemanticContext *ctx,
                                   ASTNode *subject_decl,
                                   const char *field_name,
                                   const char **container_field_name_out)
{
    if (container_field_name_out != NULL)
        *container_field_name_out = NULL;

    if (ctx == NULL || subject_decl == NULL
        || subject_decl->type != AST_CLASS_DECL || field_name == NULL) {
        return NULL;
    }

    {
        ClassField *direct = find_nominal_field_by_name(subject_decl, field_name);
        if (direct != NULL)
            return direct;
    }

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(subject_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        ASTNode *vessel_decl;
        ClassField *nested;
        Type *field_type;

        if (field == NULL || !field->is_vessel_field || field->type == NULL
            || field->name == NULL) {
            continue;
        }

        field_type = intent_role_resolve_field_type(field, ctx);
        vessel_decl = semantic_host_decl_for_type(ctx, field_type);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested = find_nominal_field_by_name(vessel_decl, field_name);
        if (nested != NULL) {
            if (container_field_name_out != NULL)
                *container_field_name_out = field->name;
            return nested;
        }
    }

    return NULL;
}

static void
resolve_ability_require_field_type_scope(ASTNode *ability_decl,
                                         ASTNode *ability_ref,
                                         ASTNode *type_node,
                                         ASTNode *site,
                                         SemanticContext *ctx,
                                         Type **out_type)
{
    GenericParams *decl_params;
    Type **effective_types = NULL;
    size_t effective_count = 0;
    char *ability_text = NULL;
    const char *ability_name;
    const char *ability_label;

    if (out_type != NULL)
        *out_type = TYPE_UNKNOWN;
    if (ctx == NULL || out_type == NULL || type_node == NULL)
        return;
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ability_ref == NULL || ability_ref->type != AST_TYPE) {
        *out_type = intent_normalize_type(intent_resolve_type_ref(type_node, ctx));
        return;
    }

    decl_params = ast_ability_generic_params(ability_decl);
    size_t decl_count = ast_generic_param_count(decl_params);
    if (decl_count == 0) {
        *out_type = intent_normalize_type(intent_resolve_type_ref(type_node, ctx));
        return;
    }

    ability_name = ast_ability_name(ability_decl);
    ability_label = ability_name != NULL ? ability_name : "<ability>";
    ability_text = ability_ref_display(ability_ref);

    effective_types = collect_effective_generic_arg_types(
        decl_params,
        ast_type_generic_args(ability_ref),
        site != NULL ? site : type_node,
        ctx,
        "ability",
        ability_label,
        &effective_count);
    if (effective_types == NULL) {
        free(ability_text);
        *out_type = TYPE_UNKNOWN;
        return;
    }

    if (ast_type_name(type_node) != NULL
        && ast_type_generic_args(type_node) == NULL) {
        int param_index = find_generic_param_index(
            decl_params, ast_type_name(type_node));
        if (param_index >= 0 && (size_t)param_index < effective_count
            && effective_types[param_index] != NULL) {
            *out_type = effective_types[param_index];
            free(ability_text);
            free(effective_types);
            return;
        }
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < decl_count && i < effective_count; i++) {
        GenericParam *param = ast_generic_param_at(decl_params, i);
        const char *param_name = ast_generic_param_name(param);
        Type *arg_type = NULL;
        Symbol *s = NULL;

        if (param_name == NULL)
            continue;
        if (i >= effective_count || effective_types[i] == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site != NULL ? site : type_node,
                "Ability field contract '%s' could not materialize generic argument for '%s'.\n"
                "Reason:\n"
                "- generic subject is ability '%s'\n"
                "- consumer path is require-field resolution from '%s'\n"
                "- require-field type resolution depends on effective type arguments from '%s'\n"
                "- generic parameter '%s' has no effective argument on this consumer path\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- provide/supply a type argument for '%s'\n"
                "- or fix the ability generic/default parameter list so '%s' is materialized",
                ability_label,
                param_name,
                ability_label,
                ability_text != NULL ? ability_text
                                     : ability_label,
                ability_text != NULL ? ability_text
                                     : ability_label,
                param_name,
                ability_text != NULL ? ability_text
                                     : ability_label,
                param_name,
                param_name);
            continue;
        }

        arg_type = effective_types[i];
        if (arg_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site != NULL ? site : type_node,
                "Ability field contract '%s' could not resolve generic argument for '%s'.\n"
                "Reason:\n"
                "- generic subject is ability '%s'\n"
                "- consumer path is require-field resolution from '%s'\n"
                "- require-field type resolution depends on effective type arguments from '%s'\n"
                "- generic parameter '%s' did not resolve to a concrete type on this consumer path\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- provide a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so '%s' resolves",
                ability_label,
                param_name,
                ability_label,
                ability_text != NULL ? ability_text
                                     : ability_label,
                ability_text != NULL ? ability_text
                                     : ability_label,
                param_name,
                ability_text != NULL ? ability_text
                                     : ability_label,
                param_name,
                param_name);
            continue;
        }
        s = symbol_create_variable(param_name,
                                   arg_type != NULL ? arg_type : TYPE_UNKNOWN,
                                   site != NULL ? site->line : ability_decl->line,
                                   site != NULL ? site->column : ability_decl->column);
        if (s == NULL)
            continue;
        s->kind = SYMBOL_TYPE_PARAM;
        scope_declare(ctx->scope, s);
    }

    *out_type = intent_normalize_type(intent_resolve_type_ref(type_node, ctx));
    scope_exit(&ctx->scope);
    free(ability_text);
    free(effective_types);
}

void
validate_ability_require_fields_for_role(ASTNode *role_decl,
                                         ASTNode *ability_decl,
                                         ASTNode *ability_ref,
                                         SemanticContext *ctx)
{
    ASTNode *bound_decl;
    const char *bound_type_name;
    char *ability_text = NULL;
    const char *ability_name;
    const char *ability_label;
    const char *role_name;
    const char *role_label;
    ASTNode *bound_type_node;
    Type *bound_type;

    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL
        || ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ctx == NULL) {
        return;
    }

    ability_name = ast_ability_name(ability_decl);
    ability_label = ability_name != NULL ? ability_name : "<ability>";
    role_name = ast_role_name(role_decl);
    role_label = role_name != NULL ? role_name : "<role>";
    bound_type_name = semantic_role_for_type_name(role_decl);
    if (bound_type_name == NULL)
        return;

    bound_type_node = semantic_role_for_type_node(role_decl);
    bound_type = semantic_host_resolve_type_ref(bound_type_node, ctx);
    bound_decl = semantic_host_decl_for_type(ctx, bound_type);
    if (bound_decl == NULL || !decl_is_subject_host(bound_decl))
        return;

    ability_text = ability_ref_display(ability_ref);

    for (size_t i = 0; i < ast_ability_require_field_count(ability_decl); i++) {
        ASTNode *req = ast_ability_require_field(ability_decl, i);
        const char *req_name = ast_require_field_name(req);
        ASTNode *req_type = ast_require_field_type(req);
        ClassField *field;
        const char *container_field_name = NULL;
        const char *bound_name = ast_class_name(bound_decl);
        Type *required_type;
        Type *field_type;

        if (req_name == NULL || req_type == NULL) {
            continue;
        }

        field = find_subject_surface_field_by_name(ctx, bound_decl, req_name,
                                                   &container_field_name);
        if (field == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, role_decl,
                "Role '%s' cannot implement ability '%s' because subject '%s' is missing required field '%s'.\n"
                "Reason:\n"
                "- impl contract is '%s'\n"
                "- ability '%s' requires subject field '%s'\n"
                "- subject '%s' has no matching surface field for that requirement\n"
                "Fix:\n"
                "- add field '%s' to subject '%s'\n"
                "- or relax/remove the require-field from ability '%s'",
                role_label,
                ability_text != NULL ? ability_text : ability_label,
                bound_name != NULL ? bound_name : "<subject>",
                req_name,
                ability_text != NULL ? ability_text : ability_label,
                ability_text != NULL ? ability_text : ability_label,
                req_name,
                bound_name != NULL ? bound_name : "<subject>",
                req_name,
                bound_name != NULL ? bound_name : "<subject>",
                ability_text != NULL ? ability_text : ability_label);
            continue;
        }

        resolve_ability_require_field_type_scope(ability_decl,
                                                 ability_ref,
                                                 req_type,
                                                 role_decl,
                                                 ctx,
                                                 &required_type);
        field_type = intent_role_resolve_field_type(field, ctx);
        if (required_type != NULL && field_type != NULL
            && !type_is_assignable(field_type, required_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, role_decl,
                "Role '%s' cannot implement ability '%s' because subject field '%s.%s%s%s' has incompatible type.\n"
                "Reason:\n"
                "- impl contract is '%s'\n"
                "- subject field '%s.%s%s%s' resolves to '%s'\n"
                "- ability '%s' requires '%s' for that field\n"
                "Fix:\n"
                "- change field '%s.%s%s%s' to type '%s'\n"
                "- or relax/remove the require-field from ability '%s'",
                role_label,
                ability_text != NULL ? ability_text : ability_label,
                bound_name != NULL ? bound_name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req_name,
                ability_text != NULL ? ability_text : ability_label,
                bound_name != NULL ? bound_name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req_name,
                field_type->name != NULL ? field_type->name : "<type>",
                ability_text != NULL ? ability_text : ability_label,
                required_type->name != NULL ? required_type->name : "<type>",
                bound_name != NULL ? bound_name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req_name,
                required_type->name != NULL ? required_type->name : "<type>",
                ability_text != NULL ? ability_text : ability_label);
        }
    }

    free(ability_text);
}
