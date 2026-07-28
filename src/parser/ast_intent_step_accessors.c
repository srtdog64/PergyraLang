/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>

const char*
ast_intent_step_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.name;
}

ASTNode*
ast_intent_step_where_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.where_type;
}

ASTNode*
ast_intent_step_using_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.using_expr;
}

ASTNode*
ast_intent_step_intent_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.intent_expr;
}

const char*
ast_intent_step_transfer_from_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.transfer_from_alias;
}

const char*
ast_intent_step_transfer_to_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.transfer_to_alias;
}

bool
ast_intent_step_replace_transfer_to_alias_copy(ASTNode* node, const char* alias)
{
    char *owned_alias;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    free(node->data.intent_step.transfer_to_alias);
    node->data.intent_step.transfer_to_alias = owned_alias;
    return true;
}

char**
ast_intent_step_who_names(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.who_count;
    return node->data.intent_step.who_names;
}

size_t
ast_intent_step_who_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.who_count;
}

ASTNode**
ast_intent_step_on_exprs(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.on_expr_count;
    return node->data.intent_step.on_exprs;
}

size_t
ast_intent_step_on_expr_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.on_expr_count;
}

const char*
ast_intent_step_outcome_binding_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.outcome_binding_name;
}

size_t
ast_intent_step_outcome_binding_length(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.outcome_binding_length;
}

uint32_t
ast_intent_step_outcome_binding_line(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.outcome_binding_line;
}

uint32_t
ast_intent_step_outcome_binding_column(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.outcome_binding_column;
}

const char*
ast_intent_step_outcome_binding_type_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.outcome_binding_type_name;
}

uint32_t
ast_intent_step_outcome_action_decl_syntax_id(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.outcome_action_decl_syntax_id;
}

bool
ast_intent_step_set_outcome_binding_copy(ASTNode* node,
                                         const char* name,
                                         size_t length,
                                         uint32_t line,
                                         uint32_t column)
{
    char *copy;

    if (node == NULL || node->type != AST_INTENT_STEP || name == NULL
        || name[0] == '\0' || length == 0) {
        return false;
    }
    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    free(node->data.intent_step.outcome_binding_name);
    node->data.intent_step.outcome_binding_name = copy;
    node->data.intent_step.outcome_binding_length = length;
    node->data.intent_step.outcome_binding_line = line;
    node->data.intent_step.outcome_binding_column = column;
    return true;
}

bool
ast_intent_step_set_outcome_resolution_copy(ASTNode* node,
                                            const char* type_name,
                                            uint32_t action_decl_syntax_id)
{
    char *copy;

    if (node == NULL || node->type != AST_INTENT_STEP || type_name == NULL
        || type_name[0] == '\0' || action_decl_syntax_id == 0) {
        return false;
    }
    copy = pergyra_strdup(type_name);
    if (copy == NULL)
        return false;
    free(node->data.intent_step.outcome_binding_type_name);
    node->data.intent_step.outcome_binding_type_name = copy;
    node->data.intent_step.outcome_action_decl_syntax_id =
        action_decl_syntax_id;
    return true;
}

ASTNode**
ast_intent_step_compensate_exprs(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.compensate_expr_count;
    return node->data.intent_step.compensate_exprs;
}

size_t
ast_intent_step_compensate_expr_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.compensate_expr_count;
}

ASTNode*
ast_intent_step_pre_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.pre_expr;
}

ASTNode*
ast_intent_step_guard_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.guard_expr;
}

ASTNode*
ast_intent_step_post_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.post_expr;
}

ASTNode*
ast_intent_step_invariant_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.invariant_expr;
}

ASTNode**
ast_intent_step_required_abilities(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.required_ability_count;
    return node->data.intent_step.required_abilities;
}

size_t
ast_intent_step_required_ability_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.required_ability_count;
}

const char*
ast_intent_step_causes_effect(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.causes_effect;
}

char**
ast_intent_step_authorized_by(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.authorized_by_count;
    return node->data.intent_step.authorized_by;
}

size_t
ast_intent_step_authorized_by_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.authorized_by_count;
}

ASTNode*
ast_intent_step_expect_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.expect_expr;
}

bool
ast_intent_step_inherited_who_from_intent(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_who_from_intent;
}

bool
ast_intent_step_derived_who_from_on_receiver(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_who_from_on_receiver;
}

bool
ast_intent_step_derived_who_from_single_participant(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_who_from_single_participant;
}

bool
ast_intent_step_inherited_where_from_intent(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_where_from_intent;
}

bool
ast_intent_step_inherited_who_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_who_from_action;
}

bool
ast_intent_step_inherited_where_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_where_from_action;
}

bool
ast_intent_step_inherited_requires_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_requires_from_action;
}

bool
ast_intent_step_inherited_causes_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_causes_from_action;
}

bool
ast_intent_step_inherited_authorized_by_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_authorized_by_from_action;
}

bool
ast_intent_step_derived_authorized_by_from_zone(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_authorized_by_from_zone;
}

bool
ast_intent_step_derived_where_from_using(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_where_from_using;
}

bool
ast_intent_step_derived_where_from_transfer(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_where_from_transfer;
}

bool
ast_intent_step_derived_using_from_transfer(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_using_from_transfer;
}

bool
ast_intent_step_derived_using_from_where(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_using_from_where;
}

bool
ast_intent_step_set_where_type(ASTNode* node, ASTNode* where_type)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return false;
    node->data.intent_step.where_type = where_type;
    return true;
}

bool
ast_intent_step_set_using_expr(ASTNode* node, ASTNode* using_expr)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return false;
    node->data.intent_step.using_expr = using_expr;
    return true;
}

bool
ast_intent_step_set_causes_effect_copy(ASTNode* node, const char* causes_effect)
{
    if (node == NULL || node->type != AST_INTENT_STEP || causes_effect == NULL)
        return false;
    node->data.intent_step.causes_effect = pergyra_strdup(causes_effect);
    return node->data.intent_step.causes_effect != NULL;
}
