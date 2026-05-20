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
    ASTNode **step_abilities;
    size_t step_ability_count;

    if (step == NULL || action_decl == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }
    step_abilities = ast_intent_step_required_abilities(
        step, &step_ability_count);
    if (step_ability_count != ast_func_required_ability_count(action_decl)) {
        return false;
    }

    for (size_t i = 0; i < step_ability_count; i++) {
        char *step_text = ability_ref_display(step_abilities[i]);
        char *action_text = ability_ref_display(
            ast_func_required_ability(action_decl, i));
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

    for (size_t i = 0; i < ast_func_param_count(action_decl); i++) {
        FuncParam *param = ast_func_param(action_decl, i);
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
intent_step_has_local_authorized_by(ASTNode *step)
{
    return step != NULL
        && step->type == AST_INTENT_STEP
        && ast_intent_step_authorized_by_count(step) > 0
        && !ast_intent_step_inherited_authorized_by_from_action(step)
        && !ast_intent_step_derived_authorized_by_from_zone(step);
}

static bool
intent_step_same_authorized_by_list(ASTNode *intent_decl,
                                    ASTNode *step,
                                    ASTNode *action_decl,
                                    ASTNode *action_subject_decl,
                                    SemanticContext *ctx)
{
    char **step_authorized;
    size_t step_authorized_count;
    char **step_who;
    size_t step_who_count;
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent_decl == NULL || step == NULL || action_decl == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }

    step_authorized = ast_intent_step_authorized_by(
        step, &step_authorized_count);
    step_who = ast_intent_step_who_names(step, &step_who_count);
    involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);
    if (step_authorized_count != ast_func_authorized_by_count(action_decl)) {
        return false;
    }

    for (size_t i = 0; i < ast_func_authorized_by_count(action_decl); i++) {
        const char *binding_name = ast_func_authorized_by(action_decl, i);
        const char *binding_type_name = intent_action_binding_type_name(
            action_decl, action_subject_decl, ctx, binding_name);
        const char *mapped_alias = NULL;

        if (binding_name == NULL)
            return false;

        if (strcmp(binding_name, "self") == 0) {
            if (step_who_count == 1)
                mapped_alias = step_who[0];
        } else if (binding_type_name != NULL) {
            for (size_t j = 0; j < involve_count; j++) {
                ASTNode *involves = involves_nodes[j];
                const char *participant_type_name = intent_involves_type_name(involves);
                if (participant_type_name != NULL
                    && strcmp(participant_type_name, binding_type_name) == 0) {
                    if (mapped_alias != NULL)
                        return false;
                    mapped_alias = ast_intent_involves_alias(involves);
                }
            }
        }

        if (mapped_alias == NULL
            || step_authorized[i] == NULL
            || strcmp(step_authorized[i], mapped_alias) != 0) {
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
    char **step_who;
    size_t step_who_count;
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent_decl == NULL || step == NULL || action_subject_decl == NULL
        || step->type != AST_INTENT_STEP) {
        return false;
    }
    step_who = ast_intent_step_who_names(step, &step_who_count);
    involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);
    if (step_who_count != 1)
        return false;

    subject_name = intent_action_nominal_decl_name(action_subject_decl);
    step_alias = step_who[0];
    if (subject_name == NULL || step_alias == NULL)
        return false;

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        const char *participant_type_name = intent_involves_type_name(involves);
        if (participant_type_name == NULL
            || strcmp(participant_type_name, subject_name) != 0) {
            continue;
        }
        if (matched_alias != NULL)
            return false;
        matched_alias = ast_intent_involves_alias(involves);
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
    char **step_who;
    size_t step_who_count;
    const char *step_name;

    if (subject_decl_out != NULL)
        *subject_decl_out = NULL;
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }
    step_who = ast_intent_step_who_names(step, &step_who_count);
    step_name = ast_intent_step_name(step);

    for (size_t i = 0; i < step_who_count; i++) {
        const char *alias = step_who[i];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *type_name = intent_involves_type_name(involves);
        ASTNode *subject_decl =
            find_subject_host_decl_by_name(ctx->program_root, type_name);
        ASTNode *action_decl = subject_decl_find_action_named(
            subject_decl, step_name);

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
    ASTNode *step_where;
    const char *step_causes;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return;

    redundant[0] = '\0';
    step_name = ast_intent_step_name(step) != NULL
        ? ast_intent_step_name(step) : "<step>";
    action_name = ast_declaration_name(action_decl) != NULL
        ? ast_declaration_name(action_decl) : "<action>";
    subject_name = intent_action_nominal_decl_name(action_subject_decl);
    step_where = ast_intent_step_where_type(step);
    step_causes = ast_intent_step_causes_effect(step);

    if (step_where != NULL
        && !ast_intent_step_inherited_where_from_action(step)
        && !ast_intent_step_derived_where_from_using(step)
        && !ast_intent_step_derived_where_from_transfer(step)
        && ast_func_within_zone(action_decl) != NULL
        && step_where->type == AST_TYPE
        && ast_type_name(step_where) != NULL
        && strcmp(ast_type_name(step_where),
            ast_func_within_zone(action_decl)) == 0) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- where", has_any ? "\n" : "");
        has_any = true;
    }

    if (ast_intent_step_who_count(step) > 0
        && !ast_intent_step_inherited_who_from_action(step)
        && !ast_intent_step_derived_who_from_on_receiver(step)
        && !ast_intent_step_derived_who_from_single_participant(step)
        && !intent_step_has_local_authorized_by(step)
        && intent_step_same_who_binding(intent_decl, step, action_subject_decl)) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- who", has_any ? "\n" : "");
        has_any = true;
    }

    if (ast_intent_step_required_ability_count(step) > 0
        && !ast_intent_step_inherited_requires_from_action(step)
        && ast_func_required_ability_count(action_decl) > 0
        && intent_step_same_ability_list(step, action_decl)) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- requires", has_any ? "\n" : "");
        has_any = true;
    }

    if (step_causes != NULL
        && !ast_intent_step_inherited_causes_from_action(step)
        && ast_func_causes_effect(action_decl) != NULL
        && strcmp(step_causes, ast_func_causes_effect(action_decl)) == 0) {
        (void)pergyra_str_appendf(redundant, sizeof(redundant),
                                  "%s- causes", has_any ? "\n" : "");
        has_any = true;
    }

    if (ast_intent_step_authorized_by_count(step) > 0
        && !ast_intent_step_inherited_authorized_by_from_action(step)
        && ast_func_authorized_by_count(action_decl) > 0
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
intent_step_inherit_action_contract(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);

    if (action_decl == NULL)
        return;

    if (ast_intent_step_where_type(step) == NULL
        && ast_func_within_zone(action_decl) != NULL) {
        if (ast_intent_step_set_where_type(step,
                ast_create_type(ast_func_within_zone(action_decl)))) {
            ast_intent_step_mark_inherited_where_from_action(step);
        }
    }

    if (ast_intent_step_required_ability_count(step) == 0
        && ast_func_required_ability_count(action_decl) > 0) {
        bool copied_all = true;
        for (size_t i = 0; i < ast_func_required_ability_count(action_decl); i++) {
            ASTNode *ability_ref = ast_func_required_ability(action_decl, i);
            if (!ast_intent_step_append_required_ability_clone(step, ability_ref)) {
                copied_all = false;
                break;
            }
        }
        if (copied_all)
            ast_intent_step_mark_inherited_requires_from_action(step);
    }

    if (ast_intent_step_causes_effect(step) == NULL
        && ast_func_causes_effect(action_decl) != NULL) {
        if (ast_intent_step_set_causes_effect_copy(step,
                ast_func_causes_effect(action_decl))) {
            ast_intent_step_mark_inherited_causes_from_action(step);
        }
    }

    if (ast_intent_step_authorized_by_count(step) == 0
        && ast_func_authorized_by_count(action_decl) > 0) {
        bool mapped_all = true;
        char **step_who;
        size_t step_who_count;
        ASTNode **involves_nodes;
        size_t involve_count;

        step_who = ast_intent_step_who_names(step, &step_who_count);
        involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);

        for (size_t i = 0; i < ast_func_authorized_by_count(action_decl); i++) {
            const char *binding_name = ast_func_authorized_by(action_decl, i);
            const char *binding_type_name = intent_action_binding_type_name(
                action_decl, action_subject_decl, ctx, binding_name);
            const char *mapped_alias = NULL;

            if (strcmp(binding_name, "self") == 0) {
                if (step_who_count == 1) {
                    mapped_alias = step_who[0];
                }
            } else if (binding_type_name != NULL) {
                for (size_t j = 0; j < involve_count; j++) {
                    ASTNode *involves = involves_nodes[j];
                    const char *participant_type_name = intent_involves_type_name(involves);
                    if (participant_type_name != NULL
                        && strcmp(participant_type_name, binding_type_name) == 0) {
                        if (mapped_alias != NULL) {
                            mapped_alias = NULL;
                            break;
                        }
                        mapped_alias = ast_intent_involves_alias(involves);
                    }
                }
            }

            if (mapped_alias == NULL) {
                mapped_all = false;
                break;
            }
            if (!ast_intent_step_append_authorized_by_copy(step, mapped_alias)) {
                mapped_all = false;
                break;
            }
        }

        if (mapped_all) {
            ast_intent_step_mark_inherited_authorized_by_from_action(step);
        } else {
            ast_intent_step_clear_authorized_by(step);
        }
    }
}
