/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent helper routines shared by intent declaration validation and
 * top-level semantic orchestration.
 */

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref);
ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program, const char *ability_name);
const char *
intent_involves_type_name(ASTNode *involves);
bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx);
static ASTNode *
intent_step_find_inheritable_action(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx,
                                    ASTNode **subject_decl_out);
static const char *
intent_action_binding_type_name(ASTNode *action_decl, ASTNode *action_subject_decl,
                                SemanticContext *ctx, const char *binding_name);
static const char *
nominal_decl_name(ASTNode *decl);

static Type *
intent_helper_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

static const char *
nominal_decl_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.name;
    return NULL;
}

static bool
intent_step_same_ability_list(ASTNode *step, ASTNode *action_decl)
{
    if (step == NULL || action_decl == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }
    if (step->data.intent_step.required_ability_count
        != action_decl->data.func_decl.required_ability_count) {
        return false;
    }

    for (size_t i = 0; i < step->data.intent_step.required_ability_count; i++) {
        char *step_text = ability_ref_display(
            step->data.intent_step.required_abilities[i]);
        char *action_text = ability_ref_display(
            action_decl->data.func_decl.required_abilities[i]);
        bool same = step_text != NULL && action_text != NULL
            && strcmp(step_text, action_text) == 0;
        free(step_text);
        free(action_text);
        if (!same)
            return false;
    }

    return true;
}

static bool
intent_step_same_authorized_by_list(ASTNode *intent_decl,
                                    ASTNode *step,
                                    ASTNode *action_decl,
                                    ASTNode *action_subject_decl,
                                    SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || action_decl == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }

    if (step->data.intent_step.authorized_by_count
        != action_decl->data.func_decl.authorized_by_count) {
        return false;
    }

    for (size_t i = 0; i < action_decl->data.func_decl.authorized_by_count; i++) {
        const char *binding_name = action_decl->data.func_decl.authorized_by[i];
        const char *binding_type_name = intent_action_binding_type_name(
            action_decl, action_subject_decl, ctx, binding_name);
        const char *mapped_alias = NULL;

        if (binding_name == NULL)
            return false;

        if (strcmp(binding_name, "self") == 0) {
            if (step->data.intent_step.who_count == 1)
                mapped_alias = step->data.intent_step.who_names[0];
        } else if (binding_type_name != NULL) {
            for (size_t j = 0; j < intent_decl->data.intent_decl.involve_count; j++) {
                ASTNode *involves = intent_decl->data.intent_decl.involves[j];
                const char *participant_type_name = intent_involves_type_name(involves);
                if (participant_type_name != NULL
                    && strcmp(participant_type_name, binding_type_name) == 0) {
                    if (mapped_alias != NULL)
                        return false;
                    mapped_alias = involves->data.intent_involves.alias;
                }
            }
        }

        if (mapped_alias == NULL
            || step->data.intent_step.authorized_by[i] == NULL
            || strcmp(step->data.intent_step.authorized_by[i], mapped_alias) != 0) {
            return false;
        }
    }

    return true;
}

static bool
intent_step_same_who_binding(ASTNode *intent_decl,
                             ASTNode *step,
                             ASTNode *action_subject_decl)
{
    const char *subject_name;
    const char *step_alias;
    const char *matched_alias = NULL;

    if (intent_decl == NULL || step == NULL || action_subject_decl == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.who_count != 1) {
        return false;
    }

    subject_name = nominal_decl_name(action_subject_decl);
    step_alias = step->data.intent_step.who_names[0];
    if (subject_name == NULL || step_alias == NULL)
        return false;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        const char *participant_type_name = intent_involves_type_name(involves);
        if (participant_type_name == NULL
            || strcmp(participant_type_name, subject_name) != 0) {
            continue;
        }
        if (matched_alias != NULL)
            return false;
        matched_alias = involves->data.intent_involves.alias;
    }

    return matched_alias != NULL && strcmp(matched_alias, step_alias) == 0;
}

