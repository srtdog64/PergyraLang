/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent action-contract inheritance and contract-source diagnostics.
 */

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_ability_ref_internal.h"

#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *
intent_action_nominal_decl_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL)
        return ast_class_name(decl);
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

static const char *
intent_action_binding_type_name(ASTNode *action_decl, ASTNode *subject_decl,
                                SemanticContext *ctx, const char *binding_name)
{
    if (binding_name == NULL || action_decl == NULL || ctx == NULL)
        return NULL;
    if (strcmp(binding_name, "self") == 0) {
        return (subject_decl != NULL && subject_decl->type == AST_CLASS_DECL)
            ? ast_class_name(subject_decl)
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
        param_type = intent_resolve_type_ref(param->type, ctx);
        return (param_type != NULL) ? param_type->name : NULL;
    }

    return NULL;
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

    subject_name = intent_action_nominal_decl_name(action_subject_decl);
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
intent_step_warn_redundant_action_contract(ASTNode *intent_decl,
                                           ASTNode *step,
                                           SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);
    char redundant[256];
    bool has_any = false;
    const char *step_name;
    const char *action_name;
    const char *subject_name;

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
    subject_name = intent_action_nominal_decl_name(action_subject_decl);

    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.derived_where_from_using
        && !step->data.intent_step.derived_where_from_transfer
        && action_decl->data.func_decl.within_zone != NULL
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL
        && strcmp(step->data.intent_step.where_type->data.type.name,
                  action_decl->data.func_decl.within_zone) == 0) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- where", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inherited_who_from_action
        && intent_step_same_who_binding(intent_decl, step, action_subject_decl)) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- who", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.required_ability_count > 0
        && !step->data.intent_step.inherited_requires_from_action
        && action_decl->data.func_decl.required_ability_count > 0
        && intent_step_same_ability_list(step, action_decl)) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- requires", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.causes_effect != NULL
        && !step->data.intent_step.inherited_causes_from_action
        && action_decl->data.func_decl.causes_effect != NULL
        && strcmp(step->data.intent_step.causes_effect,
                  action_decl->data.func_decl.causes_effect) == 0) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- causes", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.authorized_by_count > 0
        && !step->data.intent_step.inherited_authorized_by_from_action
        && action_decl->data.func_decl.authorized_by_count > 0
        && intent_step_same_authorized_by_list(intent_decl, step, action_decl,
                                               action_subject_decl, ctx)) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
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
               &step->data.intent_step.who_count,
               &step->data.intent_step.who_capacity,
               matched_alias)) {
        step->data.intent_step.inherited_who_from_action = true;
    }
}

void
intent_step_derive_who_from_single_participant(ASTNode *intent_decl,
                                               ASTNode *step,
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
        const char *alias;

        if (!intent_involves_is_subject_host(ctx->program_root, involves))
            continue;
        alias = involves->data.intent_involves.alias;
        if (alias == NULL)
            continue;
        if (matched_alias != NULL)
            return;
        matched_alias = alias;
    }

    if (matched_alias != NULL
        && intent_semantic_append_name(&step->data.intent_step.who_names,
               &step->data.intent_step.who_count,
               &step->data.intent_step.who_capacity,
               matched_alias)) {
        step->data.intent_step.derived_who_from_single_participant = true;
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
        bool copied_all = true;
        for (size_t i = 0; i < action_decl->data.func_decl.required_ability_count; i++) {
            ASTNode *ability_ref = action_decl->data.func_decl.required_abilities[i];
            if (!intent_step_append_required_ability_clone(step, ability_ref)) {
                copied_all = false;
                break;
            }
        }
        step->data.intent_step.inherited_requires_from_action = copied_all;
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
                    &step->data.intent_step.authorized_by_count,
                    &step->data.intent_step.authorized_by_capacity,
                    mapped_alias)) {
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
            step->data.intent_step.authorized_by_capacity = 0;
        }
    }
}
