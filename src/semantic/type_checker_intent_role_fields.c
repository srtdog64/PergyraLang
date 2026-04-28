#include "type_checker_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static Type *
intent_role_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

static Type *
intent_role_resolve_involves_type(ASTNode *involves, SemanticContext *ctx)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
        return NULL;
    return intent_role_resolve_type_ref(
        involves->data.intent_involves.subject_type, ctx);
}

static Type *
intent_role_resolve_field_type(ClassField *field, SemanticContext *ctx)
{
    if (field == NULL)
        return NULL;
    return intent_role_resolve_type_ref(field->type, ctx);
}

static ClassField *
find_nominal_field_by_name(ASTNode *decl, const char *field_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || field_name == NULL)
        return NULL;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static ClassField *
find_subject_surface_field_by_name(ASTNode *program_root,
                                   ASTNode *subject_decl,
                                   const char *field_name,
                                   const char **container_field_name_out)
{
    if (container_field_name_out != NULL)
        *container_field_name_out = NULL;

    if (program_root == NULL || subject_decl == NULL
        || subject_decl->type != AST_CLASS_DECL || field_name == NULL) {
        return NULL;
    }

    {
        ClassField *direct = find_nominal_field_by_name(subject_decl, field_name);
        if (direct != NULL)
            return direct;
    }

    for (size_t i = 0; i < subject_decl->data.class_decl.field_count; i++) {
        ClassField *field = subject_decl->data.class_decl.fields[i];
        ASTNode *vessel_decl;
        ClassField *nested;

        if (field == NULL || !field->is_vessel_field || field->type == NULL
            || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_type_decl_by_name(program_root, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
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
    ASTNode **effective_args = NULL;
    size_t effective_count = 0;
    char *ability_text = NULL;

    if (out_type != NULL)
        *out_type = TYPE_UNKNOWN;
    if (ctx == NULL || out_type == NULL || type_node == NULL) {
        return;
    }
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ability_ref == NULL || ability_ref->type != AST_TYPE) {
        *out_type = intent_role_resolve_type_ref(type_node, ctx);
        return;
    }

    decl_params = ability_decl->data.ability_decl.generic_params;
    if (decl_params == NULL || decl_params->count == 0) {
        *out_type = intent_role_resolve_type_ref(type_node, ctx);
        return;
    }

    ability_text = ability_ref_display(ability_ref);

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        ability_ref->data.type.generic_args,
        site != NULL ? site : type_node,
        ctx,
        "ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name : "<ability>",
        &effective_count);
    if (effective_args == NULL) {
        free(ability_text);
        *out_type = TYPE_UNKNOWN;
        return;
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < decl_params->count && i < effective_count; i++) {
        GenericParam *param = decl_params->params[i];
        Type *arg_type = NULL;
        Symbol *s = NULL;

        if (param == NULL || param->name == NULL)
            continue;
        if (effective_args[i] == NULL) {
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
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                param->name,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                param->name,
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                param->name,
                param->name);
            continue;
        }

        arg_type = intent_role_resolve_type_ref(effective_args[i], ctx);
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
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                param->name,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                param->name,
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                param->name,
                param->name);
            continue;
        }
        s = symbol_create_variable(param->name,
                                   arg_type != NULL ? arg_type : TYPE_UNKNOWN,
                                   site != NULL ? site->line : ability_decl->line,
                                   site != NULL ? site->column : ability_decl->column);
        if (s == NULL)
            continue;
        s->kind = SYMBOL_TYPE_PARAM;
        scope_declare(ctx->scope, s);
    }

    *out_type = intent_role_resolve_type_ref(type_node, ctx);
    scope_exit(&ctx->scope);
    free(ability_text);
    free(effective_args);
}

void
validate_ability_require_fields_for_role(ASTNode *role_decl,
                                         ASTNode *ability_decl,
                                         ASTNode *ability_ref,
                                         SemanticContext *ctx)
{
    ASTNode *bound_decl;
    char *ability_text = NULL;

    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL
        || ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ctx == NULL
        || role_decl->data.role_decl.for_type == NULL
        || role_decl->data.role_decl.for_type->type != AST_TYPE
        || role_decl->data.role_decl.for_type->data.type.name == NULL) {
        return;
    }

    bound_decl = find_type_decl_by_name(
        ctx->program_root, role_decl->data.role_decl.for_type->data.type.name);
    if (bound_decl == NULL || !decl_is_subject_host(bound_decl))
        return;

    ability_text = ability_ref_display(ability_ref);

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        ClassField *field;
        const char *container_field_name = NULL;
        Type *required_type;
        Type *field_type;

        if (req == NULL || req->type != AST_REQUIRE_FIELD
            || req->data.require_field.name == NULL
            || req->data.require_field.type == NULL) {
            continue;
        }

        field = find_subject_surface_field_by_name(
            ctx->program_root, bound_decl, req->data.require_field.name,
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
                role_decl->data.role_decl.name != NULL ? role_decl->data.role_decl.name : "<role>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                req->data.require_field.name,
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                req->data.require_field.name,
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                req->data.require_field.name,
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"));
            continue;
        }

        resolve_ability_require_field_type_scope(ability_decl,
                                                 ability_ref,
                                                 req->data.require_field.type,
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
                role_decl->data.role_decl.name != NULL ? role_decl->data.role_decl.name : "<role>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req->data.require_field.name,
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req->data.require_field.name,
                field_type->name != NULL ? field_type->name : "<type>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"),
                required_type->name != NULL ? required_type->name : "<type>",
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req->data.require_field.name,
                required_type->name != NULL ? required_type->name : "<type>",
                ability_text != NULL ? ability_text
                                     : (ability_decl->data.ability_decl.name != NULL
                                            ? ability_decl->data.ability_decl.name
                                            : "<ability>"));
        }
    }

    free(ability_text);
}