void
intent_step_warn_redundant_action_contract(ASTNode *intent_decl,
                                           ASTNode *step,
                                           SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);
    char redundant[256];
    size_t used = 0;
    bool has_any = false;
    const char *step_name;
    const char *action_name;
    const char *subject_name;

    (void)action_subject_decl;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return;

    redundant[0] = '\0';
    step_name = step->data.intent_step.name != NULL
        ? step->data.intent_step.name : "<step>";
    action_name = action_decl->data.func_decl.name != NULL
        ? action_decl->data.func_decl.name : "<action>";
    subject_name = nominal_decl_name(action_subject_decl);

    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.derived_where_from_using
        && !step->data.intent_step.derived_where_from_transfer
        && action_decl->data.func_decl.within_zone != NULL
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL
        && strcmp(step->data.intent_step.where_type->data.type.name,
                  action_decl->data.func_decl.within_zone) == 0) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- where", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inherited_who_from_action
        && intent_step_same_who_binding(intent_decl, step, action_subject_decl)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- who", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.required_ability_count > 0
        && !step->data.intent_step.inherited_requires_from_action
        && action_decl->data.func_decl.required_ability_count > 0
        && intent_step_same_ability_list(step, action_decl)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- requires", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.causes_effect != NULL
        && !step->data.intent_step.inherited_causes_from_action
        && action_decl->data.func_decl.causes_effect != NULL
        && strcmp(step->data.intent_step.causes_effect,
                  action_decl->data.func_decl.causes_effect) == 0) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- causes", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.authorized_by_count > 0
        && !step->data.intent_step.inherited_authorized_by_from_action
        && action_decl->data.func_decl.authorized_by_count > 0
        && intent_step_same_authorized_by_list(intent_decl, step, action_decl,
                                               action_subject_decl, ctx)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- authorized by", has_any ? "\n" : "");
        has_any = true;
    }

    if (!has_any)
        return;

    semantic_warning(ctx, step,
        "Intent step '%s' restates contract clauses already provided by the matching action contract.\n"
        "Matching action contract: %s.%s\n"
        "Redundant clauses:\n"
        "%s\n"
        "Fix:\n"
        "- remove the duplicated step clauses and let the matching action contract supply them\n"
        "- keep the local clause only when you intentionally override the matching action contract",
        step_name,
        subject_name != NULL ? subject_name : "<subject>",
        action_name,
        redundant);
}

