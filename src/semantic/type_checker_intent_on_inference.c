/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent on-clause inference rules. These rules compress user surface syntax
 * but still materialize explicit intent-step facts before DIR/AIR lowering.
 */

#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"

#include "../common/string_compat.h"

#include <string.h>

static const char *
intent_on_call_receiver_alias(ASTNode *expr, const char **method_name_out)
{
    ASTNode *callee;
    ASTNode *receiver;

    if (method_name_out != NULL)
        *method_name_out = NULL;
    if (expr == NULL || expr->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return NULL;
    receiver = ast_member_object(callee);
    if (receiver == NULL || receiver->type != AST_IDENTIFIER)
        return NULL;
    if (method_name_out != NULL)
        *method_name_out = ast_member_name(callee);
    return ast_identifier_name(receiver);
}

static ASTNode *
intent_on_call_action(ASTNode *intent_decl,
                      SemanticContext *ctx,
                      ASTNode *on_expr,
                      const char **alias_out)
{
    const char *method_name = NULL;
    const char *alias = intent_on_call_receiver_alias(on_expr, &method_name);
    ASTNode *involves;
    ASTNode *subject_decl;
    Type *subject_type;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent_decl == NULL || ctx == NULL || alias == NULL || method_name == NULL)
        return NULL;
    involves = find_intent_involves_local(intent_decl, alias);
    if (!intent_involves_is_subject_host(involves, ctx))
        return NULL;
    subject_type = intent_resolve_involves_type(involves, ctx);
    subject_decl = semantic_host_decl_for_type(ctx, subject_type);
    if (alias_out != NULL)
        *alias_out = alias;
    return subject_decl_find_action_named(subject_decl, method_name);
}

static ASTNode *
intent_single_on_action(ASTNode *intent_decl,
                        ASTNode *step,
                        SemanticContext *ctx,
                        const char **alias_out)
{
    ASTNode *matched_action = NULL;
    const char *matched_alias = NULL;
    ASTNode **on_exprs;
    size_t on_expr_count;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }
    on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);

    for (size_t i = 0; i < on_expr_count; i++) {
        const char *alias = NULL;
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, on_exprs[i], &alias);

        if (action_decl == NULL || alias == NULL)
            return NULL;
        if (matched_action != NULL
            && (matched_action != action_decl
                || strcmp(matched_alias, alias) != 0)) {
            return NULL;
        }
        matched_action = action_decl;
        matched_alias = alias;
    }

    if (alias_out != NULL)
        *alias_out = matched_alias;
    return matched_action;
}

static ASTNode *
intent_single_on_call(ASTNode *step)
{
    ASTNode **on_exprs;
    size_t on_expr_count;

    if (step == NULL || step->type != AST_INTENT_STEP
        || ast_intent_step_on_expr_count(step) != 1) {
        return NULL;
    }
    on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);
    (void)on_expr_count;
    return on_exprs != NULL ? on_exprs[0] : NULL;
}

static bool
func_decl_first_param_is_implicit_self(ASTNode *action_decl)
{
    FuncParam *param;

    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL
        || ast_func_param_count(action_decl) == 0) {
        return false;
    }
    param = ast_func_param(action_decl, 0);
    return param != NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0
        && param->type == NULL;
}

static ASTNode *
intent_on_call_arg_for_action_param(ASTNode *on_expr,
                                    ASTNode *action_decl,
                                    const char *param_name)
{
    bool has_implicit_self;
    size_t arg_index;

    if (on_expr == NULL || on_expr->type != AST_CALL
        || action_decl == NULL || action_decl->type != AST_FUNC_DECL
        || param_name == NULL) {
        return NULL;
    }

    has_implicit_self = func_decl_first_param_is_implicit_self(action_decl);
    for (size_t i = 0; i < ast_func_param_count(action_decl); i++) {
        FuncParam *param = ast_func_param(action_decl, i);

        if (param == NULL || param->name == NULL
            || strcmp(param->name, param_name) != 0) {
            continue;
        }
        if (i == 0 && has_implicit_self)
            return NULL;
        arg_index = has_implicit_self ? i - 1 : i;
        if (arg_index >= ast_call_arg_count(on_expr))
            return NULL;
        return ast_call_argument(on_expr, arg_index);
    }

    return NULL;
}

static const char *
intent_step_authorized_by_alias_from_action(ASTNode *intent_decl,
                                            ASTNode *step,
                                            ASTNode *action_decl,
                                            const char *receiver_alias)
{
    const char *auth_name;
    ASTNode *on_expr;
    ASTNode *arg;

    if (intent_decl == NULL || step == NULL || action_decl == NULL
        || action_decl->type != AST_FUNC_DECL
        || ast_func_authorized_by_count(action_decl) != 1
        || ast_func_authorized_by(action_decl, 0) == NULL) {
        return NULL;
    }

    auth_name = ast_func_authorized_by(action_decl, 0);
    if (strcmp(auth_name, "self") == 0)
        return receiver_alias;

    on_expr = intent_single_on_call(step);
    arg = intent_on_call_arg_for_action_param(on_expr, action_decl, auth_name);
    if (arg == NULL || arg->type != AST_IDENTIFIER
        || find_intent_involves_local(intent_decl,
               ast_identifier_name(arg)) == NULL) {
        return NULL;
    }
    return ast_identifier_name(arg);
}

