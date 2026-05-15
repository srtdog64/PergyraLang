/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>

bool
ast_intent_step_append_authorized_by_copy(ASTNode* node, const char* alias)
{
    char **grown;
    char *owned_alias;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    if (node->data.intent_step.authorized_by_count
        == node->data.intent_step.authorized_by_capacity) {
        next_capacity = node->data.intent_step.authorized_by_capacity == 0
            ? 4
            : node->data.intent_step.authorized_by_capacity * 2;
        if (next_capacity < node->data.intent_step.authorized_by_capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_alias);
            return false;
        }
        grown = realloc(node->data.intent_step.authorized_by,
            next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_alias);
            return false;
        }
        node->data.intent_step.authorized_by = grown;
        node->data.intent_step.authorized_by_capacity = next_capacity;
    }
    node->data.intent_step.authorized_by[
        node->data.intent_step.authorized_by_count++] = owned_alias;
    return true;
}

bool
ast_intent_step_append_who_name_copy(ASTNode* node, const char* alias)
{
    char **grown;
    char *owned_alias;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    if (node->data.intent_step.who_count
        == node->data.intent_step.who_capacity) {
        next_capacity = node->data.intent_step.who_capacity == 0
            ? 4
            : node->data.intent_step.who_capacity * 2;
        if (next_capacity < node->data.intent_step.who_capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_alias);
            return false;
        }
        grown = realloc(node->data.intent_step.who_names,
            next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_alias);
            return false;
        }
        node->data.intent_step.who_names = grown;
        node->data.intent_step.who_capacity = next_capacity;
    }
    node->data.intent_step.who_names[
        node->data.intent_step.who_count++] = owned_alias;
    return true;
}

bool
ast_intent_step_append_required_ability_clone(ASTNode* node, ASTNode* ability)
{
    ASTNode **grown;
    ASTNode *ability_copy;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || ability == NULL)
        return false;
    ability_copy = ast_clone(ability);
    if (ability_copy == NULL)
        return false;
    if (node->data.intent_step.required_ability_count
        == node->data.intent_step.required_ability_capacity) {
        next_capacity = node->data.intent_step.required_ability_capacity == 0
            ? 4
            : node->data.intent_step.required_ability_capacity * 2;
        if (next_capacity < node->data.intent_step.required_ability_capacity
            || next_capacity > SIZE_MAX / sizeof(ASTNode *)) {
            ast_destroy(ability_copy);
            return false;
        }
        grown = realloc(node->data.intent_step.required_abilities,
            next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            ast_destroy(ability_copy);
            return false;
        }
        node->data.intent_step.required_abilities = grown;
        node->data.intent_step.required_ability_capacity = next_capacity;
    }
    node->data.intent_step.required_abilities[
        node->data.intent_step.required_ability_count++] = ability_copy;
    return true;
}

void
ast_intent_step_clear_authorized_by(ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return;
    for (size_t i = 0; i < node->data.intent_step.authorized_by_count; i++)
        free(node->data.intent_step.authorized_by[i]);
    free(node->data.intent_step.authorized_by);
    node->data.intent_step.authorized_by = NULL;
    node->data.intent_step.authorized_by_count = 0;
    node->data.intent_step.authorized_by_capacity = 0;
}

void
ast_intent_step_mark_inherited_who_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_who_from_action = true;
}

void
ast_intent_step_mark_derived_who_from_on_receiver(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_who_from_on_receiver = true;
}

void
ast_intent_step_mark_derived_who_from_single_participant(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_who_from_single_participant = true;
}

void
ast_intent_step_mark_inherited_where_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_where_from_action = true;
}

void
ast_intent_step_mark_inherited_requires_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_requires_from_action = true;
}

void
ast_intent_step_mark_inherited_causes_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_causes_from_action = true;
}

void
ast_intent_step_mark_inherited_authorized_by_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_authorized_by_from_action = true;
}

void
ast_intent_step_mark_derived_authorized_by_from_zone(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_authorized_by_from_zone = true;
}

void
ast_intent_step_mark_derived_where_from_using(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_where_from_using = true;
}

void
ast_intent_step_mark_derived_where_from_transfer(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_where_from_transfer = true;
}

void
ast_intent_step_mark_derived_using_from_transfer(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_using_from_transfer = true;
}

void
ast_intent_step_mark_derived_using_from_where(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_using_from_where = true;
}

const char*
ast_zone_layer_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return NULL;
    return node->data.zone_layer_slot.slot_name;
}

const char*
ast_zone_layer_slot_layer_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return NULL;
    return node->data.zone_layer_slot.layer_type;
}

bool
ast_zone_layer_slot_is_relation(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_LAYER_SLOT
        && node->data.zone_layer_slot.is_relation;
}

bool
ast_zone_layer_slot_is_pool(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_LAYER_SLOT
        && node->data.zone_layer_slot.is_pool;
}

int
ast_zone_layer_slot_pool_capacity(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return 0;
    return node->data.zone_layer_slot.pool_capacity;
}

const char*
ast_zone_state_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.state_name;
}

bool
ast_zone_state_is_relation(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_STATE
        && node->data.zone_state.is_relation;
}

const char*
ast_zone_state_layer_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.layer_slot_name;
}

const char*
ast_zone_state_left_or_target_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.left_or_target_slot_name;
}

const char*
ast_zone_state_right_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.right_slot_name;
}
