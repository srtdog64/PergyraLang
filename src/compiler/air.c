/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR (Abstraction Intent Representation) synthesis and drift checks.
 */

#include "air.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
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

static void
air_set_error(char **error_message, const char *fmt, ...)
{
    if (error_message == NULL)
        return;
    va_list args;
    va_start(args, fmt);
    *error_message = air_vformat(fmt, args);
    va_end(args);
}

static char *
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

static void
air_clear_drifts(AIRProgram *air)
{
    if (air == NULL)
        return;
    for (size_t i = 0; i < air->drift_count; i++)
        free((char *)air->drifts[i].message);
    free(air->drifts);
    air->drifts = NULL;
    air->drift_count = 0;
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

static AIRBoundaryKind
air_boundary_from_dir_step(const DIRIntentStep *step)
{
    if (step == NULL)
        return AIR_BOUNDARY_UNKNOWN;
    if (step->where_type_name != NULL)
        return AIR_BOUNDARY_ZONE;
    if (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL)
        return AIR_BOUNDARY_WORLD;
    return AIR_BOUNDARY_UNKNOWN;
}

static bool
air_sync_conflicts(AIRSyncClass expected, AIRSyncClass actual)
{
    if (expected == AIR_SYNC_UNKNOWN || actual == AIR_SYNC_UNKNOWN)
        return false;
    if (expected == AIR_SYNC_EITHER || actual == AIR_SYNC_EITHER)
        return false;
    return expected != actual;
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

static bool
air_append_drift(AIRProgram *air,
                 AIRDriftKind kind,
                 size_t intent_index,
                 size_t boundary_index,
                 const char *message,
                 char **error_message)
{
    char *message_copy = air_strdup_owned(message);
    AIRDrift *next;

    if (message_copy == NULL) {
        air_set_error(error_message, "AIR drift message allocation failed");
        return false;
    }
    next = (AIRDrift *)realloc(air->drifts, sizeof(AIRDrift) * (air->drift_count + 1));
    if (next == NULL) {
        free(message_copy);
        air_set_error(error_message, "AIR drift allocation failed");
        return false;
    }
    air->drifts = next;
    air->drifts[air->drift_count].kind = kind;
    air->drifts[air->drift_count].intent_index = intent_index;
    air->drifts[air->drift_count].boundary_index = boundary_index;
    air->drifts[air->drift_count].message = message_copy;
    air->drift_count++;
    return true;
}

static bool
air_name_matches(const char *a, const char *b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static bool
air_boundary_authority_matches(const AIRBoundaryNode *boundary, const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        if (air_name_matches(boundary->authority_names[i], authority_name))
            return true;
    }
    return false;
}

static bool
air_format_authority_names(const AIRBoundaryNode *boundary,
                           char *out,
                           size_t out_size)
{
    size_t used = 0;
    bool emitted = false;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (boundary == NULL || boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        const char *name = boundary->authority_names[i];
        int written;

        if (name == NULL || name[0] == '\0')
            continue;
        written = snprintf(out + used,
                           out_size - used,
                           "%s%s",
                           emitted ? ", " : "",
                           name);
        if (written < 0)
            return emitted;
        if ((size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return true;
        }
        used += (size_t)written;
        emitted = true;
    }
    return emitted;
}

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || intent == NULL || boundary == NULL)
        return false;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return true;
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, boundary->source_name);
}

static void
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir)
{
    if (air == NULL || hir == NULL)
        return;
    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            const AIRIntentNode *intent = &air->intents[boundary->intent_index];
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                boundary->has_hir_routine_evidence = true;
                air->hir_routine_evidence_count++;
            }
        }
    }
}

static bool
air_rir_scope_matches_boundary(const RIRScope *scope, const AIRBoundaryNode *boundary)
{
    if (scope == NULL || boundary == NULL)
        return false;
    if (!(scope->kind == RIR_SCOPE_INTENT
          || scope->kind == RIR_SCOPE_ZONE
          || scope->kind == RIR_SCOPE_WORLD)) {
        return false;
    }
    return air_name_matches(scope->name, boundary->source_name)
        || air_name_matches(scope->owner_name, boundary->owner_name)
        || air_name_matches(scope->owner_name, boundary->source_name);
}

