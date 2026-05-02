/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent contract-source summary formatting.
 */

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_intent_helpers_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *
intent_involves_type_name(ASTNode *involves);

static const char *
intent_summary_nominal_decl_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.name;
    return NULL;
}

static ASTNode *
intent_summary_subject_action(ASTNode *decl, const char *action_name)
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

static ASTNode *
intent_summary_inheritable_action(ASTNode *intent_decl, ASTNode *step,
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
        ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root,
                                                               type_name);
        ASTNode *action_decl = intent_summary_subject_action(
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
    if (written <= 0)
        return;

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
        matched_action = intent_summary_inheritable_action(
            (ASTNode *)intent_decl, (ASTNode *)step, ctx, &matched_subject_decl);
        if (matched_action != NULL) {
            const char *subject_name = matched_subject_decl != NULL
                ? intent_summary_nominal_decl_name(matched_subject_decl)
                : NULL;
            const char *action_name = matched_action->type == AST_FUNC_DECL
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
        && !step->data.intent_step.inherited_who_from_action
        && !step->data.intent_step.inherited_who_from_intent) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names != NULL
                ? step->data.intent_step.who_names[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s", alias_used > 0 ? ", " : "", alias);
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
        && !step->data.intent_step.inherited_where_from_intent
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
        && !step->data.intent_step.inherited_authorized_by_from_action
        && !step->data.intent_step.derived_authorized_by_from_zone) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.authorized_by_count; i++) {
            const char *alias = step->data.intent_step.authorized_by != NULL
                ? step->data.intent_step.authorized_by[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s", alias_used > 0 ? ", " : "", alias);
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
    if (step->data.intent_step.inherited_who_from_intent) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names != NULL
                ? step->data.intent_step.who_names[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s", alias_used > 0 ? ", " : "", alias);
        }
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused who from intent-level default%s%s",
            has_any ? "\n" : "",
            alias_list[0] != '\0' ? ": " : "",
            alias_list);
        has_any = true;
        alias_list[0] = '\0';
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
    if (step->data.intent_step.inherited_where_from_intent) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- reused zone from intent-level default: %s",
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
    if (step->data.intent_step.derived_authorized_by_from_zone) {
        size_t alias_used = 0;
        for (size_t i = 0; i < step->data.intent_step.authorized_by_count; i++) {
            const char *alias = step->data.intent_step.authorized_by != NULL
                ? step->data.intent_step.authorized_by[i] : NULL;
            if (alias == NULL)
                continue;
            intent_step_summary_append(alias_list, sizeof(alias_list), &alias_used,
                "%s%s", alias_used > 0 ? ", " : "", alias);
        }
        intent_step_summary_append(buffer, buffer_size, &used,
            "%s- derived authorized by from zone authority%s%s (approval owner stays on the zone/resource layer)",
            has_any ? "\n" : "",
            alias_list[0] != '\0' ? ": " : "",
            alias_list);
        has_any = true;
        alias_list[0] = '\0';
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
