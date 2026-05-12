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
    callee = expr->data.call.callee;
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return NULL;
    receiver = callee->data.member.object;
    if (receiver == NULL || receiver->type != AST_IDENTIFIER)
        return NULL;
    if (method_name_out != NULL)
        *method_name_out = callee->data.member.name;
    return receiver->data.identifier.name;
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

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent_decl == NULL || ctx == NULL || alias == NULL || method_name == NULL)
        return NULL;
    involves = find_intent_involves_local(intent_decl, alias);
    if (!intent_involves_is_subject_host(ctx->program_root, involves))
        return NULL;
    subject_decl = find_subject_host_decl_by_name(
        ctx->program_root, intent_involves_type_name(involves));
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

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }

    for (size_t i = 0; i < step->data.intent_step.on_expr_count; i++) {
        const char *alias = NULL;
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, step->data.intent_step.on_exprs[i], &alias);

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
    if (step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.on_expr_count != 1) {
        return NULL;
    }
    return step->data.intent_step.on_exprs[0];
}

static bool
func_decl_first_param_is_implicit_self(ASTNode *action_decl)
{
    FuncParam *param;

    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL
        || action_decl->data.func_decl.param_count == 0
        || action_decl->data.func_decl.params == NULL) {
        return false;
    }
    param = action_decl->data.func_decl.params[0];
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
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *param = action_decl->data.func_decl.params != NULL
            ? action_decl->data.func_decl.params[i] : NULL;

        if (param == NULL || param->name == NULL
            || strcmp(param->name, param_name) != 0) {
            continue;
        }
        if (i == 0 && has_implicit_self)
            return NULL;
        arg_index = has_implicit_self ? i - 1 : i;
        if (arg_index >= on_expr->data.call.arg_count)
            return NULL;
        return on_expr->data.call.arguments[arg_index];
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
        || action_decl->data.func_decl.authorized_by_count != 1
        || action_decl->data.func_decl.authorized_by == NULL
        || action_decl->data.func_decl.authorized_by[0] == NULL) {
        return NULL;
    }

    auth_name = action_decl->data.func_decl.authorized_by[0];
    if (strcmp(auth_name, "self") == 0)
        return receiver_alias;

    on_expr = intent_single_on_call(step);
    arg = intent_on_call_arg_for_action_param(on_expr, action_decl, auth_name);
    if (arg == NULL || arg->type != AST_IDENTIFIER
        || find_intent_involves_local(intent_decl,
               arg->data.identifier.name) == NULL) {
        return NULL;
    }
    return arg->data.identifier.name;
}

void
intent_step_derive_who_from_on_receiver(ASTNode *intent_decl,
                                        ASTNode *step,
                                        SemanticContext *ctx)
{
    const char *matched_alias = NULL;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.who_count != 0) {
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.on_expr_count; i++) {
        const char *alias = NULL;
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, step->data.intent_step.on_exprs[i], &alias);

        if (alias == NULL || action_decl == NULL)
            continue;
        if (matched_alias != NULL && strcmp(matched_alias, alias) != 0)
            return;
        matched_alias = alias;
    }

    if (matched_alias != NULL
        && intent_semantic_append_name(&step->data.intent_step.who_names,
               &step->data.intent_step.who_count,
               &step->data.intent_step.who_capacity,
               matched_alias)) {
        step->data.intent_step.derived_who_from_on_receiver = true;
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

    if (step->data.intent_step.required_ability_count == 0
        && action_decl->data.func_decl.required_ability_count > 0) {
        bool copied_all = true;
        for (size_t i = 0; i < action_decl->data.func_decl.required_ability_count; i++) {
            if (!intent_step_append_required_ability_clone(
                    step, action_decl->data.func_decl.required_abilities[i])) {
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
        step->data.intent_step.inherited_causes_from_action =
            (step->data.intent_step.causes_effect != NULL);
    }

    if (step->data.intent_step.authorized_by_count == 0
        && receiver_alias != NULL) {
        const char *authorized_alias =
            intent_step_authorized_by_alias_from_action(
                intent_decl, step, action_decl, receiver_alias);
        if (authorized_alias != NULL
            && intent_semantic_append_name(&step->data.intent_step.authorized_by,
               &step->data.intent_step.authorized_by_count,
               &step->data.intent_step.authorized_by_capacity,
               authorized_alias)) {
            step->data.intent_step.inherited_authorized_by_from_action = true;
        }
    }
}

void
intent_step_derive_where_from_on_receiver(ASTNode *intent_decl,
                                          ASTNode *step,
                                          SemanticContext *ctx)
{
    const char *matched_zone = NULL;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type != NULL) {
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.on_expr_count; i++) {
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, step->data.intent_step.on_exprs[i], NULL);
        const char *zone_name;

        if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
            continue;
        zone_name = action_decl->data.func_decl.within_zone;
        if (zone_name == NULL)
            continue;
        if (matched_zone != NULL && strcmp(matched_zone, zone_name) != 0)
            return;
        matched_zone = zone_name;
    }

    if (matched_zone != NULL) {
        step->data.intent_step.where_type = ast_create_type(matched_zone);
        step->data.intent_step.inherited_where_from_action = true;
    }
}

bool
intent_step_report_on_action_zone_conflict(ASTNode *intent_decl,
                                           ASTNode *step,
                                           SemanticContext *ctx)
{
    const char *first_zone = NULL;
    const char *second_zone = NULL;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type != NULL) {
        return false;
    }

    for (size_t i = 0; i < step->data.intent_step.on_expr_count; i++) {
        ASTNode *action_decl = intent_on_call_action(
            intent_decl, ctx, step->data.intent_step.on_exprs[i], NULL);
        const char *zone_name;

        if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
            continue;
        zone_name = action_decl->data.func_decl.within_zone;
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
        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
        first_zone,
        second_zone);
    return true;
}