static void
air_collect_rir_evidence(AIRProgram *air, const RIRProgram *rir)
{
    if (air == NULL || rir == NULL)
        return;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        for (size_t j = 0; j < scope->fact_count; j++) {
            if (scope->facts[j].kind == RIR_FACT_AUTHORITY) {
                air->rir_authority_evidence_count++;
            }
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            if (scope->ops[j].kind == RIR_OP_AUTHORIZE) {
                air->rir_authority_evidence_count++;
            }
        }
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            if (!air_rir_scope_matches_boundary(scope, boundary))
                continue;
            boundary->has_rir_boundary_evidence = true;
            air->rir_boundary_evidence_count++;
            for (size_t k = 0; k < scope->fact_count; k++) {
                if (scope->facts[k].kind == RIR_FACT_AUTHORITY
                    && air_boundary_authority_matches(boundary, scope->facts[k].name)) {
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
            for (size_t k = 0; !boundary->has_rir_authority_evidence && k < scope->op_count; k++) {
                if (scope->ops[k].kind == RIR_OP_AUTHORIZE
                    && air_boundary_authority_matches(boundary, scope->ops[k].subject)) {
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
        }
    }
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

    size_t intent_node_count = 0;
    size_t boundary_node_count = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        intent_node_count += dir->intents[i].step_count;
        for (size_t j = 0; j < dir->intents[i].step_count; j++) {
            if (air_boundary_from_dir_step(&dir->intents[i].steps[j]) != AIR_BOUNDARY_UNKNOWN)
                boundary_node_count++;
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

    size_t intent_index = 0;
    size_t boundary_index = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        const char *owner = air_dir_node_name(dir, info->node_id);
        ASTNode *owner_ast = air_dir_node_ast(dir, info->node_id);
        for (size_t j = 0; j < info->step_count; j++) {
            const DIRIntentStep *step = &info->steps[j];
            AIRSyncClass sync_class = air_sync_from_dir_step(step);
            air->intents[intent_index].intent_owner = owner;
            air->intents[intent_index].step_name = step->name;
            air->intents[intent_index].step_index = step->index;
            air->intents[intent_index].ast = step->ast != NULL ? step->ast : owner_ast;
            air->intents[intent_index].sync_class = sync_class;
            air->intents[intent_index].failure_class = air_failure_from_dir_step(step);

            AIRBoundaryKind boundary_kind = air_boundary_from_dir_step(step);
            if (boundary_kind != AIR_BOUNDARY_UNKNOWN) {
                air->boundaries[boundary_index].kind = boundary_kind;
                air->boundaries[boundary_index].owner_name = owner;
                air->boundaries[boundary_index].source_name = step->where_type_name != NULL
                    ? step->where_type_name
                    : step->using_alias;
                air->boundaries[boundary_index].intent_index = intent_index;
                air->boundaries[boundary_index].step_index = step->index;
                air->boundaries[boundary_index].ast = step->ast != NULL ? step->ast : owner_ast;
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                air->boundaries[boundary_index].authority_names = step->authorized_by;
                air->boundaries[boundary_index].authority_name_count = step->authorized_by_count;
                boundary_index++;
            }
            intent_index++;
        }
    }
    air->intent_count = intent_node_count;
    air->boundary_count = boundary_node_count;
    air_collect_hir_evidence(air, hir);
    air_collect_rir_evidence(air, rir);

    if (!air_validate(air, error_message)) {
        air_destroy(air);
        return NULL;
    }
    if (!air_check_drift(air, error_message)) {
        air_destroy(air);
        return NULL;
    }
    return air;
}

bool
air_validate(const AIRProgram *air, char **error_message)
{
    if (air == NULL) {
        air_set_error(error_message, "AIR validation requires a program");
        return false;
    }
    for (size_t i = 0; i < air->intent_count; i++) {
        if (air->intents[i].step_name == NULL) {
            air_set_error(error_message, "AIR intent node %zu has no step name", i);
            return false;
        }
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        if (air->boundaries[i].kind == AIR_BOUNDARY_UNKNOWN) {
            air_set_error(error_message, "AIR boundary node %zu has unknown kind", i);
            return false;
        }
        if (air->boundaries[i].intent_index >= air->intent_count) {
            air_set_error(error_message,
                          "AIR boundary node %zu references missing intent node %zu",
                          i,
                          air->boundaries[i].intent_index);
            return false;
        }
    }
    return true;
}

bool
air_check_drift(AIRProgram *air, char **error_message)
{
    if (!air_validate(air, error_message))
        return false;

    air_clear_drifts(air);

    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];
        AIRIntentNode *intent = &air->intents[boundary->intent_index];
        if (air_sync_conflicts(intent->sync_class, boundary->sync_class)) {
            if (!air_append_drift(air,
                                  AIR_DRIFT_SYNC_ASYNC_CONFLICT,
                                  boundary->intent_index,
                                  i,
                                  PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT
                                  ": intent sync class conflicts with boundary implementation sync class",
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence && !boundary->has_rir_boundary_evidence) {
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                                  ": AIR boundary has no matching RIR boundary evidence",
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && boundary->authority_required
            && !boundary->has_rir_authority_evidence) {
            char authority_names[256];
            char message[512];
            const char *drift_message =
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR authority boundary has no matching RIR authority evidence";

            if (air_format_authority_names(boundary,
                                           authority_names,
                                           sizeof(authority_names))) {
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR authority boundary has no matching RIR authority evidence; expected authority participant(s): %s",
                         authority_names);
                drift_message = message;
            }
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  drift_message,
                                  error_message)) {
                return false;
            }
        }
    }
    return true;
}

