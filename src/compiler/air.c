/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR (Abstraction Intent Representation) synthesis owner.
 */

#include "air.h"
#include "air_internal.h"
#include "../common/env_flags.h"

#include <stdint.h>
#include <stdlib.h>

static bool
air_count_add(size_t *count, size_t addend)
{
    if (count == NULL || *count > SIZE_MAX - addend)
        return false;
    *count += addend;
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
    if (name_count > SIZE_MAX / sizeof(char *))
        return false;

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

static ASTNode *
air_step_provenance_ast(const DIRIntentStep *step, ASTNode *owner_ast)
{
    return step != NULL && step->ast != NULL ? step->ast : owner_ast;
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
    if (pgy_env_value_is_false(value))
        return false;
    return true;
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
    air->has_rir_input = rir != NULL;

    size_t intent_node_count = 0;
    size_t boundary_node_count = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        if (!air_count_add(&intent_node_count, dir->intents[i].step_count)) {
            air_destroy(air);
            air_set_error(error_message, "AIR intent count overflow");
            return NULL;
        }
        for (size_t j = 0; j < dir->intents[i].step_count; j++) {
            if (air_step_has_zone_boundary(&dir->intents[i].steps[j])) {
                if (!air_count_add(&boundary_node_count, 1)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR boundary count overflow");
                    return NULL;
                }
            }
            if (air_step_has_world_boundary(&dir->intents[i].steps[j])) {
                if (!air_count_add(&boundary_node_count, 1)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR boundary count overflow");
                    return NULL;
                }
            }
            if (!air_count_add(&boundary_node_count,
                               air_count_step_expr_boundaries(&dir->intents[i].steps[j]))) {
                air_destroy(air);
                air_set_error(error_message, "AIR boundary count overflow");
                return NULL;
            }
        }
    }

    if (intent_node_count > 0) {
        if (intent_node_count > SIZE_MAX / sizeof(AIRIntentNode)) {
            air_destroy(air);
            air_set_error(error_message, "AIR intent allocation overflow");
            return NULL;
        }
        air->intents = (AIRIntentNode *)calloc(intent_node_count, sizeof(AIRIntentNode));
        if (air->intents == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR intent allocation failed");
            return NULL;
        }
    }
    if (boundary_node_count > 0) {
        if (boundary_node_count > SIZE_MAX / sizeof(AIRBoundaryNode)) {
            air_destroy(air);
            air_set_error(error_message, "AIR boundary allocation overflow");
            return NULL;
        }
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
            air->intents[intent_index].ast =
                air_step_provenance_ast(step, owner_ast);
            air->intents[intent_index].sync_class = sync_class;
            air->intents[intent_index].failure_class = air_failure_from_dir_step(step);
            air->intents[intent_index].who_from_intent_default =
                step->who_inherited_from_intent;
            air->intents[intent_index].who_from_on_receiver =
                step->who_derived_from_on_receiver;
            air->intents[intent_index].who_from_single_participant =
                step->who_derived_from_single_participant;
            air->intents[intent_index].requires_from_action =
                step->requires_inherited_from_action;
            air->intents[intent_index].causes_from_action =
                step->causes_inherited_from_action;

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
                air->boundaries[boundary_index].ast =
                    air_step_provenance_ast(step, owner_ast);
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                air->boundaries[boundary_index].source_from_intent_default =
                    step->where_inherited_from_intent;
                air->boundaries[boundary_index].source_from_action =
                    step->where_inherited_from_action;
                air->boundaries[boundary_index].source_from_transfer =
                    step->where_derived_from_transfer;
                air->boundaries[boundary_index].authority_from_zone =
                    step->authorized_by_derived_from_zone;
                air->boundaries[boundary_index].authority_from_action =
                    step->authorized_by_inherited_from_action;
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
                air->boundaries[boundary_index].ast =
                    air_step_provenance_ast(step, owner_ast);
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                air->boundaries[boundary_index].source_from_transfer = true;
                air->boundaries[boundary_index].authority_from_zone =
                    step->authorized_by_derived_from_zone;
                air->boundaries[boundary_index].authority_from_action =
                    step->authorized_by_inherited_from_action;
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
        || !air_collect_rir_evidence(air, rir, error_message)
        || !air_collect_observability_schema_evidence(air, error_message)
        || !air_collect_runtime_frontier_policy_evidence(air, error_message)) {
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
