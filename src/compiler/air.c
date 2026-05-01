/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR (Abstraction Intent Representation) synthesis owner.
 */

#include "air.h"
#include "air_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
air_vformat(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return NULL;

    char *buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL)
        return NULL;
    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    return buffer;
}

void
air_set_error(char **error_message, const char *fmt, ...)
{
    if (error_message == NULL)
        return;
    va_list args;
    va_start(args, fmt);
    *error_message = air_vformat(fmt, args);
    va_end(args);
}

char *
air_strdup_owned(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static bool
air_ensure_owned_name_capacity(AIRProgram *air)
{
    char **grown;
    size_t new_capacity;

    if (air == NULL)
        return false;
    if (air->owned_name_count < air->owned_name_capacity)
        return true;

    new_capacity = air->owned_name_capacity == 0
        ? 16
        : air->owned_name_capacity * 2;
    if (new_capacity < air->owned_name_capacity
        || new_capacity > SIZE_MAX / sizeof(char *)) {
        return false;
    }

    grown = (char **)realloc(air->owned_names,
                             new_capacity * sizeof(char *));
    if (grown == NULL)
        return false;
    air->owned_names = grown;
    air->owned_name_capacity = new_capacity;
    return true;
}

const char *
air_program_owned_name(AIRProgram *air, const char *text)
{
    char *copy;

    if (air == NULL || text == NULL)
        return NULL;

    copy = air_strdup_owned(text);
    if (copy == NULL)
        return NULL;

    if (!air_ensure_owned_name_capacity(air)) {
        free(copy);
        return NULL;
    }

    air->owned_names[air->owned_name_count++] = copy;
    return copy;
}

bool
air_assign_owned_name(AIRProgram *air, const char **slot, const char *text)
{
    if (slot == NULL)
        return false;
    *slot = NULL;
    if (text == NULL)
        return true;
    *slot = air_program_owned_name(air, text);
    return *slot != NULL;
}

bool
air_assign_first_owned_name(AIRProgram *air,
                            const char **slot,
                            const char *text,
                            char **error_message,
                            const char *what)
{
    if (slot == NULL || *slot != NULL || text == NULL)
        return true;
    if (!air_assign_owned_name(air, slot, text)) {
        air_set_error(error_message, "AIR %s evidence name allocation failed", what);
        return false;
    }
    return true;
}

bool
air_append_evidence_node(AIRProgram *air,
                         AIREvidenceKind kind,
                         size_t boundary_index,
                         const char *provider_name,
                         const char *subject_name,
                         char **error_message)
{
    return air_append_evidence_node_ex(air,
                                       kind,
                                       boundary_index,
                                       provider_name,
                                       subject_name,
                                       1,
                                       0,
                                       error_message);
}

bool
air_append_evidence_node_ex(AIRProgram *air,
                            AIREvidenceKind kind,
                            size_t boundary_index,
                            const char *provider_name,
                            const char *subject_name,
                            size_t fact_count,
                            size_t fallback_count,
                            char **error_message)
{
    AIREvidenceNode *node;

    if (air == NULL) {
        air_set_error(error_message, "AIR evidence append requires a program");
        return false;
    }
    if (air->evidence_count >= air->evidence_capacity) {
        AIREvidenceNode *next;
        size_t new_capacity = air->evidence_capacity == 0
            ? 16
            : air->evidence_capacity * 2;
        if (new_capacity < air->evidence_capacity
            || new_capacity > SIZE_MAX / sizeof(AIREvidenceNode)) {
            air_set_error(error_message, "AIR evidence node allocation failed");
            return false;
        }
        next = (AIREvidenceNode *)realloc(air->evidence_nodes,
                                          new_capacity * sizeof(AIREvidenceNode));
        if (next == NULL) {
            air_set_error(error_message, "AIR evidence node allocation failed");
            return false;
        }
        air->evidence_nodes = next;
        air->evidence_capacity = new_capacity;
    }
    if (air->evidence_nodes == NULL) {
        air_set_error(error_message, "AIR evidence node allocation failed");
        return false;
    }

    node = &air->evidence_nodes[air->evidence_count];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->boundary_index = boundary_index;
    node->fact_count = fact_count;
    node->fallback_count = fallback_count;
    if (!air_assign_owned_name(air, &node->provider_name, provider_name)
        || !air_assign_owned_name(air, &node->subject_name, subject_name)) {
        air_set_error(error_message, "AIR evidence node provenance allocation failed");
        return false;
    }
    air->evidence_count++;
    return true;
}

static bool
air_assign_authority_names(AIRProgram *air,
                           AIRBoundaryNode *boundary,
                           const char **names,
                           size_t name_count)
{
    if (boundary == NULL)
        return false;
    boundary->authority_names = NULL;
    boundary->authority_name_count = 0;
    if (name_count == 0)
        return true;

    boundary->authority_names = (const char **)calloc(name_count, sizeof(char *));
    if (boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < name_count; i++) {
        const char *copy = air_program_owned_name(air, names != NULL ? names[i] : NULL);
        if (copy == NULL)
            return false;
        boundary->authority_names[i] = copy;
    }
    boundary->authority_name_count = name_count;
    return true;
}

void
air_clear_drifts(AIRProgram *air)
{
    if (air == NULL)
        return;
    for (size_t i = 0; i < air->drift_count; i++)
        free((char *)air->drifts[i].message);
    free(air->drifts);
    air->drifts = NULL;
    air->drift_count = 0;
    air->drift_capacity = 0;
}

static const char *
air_dir_node_name(const DIRProgram *dir, size_t node_id)
{
    if (dir == NULL)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].id == node_id)
            return dir->nodes[i].name;
    }
    return NULL;
}

