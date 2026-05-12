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
#include "diag_codes.h"

#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool
intent_semantic_append_name(char ***items, size_t *count, size_t *capacity,
                            const char *name)
{
    char **grown;
    char *owned_name;

    if (items == NULL || count == NULL || capacity == NULL || name == NULL)
        return false;
    owned_name = pergyra_strdup(name);
    if (owned_name == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity < *capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_name);
            return false;
        }
        grown = realloc(*items, next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_name);
            return false;
        }
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = owned_name;
    (*count)++;
    return true;
}

bool
intent_step_append_required_ability_clone(ASTNode *step, ASTNode *ability)
{
    ASTNode **grown;
    ASTNode *ability_copy;

    if (step == NULL || ability == NULL || step->type != AST_INTENT_STEP)
        return false;
    ability_copy = ast_clone(ability);
    if (ability_copy == NULL)
        return false;
    if (step->data.intent_step.required_ability_count
        == step->data.intent_step.required_ability_capacity) {
        size_t next_capacity =
            step->data.intent_step.required_ability_capacity == 0
                ? 4
                : step->data.intent_step.required_ability_capacity * 2;
        if (next_capacity < step->data.intent_step.required_ability_capacity
            || next_capacity > SIZE_MAX / sizeof(ASTNode *)) {
            ast_destroy(ability_copy);
            return false;
        }
        grown = realloc(step->data.intent_step.required_abilities,
            next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            ast_destroy(ability_copy);
            return false;
        }
        step->data.intent_step.required_abilities = grown;
        step->data.intent_step.required_ability_capacity = next_capacity;
    }
    step->data.intent_step.required_abilities[
        step->data.intent_step.required_ability_count++] = ability_copy;
    return true;
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

ASTNode *
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

bool
subject_decl_has_action_named(ASTNode *decl, const char *action_name)
{
    return subject_decl_find_action_named(decl, action_name) != NULL;
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