static void
intent_step_summary_append(char *buffer, size_t buffer_size, size_t *used,
                           const char *fmt, ...)
{
    va_list args;
    int written;
    size_t remaining;

    if (buffer == NULL || used == NULL || fmt == NULL || buffer_size == 0
        || *used >= buffer_size) {
        return;
    }

    remaining = buffer_size - *used;
    va_start(args, fmt);
    written = vsnprintf(buffer + *used, remaining, fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    if ((size_t)written >= remaining) {
        *used = buffer_size - 1;
        return;
    }

    *used += (size_t)written;
}

void
intent_step_format_contract_source_summary(const ASTNode *intent_decl,
                                           const ASTNode *step,
                                           SemanticContext *ctx,
                                           char *buffer,
                                           size_t buffer_size)
{
    size_t used = 0;
    bool has_any = false;
    ASTNode *matched_subject_decl = NULL;
    ASTNode *matched_action = NULL;
    char alias_list[256];

    if (buffer == NULL || buffer_size == 0 || step == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }

    buffer[0] = '\0';
    alias_list[0] = '\0';

    if (intent_decl != NULL && ctx != NULL) {
        matched_action = intent_step_find_inheritable_action(
            (ASTNode *)intent_decl, (ASTNode *)step, ctx, &matched_subject_decl);
        if (matched_action != NULL) {
            const char *subject_name =
                matched_subject_decl != NULL
                    ? nominal_decl_name(matched_subject_decl)
                    : NULL;
            const char *action_name =
                matched_action->type == AST_FUNC_DECL
                    ? matched_action->data.func_decl.name
                    : NULL;
            intent_step_summary_append(buffer, buffer_size, &used,
                "%s- matching action contract: %s.%s",
                has_any ? "\n" : "",
                subject_name != NULL ? subject_name : "<subject>",
                action_name != NULL ? action_name : "<action>");
            has_any = true;
        } else if (step->data.intent_step.who_count > 0) {
            intent_step_summary_append(buffer, buffer_size, &used,
                "%s- no unique matching action contract was found for this step name across current who participants",
                has_any ? "\n" : "");
            has_any = true;
        }
    }

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inherited_who_from_action) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names != NULL
                ? step->data.intent_step.who_names[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s",
                alias_used > 0 ? ", " : "",
                alias);
        }
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared who on step%s%s",
            has_any ? "\n" : "",
            alias_list[0] != '\0' ? ": " : "",
            alias_list);
        has_any = true;
        alias_list[0] = '\0';
    }
    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.derived_where_from_using
        && !step->data.intent_step.derived_where_from_transfer
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared zone on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.where_type->data.type.name);
        has_any = true;
    }
    if (step->data.intent_step.required_ability_count > 0
        && !step->data.intent_step.inherited_requires_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared requires on step",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.causes_effect != NULL
        && !step->data.intent_step.inherited_causes_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared causes on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.causes_effect);
        has_any = true;
    }
    if (step->data.intent_step.authorized_by_count > 0
        && !step->data.intent_step.inherited_authorized_by_from_action) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.authorized_by_count; i++) {
            const char *alias = step->data.intent_step.authorized_by != NULL
                ? step->data.intent_step.authorized_by[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s",
                alias_used > 0 ? ", " : "",
                alias);
        }
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared authorized by on step%s%s (step-local approval source)",
            has_any ? "\n" : "",
            alias_list[0] != '\0' ? ": " : "",
            alias_list);
        has_any = true;
        alias_list[0] = '\0';
    }
    if (step->data.intent_step.using_expr != NULL
        && !step->data.intent_step.derived_using_from_transfer
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER
        && step->data.intent_step.using_expr->data.identifier.name != NULL) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- locally declared using on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.using_expr->data.identifier.name);
        has_any = true;
    }
    if (step->data.intent_step.transfer_from_alias != NULL
        && step->data.intent_step.transfer_to_alias != NULL) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- transfer handoff edge on step: %s -> %s (embedding/handoff contract source)",
            has_any ? "\n" : "",
            step->data.intent_step.transfer_from_alias,
            step->data.intent_step.transfer_to_alias);
        has_any = true;
    }

    if (step->data.intent_step.inherited_who_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused who from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    } else if (step->data.intent_step.inherited_where_from_action
               || step->data.intent_step.inherited_requires_from_action
               || step->data.intent_step.inherited_causes_from_action
               || step->data.intent_step.inherited_authorized_by_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inherited_where_from_action) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused zone from matching action contract: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.inherited_requires_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused requires from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inherited_causes_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused causes from matching action contract: %s",
            has_any ? "\n" : "",
            step->data.intent_step.causes_effect != NULL
                ? step->data.intent_step.causes_effect
                : "<effect>");
        has_any = true;
    }
    if (step->data.intent_step.inherited_authorized_by_from_action) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused authorized by from matching action contract (approval source comes from the action header)",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.derived_where_from_transfer) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- derived zone from transfer target handoff: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.derived_where_from_using) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- derived zone from using binding: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.derived_using_from_transfer) {
        const char *using_name =
            (step->data.intent_step.using_expr != NULL
             && step->data.intent_step.using_expr->type == AST_IDENTIFIER
             && step->data.intent_step.using_expr->data.identifier.name != NULL)
                ? step->data.intent_step.using_expr->data.identifier.name
                : "<binding>";
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- derived using from transfer target: %s",
            has_any ? "\n" : "", using_name);
        has_any = true;
    }
    if ((step->data.intent_step.derived_where_from_transfer
         || step->data.intent_step.derived_using_from_transfer)
        && step->data.intent_step.transfer_from_alias != NULL
        && step->data.intent_step.transfer_to_alias != NULL) {
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- derived from transfer edge: %s -> %s",
            has_any ? "\n" : "",
            step->data.intent_step.transfer_from_alias,
            step->data.intent_step.transfer_to_alias);
    }
}

const char *
intent_step_single_who_alias(const ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.who_count != 1
        || step->data.intent_step.who_names == NULL) {
        return NULL;
    }
    return step->data.intent_step.who_names[0];
}