void
intent_step_derive_who_from_on_receiver(ASTNode *intent_decl,
                                        ASTNode *step,
                                        SemanticContext *ctx)
{
    const char *matched_alias = NULL;
    ASTNode **on_exprs;
    size_t on_expr_count;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_who_count(step) != 0) {
        return;
    }
    on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);

    for (size_t i = 0; i < on_expr_count; i++) {
        const char *alias = NULL;
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, on_exprs[i], &alias);

        if (alias == NULL || action_decl == NULL)
            continue;
        if (matched_alias != NULL && strcmp(matched_alias, alias) != 0)
            return;
        matched_alias = alias;
    }

    if (matched_alias != NULL
        && ast_intent_step_append_who_name_copy(step, matched_alias)) {
        ast_intent_step_mark_derived_who_from_on_receiver(step);
    }
}

void
intent_step_derive_who_from_action(ASTNode *intent_decl, ASTNode *step,
                                   SemanticContext *ctx)
{
    const char *matched_alias = NULL;
    const char *step_name;
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_who_count(step) != 0) {
        return;
    }
    step_name = ast_intent_step_name(step);
    involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        ASTNode *subject_decl;
        ASTNode *action_decl;
        Type *subject_type;

        if (!intent_involves_is_subject_host(involves, ctx))
            continue;
        subject_type = intent_resolve_involves_type(involves, ctx);
        subject_decl = semantic_host_decl_for_type(ctx, subject_type);
        action_decl = subject_decl_find_action_named(subject_decl, step_name);
        if (action_decl == NULL)
            continue;

        if (matched_alias != NULL)
            return;
        matched_alias = ast_intent_involves_alias(involves);
    }

    if (matched_alias != NULL
        && ast_intent_step_append_who_name_copy(step, matched_alias)) {
        ast_intent_step_mark_inherited_who_from_action(step);
    }
}

void
intent_step_derive_who_from_single_participant(ASTNode *intent_decl,
                                               ASTNode *step,
                                               SemanticContext *ctx)
{
    const char *matched_alias = NULL;
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_who_count(step) != 0) {
        return;
    }

    involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        const char *alias;

        if (!intent_involves_is_subject_host(involves, ctx))
            continue;
        alias = ast_intent_involves_alias(involves);
        if (alias == NULL)
            continue;
        if (matched_alias != NULL)
            return;
        matched_alias = alias;
    }

    if (matched_alias != NULL
        && ast_intent_step_append_who_name_copy(step, matched_alias)) {
        ast_intent_step_mark_derived_who_from_single_participant(step);
    }
}

void
intent_step_inherit_contract_from_on_receiver(ASTNode *intent_decl,
                                              ASTNode *step,
                                              SemanticContext *ctx)
{
    const char *receiver_alias = NULL;
    ASTNode *action_decl = intent_single_on_action(
        intent_decl, step, ctx, &receiver_alias);

    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return;

    if (ast_intent_step_required_ability_count(step) == 0
        && ast_func_required_ability_count(action_decl) > 0) {
        bool copied_all = true;
        for (size_t i = 0; i < ast_func_required_ability_count(action_decl); i++) {
            if (!ast_intent_step_append_required_ability_clone(
                    step, ast_func_required_ability(action_decl, i))) {
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
        && receiver_alias != NULL) {
        const char *authorized_alias =
            intent_step_authorized_by_alias_from_action(
                intent_decl, step, action_decl, receiver_alias);
        if (authorized_alias != NULL
            && ast_intent_step_append_authorized_by_copy(
                step, authorized_alias)) {
            ast_intent_step_mark_inherited_authorized_by_from_action(step);
        }
    }
}

void
intent_step_derive_where_from_on_receiver(ASTNode *intent_decl,
                                          ASTNode *step,
                                          SemanticContext *ctx)
{
    const char *matched_zone = NULL;
    ASTNode **on_exprs;
    size_t on_expr_count;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_where_type(step) != NULL) {
        return;
    }
    on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);

    for (size_t i = 0; i < on_expr_count; i++) {
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, on_exprs[i], NULL);
        const char *zone_name;

        if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
            continue;
        zone_name = ast_func_within_zone(action_decl);
        if (zone_name == NULL)
            continue;
        if (matched_zone != NULL && strcmp(matched_zone, zone_name) != 0)
            return;
        matched_zone = zone_name;
    }

    if (matched_zone != NULL)
        (void)intent_step_set_where_type_name(
            step, matched_zone,
            INTENT_STEP_WHERE_PROVENANCE_INHERITED_ACTION);
}

bool
intent_step_report_on_action_zone_conflict(ASTNode *intent_decl,
                                           ASTNode *step,
                                           SemanticContext *ctx)
{
    const char *first_zone = NULL;
    const char *second_zone = NULL;
    ASTNode **on_exprs;
    size_t on_expr_count;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_where_type(step) != NULL) {
        return false;
    }
    on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);

    for (size_t i = 0; i < on_expr_count; i++) {
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, on_exprs[i], NULL);
        const char *zone_name;

        if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
            continue;
        zone_name = ast_func_within_zone(action_decl);
        if (zone_name == NULL)
            continue;
        if (first_zone == NULL) {
            first_zone = zone_name;
            continue;
        }
        if (strcmp(first_zone, zone_name) != 0) {
            second_zone = zone_name;
            break;
        }
    }

    if (first_zone == NULL || second_zone == NULL)
        return false;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_INTENT_STEP_INVALID,
        PGY_CAUSE_INTENT_STEP,
        PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS,
        step,
        "Intent step '%s' cannot infer a where zone from on-call actions.\n"
        "Reason:\n"
        "- compact step inference found action contracts in both '%s' and '%s'\n"
        "- multiple on-call action zones make the step boundary ambiguous\n"
        "Fix:\n"
        "- add an explicit 'where: <Zone>;' and matching 'using: <zoneAlias>;'\n"
        "- or split the on-calls into separate intent steps with one zone each",
        ast_intent_step_name(step) != NULL
            ? ast_intent_step_name(step) : "<step>",
        first_zone,
        second_zone);
    return true;
}