static ASTNode *
air_dir_node_ast(const DIRProgram *dir, size_t node_id)
{
    if (dir == NULL)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].id == node_id)
            return dir->nodes[i].ast;
    }
    return NULL;
}

static AIRSyncClass
air_sync_from_dir_step(const DIRIntentStep *step)
{
    if (step == NULL)
        return AIR_SYNC_UNKNOWN;
    if (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL)
        return AIR_SYNC_ASYNC;
    return AIR_SYNC_SYNC;
}

static AIRFailureClass
air_failure_from_dir_step(const DIRIntentStep *step)
{
    if (step == NULL)
        return AIR_FAILURE_UNKNOWN;
    if (step->causes_effect_name != NULL)
        return AIR_FAILURE_COMPENSABLE;
    return AIR_FAILURE_RECOVERABLE;
}

static bool
air_strict_evidence_enabled(void)
{
    const char *value = getenv("PGY_AIR_STRICT_EVIDENCE");
    if (value == NULL || value[0] == '\0')
        return true;
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "off") == 0)
        return false;
    return true;
}

bool
air_name_matches(const char *a, const char *b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

AIRProgram *
air_synthesize(const HIRProgram *hir,
               const DIRProgram *dir,
               const RIRProgram *rir,
               char **error_message)
{
    if (dir == NULL) {
        air_set_error(error_message, "AIR synthesis requires DIR input");
        return NULL;
    }

    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    if (air == NULL) {
        air_set_error(error_message, "AIR program allocation failed");
        return NULL;
    }
    air->strict_evidence = air_strict_evidence_enabled();
    air->has_hir_input = hir != NULL;

    size_t intent_node_count = 0;
    size_t boundary_node_count = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        intent_node_count += dir->intents[i].step_count;
        for (size_t j = 0; j < dir->intents[i].step_count; j++) {
            if (air_step_has_zone_boundary(&dir->intents[i].steps[j]))
                boundary_node_count++;
            if (air_step_has_world_boundary(&dir->intents[i].steps[j]))
                boundary_node_count++;
            boundary_node_count += air_count_step_expr_boundaries(&dir->intents[i].steps[j]);
        }
    }

    if (intent_node_count > 0) {
        air->intents = (AIRIntentNode *)calloc(intent_node_count, sizeof(AIRIntentNode));
        if (air->intents == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR intent allocation failed");
            return NULL;
        }
    }
    if (boundary_node_count > 0) {
        air->boundaries = (AIRBoundaryNode *)calloc(boundary_node_count, sizeof(AIRBoundaryNode));
        if (air->boundaries == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR boundary allocation failed");
            return NULL;
        }
    }
    air->intent_count = intent_node_count;
    air->boundary_count = boundary_node_count;

    size_t intent_index = 0;
    size_t boundary_index = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        const char *owner_source = air_dir_node_name(dir, info->node_id);
        const char *owner = air_program_owned_name(air, owner_source);
        ASTNode *owner_ast = air_dir_node_ast(dir, info->node_id);
        if (owner_source != NULL && owner == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR owner name allocation failed");
            return NULL;
        }
        for (size_t j = 0; j < info->step_count; j++) {
            const DIRIntentStep *step = &info->steps[j];
            AIRSyncClass sync_class = air_sync_from_dir_step(step);
            air->intents[intent_index].intent_owner = owner;
            if (!air_assign_owned_name(air, &air->intents[intent_index].step_name, step->name)) {
                air_destroy(air);
                air_set_error(error_message, "AIR intent step name allocation failed");
                return NULL;
            }
            air->intents[intent_index].step_index = step->index;
            air->intents[intent_index].ast = step->ast != NULL ? step->ast : owner_ast;
            air->intents[intent_index].sync_class = sync_class;
            air->intents[intent_index].failure_class = air_failure_from_dir_step(step);

            if (air_step_has_zone_boundary(step)) {
                air->boundaries[boundary_index].kind = AIR_BOUNDARY_ZONE;
                air->boundaries[boundary_index].owner_name = owner;
                if (!air_assign_owned_name(air,
                                           &air->boundaries[boundary_index].source_name,
                                           step->where_type_name)
                    || !air_assign_authority_names(air,
                                                   &air->boundaries[boundary_index],
                                                   step->authorized_by,
                                                   step->authorized_by_count)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR zone boundary allocation failed");
                    return NULL;
                }
                air->boundaries[boundary_index].intent_index = intent_index;
                air->boundaries[boundary_index].step_index = step->index;
                air->boundaries[boundary_index].ast = step->ast != NULL ? step->ast : owner_ast;
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                boundary_index++;
            }
            if (air_step_has_world_boundary(step)) {
                air->boundaries[boundary_index].kind = AIR_BOUNDARY_WORLD;
                air->boundaries[boundary_index].owner_name = owner;
                if (!air_assign_owned_name(air,
                                           &air->boundaries[boundary_index].source_name,
                                           step->transfer_to_alias != NULL
                                               ? step->transfer_to_alias
                                               : step->transfer_from_alias)
                    || !air_assign_authority_names(air,
                                                   &air->boundaries[boundary_index],
                                                   step->authorized_by,
                                                   step->authorized_by_count)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR world boundary allocation failed");
                    return NULL;
                }
                air->boundaries[boundary_index].intent_index = intent_index;
                air->boundaries[boundary_index].step_index = step->index;
                air->boundaries[boundary_index].ast = step->ast != NULL ? step->ast : owner_ast;
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                boundary_index++;
            }
            if (!air_append_step_expr_boundaries(air,
                                                air->boundaries,
                                                &boundary_index,
                                                intent_index,
                                                owner,
                                                step)) {
                air_destroy(air);
                air_set_error(error_message, "AIR boundary synthesis failed for intent step %s", step->name);
                return NULL;
            }
            intent_index++;
        }
    }
    if (intent_index != intent_node_count || boundary_index != boundary_node_count) {
        air_destroy(air);
        air_set_error(error_message,
                      "AIR synthesis count mismatch: intents %zu/%zu boundaries %zu/%zu",
                      intent_index,
                      intent_node_count,
                      boundary_index,
                      boundary_node_count);
        return NULL;
    }
    if (!air_collect_hir_evidence(air, hir, error_message)
        || !air_collect_rir_evidence(air, rir, error_message)) {
        air_destroy(air);
        return NULL;
    }

    if (!air_verify(air, error_message)) {
        air_destroy(air);
        return NULL;
    }
    return air;
}

void
air_destroy(AIRProgram *air)
{
    if (air == NULL)
        return;
    air_clear_drifts(air);
    for (size_t i = 0; i < air->boundary_count; i++)
        free((void *)air->boundaries[i].authority_names);
    for (size_t i = 0; i < air->owned_name_count; i++)
        free(air->owned_names[i]);
    free(air->owned_names);
    free(air->intents);
    free(air->boundaries);
    free(air->evidence_nodes);
    free(air);
}