bool
intent_condition_is_bool(ASTNode *expr, SemanticContext *ctx, const char *label)
{
    Type *ty;
    if (expr == NULL)
        return true;
    ty = type_check_expression(expr, ctx);
    if (ty != NULL && !type_equals(ty, TYPE_BOOL)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_INTENT_NON_BOOL_CLAUSE, PGY_FIX_CONVERT_TO_BOOL,
            expr,
            "Intent %s expects a Bool value, got '%s'. Use a Bool predicate or cast/normalize the expression to Bool before this clause.",
            label != NULL ? label : "condition",
            ty->name != NULL ? ty->name : "<type>");
        return false;
    }
    return true;
}

const char *
intent_involves_type_name(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

bool
intent_involves_is_subject_host(ASTNode *program, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name(involves);
    ASTNode *decl = NULL;

    if (type_name == NULL)
        return false;
    decl = find_subject_host_decl_by_name(program, type_name);
    return decl_is_subject_host(decl);
}

bool
subject_decl_has_action_named(ASTNode *decl, const char *action_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || action_name == NULL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return false;
    }

    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return true;
        }
    }
    return false;
}

static ASTNode *
subject_decl_find_action_named(ASTNode *decl, const char *action_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || action_name == NULL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }

    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return method;
        }
    }
    return NULL;
}

static bool
intent_semantic_append_name(char ***items, size_t *count, const char *name)
{
    char **grown;

    if (items == NULL || count == NULL || name == NULL)
        return false;
    grown = realloc(*items, (*count + 1) * sizeof(char *));
    if (grown == NULL)
        return false;
    grown[*count] = pergyra_strdup(name);
    *items = grown;
    (*count)++;
    return true;
}

static const char *
intent_action_binding_type_name(ASTNode *action_decl, ASTNode *subject_decl,
                                SemanticContext *ctx, const char *binding_name)
{
    if (binding_name == NULL || action_decl == NULL || ctx == NULL)
        return NULL;
    if (strcmp(binding_name, "self") == 0) {
        return (subject_decl != NULL && subject_decl->type == AST_CLASS_DECL)
            ? subject_decl->data.class_decl.name
            : NULL;
    }

    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *param = action_decl->data.func_decl.params[i];
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0
            || param->type == NULL) {
            continue;
        }
        param_type = intent_helper_resolve_type_ref(param->type, ctx);
        return (param_type != NULL) ? param_type->name : NULL;
    }

    return NULL;
}

static ASTNode *
intent_step_find_inheritable_action(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx,
                                    ASTNode **subject_decl_out)
{
    ASTNode *matched_action = NULL;
    ASTNode *matched_subject_decl = NULL;

    if (subject_decl_out != NULL)
        *subject_decl_out = NULL;
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *type_name = intent_involves_type_name(involves);
        ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root, type_name);
        ASTNode *action_decl = subject_decl_find_action_named(
            subject_decl, step->data.intent_step.name);

        if (action_decl == NULL)
            continue;
        if (matched_action != NULL && matched_action != action_decl)
            return NULL;
        matched_action = action_decl;
        matched_subject_decl = subject_decl;
    }

    if (subject_decl_out != NULL)
        *subject_decl_out = matched_subject_decl;
    return matched_action;
}

void
intent_step_derive_who_from_action(ASTNode *intent_decl, ASTNode *step,
                                   SemanticContext *ctx)
{
    const char *matched_alias = NULL;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.who_count != 0) {
        return;
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        ASTNode *subject_decl;
        ASTNode *action_decl;

        if (!intent_involves_is_subject_host(ctx->program_root, involves))
            continue;
        subject_decl = find_subject_host_decl_by_name(
            ctx->program_root, intent_involves_type_name(involves));
        action_decl = subject_decl_find_action_named(
            subject_decl, step->data.intent_step.name);
        if (action_decl == NULL)
            continue;

        if (matched_alias != NULL)
            return;
        matched_alias = involves->data.intent_involves.alias;
    }

    if (matched_alias != NULL
        && intent_semantic_append_name(&step->data.intent_step.who_names,
               &step->data.intent_step.who_count, matched_alias)) {
        step->data.intent_step.inherited_who_from_action = true;
    }
}