void
air_destroy(AIRProgram *air)
{
    if (air == NULL)
        return;
    air_clear_drifts(air);
    free(air->intents);
    free(air->boundaries);
    free(air);
}

void
air_dump(const AIRProgram *air, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (air == NULL) {
        fprintf(out, "AIRProgram(null)\n");
        return;
    }
    fprintf(out, "AIRProgram intents=%zu boundaries=%zu drifts=%zu strict_evidence=%s\n",
            air->intent_count,
            air->boundary_count,
            air->drift_count,
            air->strict_evidence ? "yes" : "no");
    fprintf(out, "  evidence hir_routines=%zu rir_boundaries=%zu rir_authority=%zu\n",
            air->hir_routine_evidence_count,
            air->rir_boundary_evidence_count,
            air->rir_authority_evidence_count);
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        fprintf(out,
                "  intent[%zu] owner=%s step=%s index=%zu sync=%s failure=%s\n",
                i,
                intent->intent_owner != NULL ? intent->intent_owner : "<anonymous>",
                intent->step_name != NULL ? intent->step_name : "<unnamed>",
                intent->step_index,
                air_sync_class_name(intent->sync_class),
                air_failure_class_name(intent->failure_class));
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        fprintf(out,
                "  boundary[%zu] kind=%s owner=%s source=%s intent=%zu step=%zu sync=%s authority=%s\n",
                i,
                air_boundary_kind_name(boundary->kind),
                boundary->owner_name != NULL ? boundary->owner_name : "<anonymous>",
                boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                boundary->intent_index,
                boundary->step_index,
                air_sync_class_name(boundary->sync_class),
                boundary->authority_required ? "yes" : "no");
        fprintf(out,
                "    evidence hir=%s rir_boundary=%s rir_authority=%s\n",
                boundary->has_hir_routine_evidence ? "yes" : "no",
                boundary->has_rir_boundary_evidence ? "yes" : "no",
                boundary->has_rir_authority_evidence ? "yes" : "no");
    }
}

const char *
air_sync_class_name(AIRSyncClass sync_class)
{
    switch (sync_class) {
    case AIR_SYNC_UNKNOWN: return "unknown";
    case AIR_SYNC_SYNC: return "sync";
    case AIR_SYNC_ASYNC: return "async";
    case AIR_SYNC_EITHER: return "either";
    }
    return "invalid";
}

const char *
air_failure_class_name(AIRFailureClass failure_class)
{
    switch (failure_class) {
    case AIR_FAILURE_UNKNOWN: return "unknown";
    case AIR_FAILURE_RECOVERABLE: return "recoverable";
    case AIR_FAILURE_FATAL: return "fatal";
    case AIR_FAILURE_COMPENSABLE: return "compensable";
    }
    return "invalid";
}

const char *
air_boundary_kind_name(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_UNKNOWN: return "unknown";
    case AIR_BOUNDARY_ZONE: return "zone";
    case AIR_BOUNDARY_WORLD: return "world";
    case AIR_BOUNDARY_PARALLEL: return "parallel";
    case AIR_BOUNDARY_IO: return "io";
    case AIR_BOUNDARY_CHANNEL: return "channel";
    }
    return "invalid";
}

const char *
air_drift_kind_name(AIRDriftKind kind)
{
    switch (kind) {
    case AIR_DRIFT_NONE: return "none";
    case AIR_DRIFT_SYNC_ASYNC_CONFLICT: return "sync_async_conflict";
    case AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING: return "boundary_evidence_missing";
    }
    return "invalid";
}
