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

const char *
intent_step_single_who_alias(const ASTNode *step)
{
    char **who_names;
    size_t who_count;

    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    who_names = ast_intent_step_who_names(step, &who_count);
    if (who_count != 1 || who_names == NULL) {
        return NULL;
    }
    return who_names[0];
}

bool
intent_step_set_where_type_name(ASTNode *step,
                                const char *zone_name,
                                IntentStepWhereProvenance provenance)
{
    ASTNode *where_type;

    if (step == NULL || step->type != AST_INTENT_STEP || zone_name == NULL)
        return false;

    where_type = ast_create_type(zone_name);
    if (where_type == NULL)
        return false;
    if (!ast_intent_step_set_where_type(step, where_type)) {
        ast_destroy(where_type);
        return false;
    }

    switch (provenance) {
    case INTENT_STEP_WHERE_PROVENANCE_INHERITED_ACTION:
        ast_intent_step_mark_inherited_where_from_action(step);
        break;
    case INTENT_STEP_WHERE_PROVENANCE_DERIVED_USING:
        ast_intent_step_mark_derived_where_from_using(step);
        break;
    case INTENT_STEP_WHERE_PROVENANCE_DERIVED_TRANSFER:
        ast_intent_step_mark_derived_where_from_transfer(step);
        break;
    }
    return true;
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
    ASTNode *subject_type = ast_intent_involves_subject_type(involves);

    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || subject_type == NULL
        || ast_type_name(subject_type) == NULL) {
        return NULL;
    }
    return ast_type_name(subject_type);
}

bool
intent_involves_is_subject_host(ASTNode *involves, SemanticContext *ctx)
{
    Type *type = intent_resolve_involves_type(involves, ctx);
    ASTNode *decl = NULL;

    if (type == NULL || type == TYPE_UNKNOWN || ctx == NULL)
        return false;
    decl = semantic_host_decl_for_type(ctx, type);
    return decl_is_subject_host(decl);
}

ASTNode *
subject_decl_find_action_named(ASTNode *decl, const char *action_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || action_name == NULL
        || ast_class_nominal_kind(decl) != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }

    size_t method_count = 0;
    ASTNode **methods = ast_class_methods(decl, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        const char *method_name = ast_declaration_name(method);
        if (method != NULL && method->type == AST_FUNC_DECL
            && ast_func_is_action(method)
            && method_name != NULL
            && strcmp(method_name, action_name) == 0) {
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
        for (size_t i = 0; i < ast_zone_refresh_field_map_count(refresh); i++) {
            const char *mapped_target =
                ast_zone_refresh_mapped_target_field(refresh, i);
            const char *mapped_source =
                ast_zone_refresh_mapped_source_field(refresh, i);
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
    size_t field_count = projection_source_field_count(target_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(target_decl, i);
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