void
intent_step_inherit_action_contract(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);

    if (action_decl == NULL)
        return;

    if (step->data.intent_step.where_type == NULL
        && action_decl->data.func_decl.within_zone != NULL) {
        step->data.intent_step.where_type =
            ast_create_type(action_decl->data.func_decl.within_zone);
        step->data.intent_step.inherited_where_from_action = true;
    }

    if (step->data.intent_step.required_ability_count == 0
        && action_decl->data.func_decl.required_ability_count > 0) {
        for (size_t i = 0; i < action_decl->data.func_decl.required_ability_count; i++) {
            ASTNode *ability_ref = action_decl->data.func_decl.required_abilities[i];
            ASTNode *ability_copy = ast_clone(ability_ref);
            size_t next = step->data.intent_step.required_ability_count + 1;
            step->data.intent_step.required_abilities = realloc(
                step->data.intent_step.required_abilities,
                next * sizeof(ASTNode *));
            step->data.intent_step.required_abilities[next - 1] = ability_copy;
            step->data.intent_step.required_ability_count = next;
        }
        step->data.intent_step.inherited_requires_from_action = true;
    }

    if (step->data.intent_step.causes_effect == NULL
        && action_decl->data.func_decl.causes_effect != NULL) {
        step->data.intent_step.causes_effect =
            pergyra_strdup(action_decl->data.func_decl.causes_effect);
        step->data.intent_step.inherited_causes_from_action = true;
    }

    if (step->data.intent_step.authorized_by_count == 0
        && action_decl->data.func_decl.authorized_by_count > 0) {
        bool mapped_all = true;

        for (size_t i = 0; i < action_decl->data.func_decl.authorized_by_count; i++) {
            const char *binding_name = action_decl->data.func_decl.authorized_by[i];
            const char *binding_type_name = intent_action_binding_type_name(
                action_decl, action_subject_decl, ctx, binding_name);
            const char *mapped_alias = NULL;

            if (strcmp(binding_name, "self") == 0) {
                if (step->data.intent_step.who_count == 1) {
                    mapped_alias = step->data.intent_step.who_names[0];
                }
            } else if (binding_type_name != NULL) {
                for (size_t j = 0; j < intent_decl->data.intent_decl.involve_count; j++) {
                    ASTNode *involves = intent_decl->data.intent_decl.involves[j];
                    const char *participant_type_name = intent_involves_type_name(involves);
                    if (participant_type_name != NULL
                        && strcmp(participant_type_name, binding_type_name) == 0) {
                        if (mapped_alias != NULL) {
                            mapped_alias = NULL;
                            break;
                        }
                        mapped_alias = involves->data.intent_involves.alias;
                    }
                }
            }

            if (mapped_alias == NULL) {
                mapped_all = false;
                break;
            }
            if (!intent_semantic_append_name(&step->data.intent_step.authorized_by,
                    &step->data.intent_step.authorized_by_count, mapped_alias)) {
                mapped_all = false;
                break;
            }
        }

        if (mapped_all) {
            step->data.intent_step.inherited_authorized_by_from_action = true;
        } else {
            for (size_t i = 0; i < step->data.intent_step.authorized_by_count; i++)
                free(step->data.intent_step.authorized_by[i]);
            free(step->data.intent_step.authorized_by);
            step->data.intent_step.authorized_by = NULL;
            step->data.intent_step.authorized_by_count = 0;
        }
    }
}

const char *
projection_refresh_source_field_name(ASTNode *refresh,
                                     const char *target_field_name)
{
    if (target_field_name == NULL)
        return NULL;
    if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
        for (size_t i = 0; i < refresh->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                refresh->data.zone_refresh.mapped_target_fields[i];
            const char *mapped_source =
                refresh->data.zone_refresh.mapped_source_fields[i];
            if (mapped_target != NULL && mapped_source != NULL
                && strcmp(mapped_target, target_field_name) == 0) {
                return mapped_source;
            }
        }
    }
    return target_field_name;
}

bool
projection_target_decl_has_field(ASTNode *target_decl, const char *field_name)
{
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return false;
    }
    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *field = target_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return true;
        }
    }
    return false;
}

ASTNode *
find_zone_authority(ASTNode *zone, const char *slot_name);

ASTNode *
resolve_zone_subject_slot_for_participant(ASTNode *zone,
                                          SemanticContext *ctx,
                                          const char *participant_alias,
                                          const char *participant_type_name,
                                          bool *ambiguous_out);