ASTNode *
find_intent_involves_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
find_unique_intent_involves_by_type_name(ASTNode *intent,
                                         const char *type_name,
                                         const char **alias_out)
{
    ASTNode *matched = NULL;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        const char *participant_type_name = intent_involves_type_name(involves);

        if (participant_type_name == NULL
            || strcmp(participant_type_name, type_name) != 0) {
            continue;
        }

        if (matched != NULL)
            return NULL;
        matched = involves;
    }

    if (matched != NULL && alias_out != NULL)
        *alias_out = matched->data.intent_involves.alias;
    return matched;
}

ASTNode *
intent_step_resolve_transfer_target_involves(ASTNode *intent_decl,
                                             ASTNode *step,
                                             const char **resolved_alias_out)
{
    const char *to_name;
    ASTNode *to_involves;
    const char *resolved_alias = NULL;

    if (resolved_alias_out != NULL)
        *resolved_alias_out = NULL;
    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return NULL;

    to_name = step->data.intent_step.transfer_to_alias;
    if (to_name == NULL)
        return NULL;

    to_involves = find_intent_involves_local(intent_decl, to_name);
    if (to_involves == NULL) {
        to_involves = find_unique_intent_involves_by_type_name(
            intent_decl, to_name, &resolved_alias);
        if (to_involves != NULL && resolved_alias != NULL) {
            free(step->data.intent_step.transfer_to_alias);
            step->data.intent_step.transfer_to_alias = pergyra_strdup(resolved_alias);
        }
    } else {
        resolved_alias = to_name;
    }

    if (resolved_alias_out != NULL)
        *resolved_alias_out = resolved_alias;
    return to_involves;
}

void
intent_step_derive_transfer_context(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx)
{
    const char *to_alias = NULL;
    ASTNode *to_involves;
    Type *to_type;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP)
        return;

    to_involves = intent_step_resolve_transfer_target_involves(
        intent_decl, step, &to_alias);
    if (to_involves == NULL || to_involves->type != AST_INTENT_INVOLVES)
        return;

    to_type = intent_role_resolve_involves_type(to_involves, ctx);

    if (step->data.intent_step.using_expr == NULL) {
        step->data.intent_step.using_expr = ast_create_identifier(to_alias);
        step->data.intent_step.derived_using_from_transfer = true;
    }

    if (step->data.intent_step.where_type == NULL
        && to_type != NULL
        && to_type->name != NULL) {
        step->data.intent_step.where_type = ast_create_type(to_type->name);
        step->data.intent_step.derived_where_from_transfer = true;
    }
}

void
intent_step_derive_zone_binding_context(ASTNode *intent_decl, ASTNode *step,
                                        SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }

    if (step->data.intent_step.where_type == NULL
        && step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        ASTNode *using_involves = find_intent_involves_local(intent_decl,
            step->data.intent_step.using_expr->data.identifier.name);
        Type *using_type = intent_role_resolve_involves_type(using_involves, ctx);
        ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root,
            AST_ZONE_DECL, using_type != NULL ? using_type->name : NULL);
        if (zone_decl != NULL && using_type != NULL && using_type->name != NULL) {
            step->data.intent_step.where_type = ast_create_type(using_type->name);
            step->data.intent_step.derived_where_from_using = true;
        }
    }

    if (step->data.intent_step.using_expr == NULL
        && step->data.intent_step.where_type != NULL) {
        Type *zone_type = intent_role_resolve_type_ref(step->data.intent_step.where_type, ctx);
        const char *matched_alias = NULL;

        if (zone_type == NULL || zone_type->name == NULL)
            return;

        for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
            ASTNode *involves = intent_decl->data.intent_decl.involves[i];
            Type *participant_type;

            if (involves == NULL || involves->type != AST_INTENT_INVOLVES
                || involves->data.intent_involves.subject_type == NULL) {
                continue;
            }

            participant_type = intent_role_resolve_involves_type(involves, ctx);
            if (participant_type == NULL || participant_type->name == NULL
                || strcmp(participant_type->name, zone_type->name) != 0) {
                continue;
            }

            if (matched_alias != NULL)
                return;
            matched_alias = involves->data.intent_involves.alias;
        }

        if (matched_alias != NULL)
            step->data.intent_step.using_expr = ast_create_identifier(matched_alias);
    }
}
