/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR invariant validation owner. This TU owns AIR graph shape,
 * evidence shape, and provenance invariants before drift verification.
 */

#include "air_internal.h"
#include "air_validate_machine_layer.h"

#include "../runtime/pgy_runtime_capability.h"
#include "../semantic/capability_analyze.h"
#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void
air_set_invariant_error(char **error_message, const char *fmt, ...)
{
    va_list args;
    char *detail;

    va_start(args, fmt);
    detail = air_vformat_owned(fmt, args);
    va_end(args);
    if (detail == NULL) {
        air_set_error(error_message,
                      PGY_CODE_AIR_INVARIANT_INVALID
                      ": AIR invariant detail formatting failed");
        return;
    }
    air_set_error(error_message,
                  PGY_CODE_AIR_INVARIANT_INVALID ": %s",
                  detail);
    free(detail);
}

bool
air_name_is_empty(const char *name)
{
    return name == NULL || name[0] == '\0';
}

static bool
air_boundary_sync_shape_valid(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_WORLD:
        return boundary->sync_class == AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_CHANNEL:
        return boundary->sync_class == AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_IO:
        return boundary->sync_class == AIR_SYNC_EITHER;
    case AIR_BOUNDARY_EXECUTION:
        return boundary->sync_class == AIR_SYNC_SYNC;
    case AIR_BOUNDARY_ZONE:
        return boundary->sync_class == AIR_SYNC_SYNC
            || boundary->sync_class == AIR_SYNC_ASYNC
            || boundary->sync_class == AIR_SYNC_EITHER;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return false;
    }
}

static bool
air_drift_kind_valid(AIRDriftKind kind)
{
    switch (kind) {
    case AIR_DRIFT_SYNC_ASYNC_CONFLICT:
    case AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING:
    case AIR_DRIFT_EFFECT_PROPAGATION_MISSING:
    case AIR_DRIFT_RELATION_PROPAGATION_MISSING:
    case AIR_DRIFT_DAG_DEAD_END_PRESENT:
        return true;
    case AIR_DRIFT_NONE:
    default:
        return false;
    }
}

static bool
air_drift_kind_is_global(AIRDriftKind kind)
{
    return kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
        || kind == AIR_DRIFT_EFFECT_PROPAGATION_MISSING
        || kind == AIR_DRIFT_RELATION_PROPAGATION_MISSING
        || kind == AIR_DRIFT_DAG_DEAD_END_PRESENT;
}

static bool
air_validate_slot_site_inventory(const AIRProgram *air,
                                 char **error_message)
{
    if (air->slot_site_count > 0 && air->slot_sites == NULL) {
        air_set_invariant_error(error_message,
                                "AIR has slot site count without slot site array");
        return false;
    }
    for (size_t i = 0; i < air_slot_site_count(air); i++) {
        const AIRSlotSite *site = air_slot_site_at(air, i);
        if (site == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR slot site %zu is missing",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->slot)) {
            air_set_invariant_error(error_message,
                                    "AIR slot site %zu has empty slot",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->op)) {
            air_set_invariant_error(error_message,
                                    "AIR slot site %zu has empty op",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->routine)) {
            air_set_invariant_error(error_message,
                                    "AIR slot site %zu has empty routine",
                                    i);
            return false;
        }
    }
    return true;
}

static bool
air_validate_effect_site_inventory(const AIRProgram *air,
                                   char **error_message)
{
    const uint32_t program_capabilities = air_program_capabilities(air);

    if (air->effect_site_count > 0 && air->effect_sites == NULL) {
        air_set_invariant_error(error_message,
                                "AIR has effect site count without effect site array");
        return false;
    }
    for (size_t i = 0; i < air_effect_site_count(air); i++) {
        const AIREffectSite *site = air_effect_site_at(air, i);
        const char *capability_name;

        if (site == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu is missing",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->op)) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu has empty op",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->effect)) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu has empty effect",
                                    i);
            return false;
        }
        if (air_name_is_empty(site->routine)) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu has empty routine",
                                    i);
            return false;
        }
        if (site->cap == PGY_CAP_NONE || (site->cap & (site->cap - 1u)) != 0) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu has invalid capability mask 0x%x",
                                    i,
                                    (unsigned)site->cap);
            return false;
        }
        capability_name = capability_bit_name(site->cap);
        if (capability_name == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu has unknown capability mask 0x%x",
                                    i,
                                    (unsigned)site->cap);
            return false;
        }
        if (!air_name_matches(site->effect, capability_name)) {
            air_set_invariant_error(error_message,
                                    "AIR effect site %zu name %s does not match capability %s",
                                    i,
                                    site->effect,
                                    capability_name);
            return false;
        }
        if ((program_capabilities & site->cap) != site->cap) {
            air_set_invariant_error(
                error_message,
                "AIR effect site %zu capability %s is missing from program capability mask",
                i,
                capability_name);
            return false;
        }
    }
    return true;
}

static bool
air_validate_lifecycle_state_space_inventory(const AIRProgram *air,
                                             char **error_message)
{
    if (air->lifecycle_state_space_count > 0
        && air->lifecycle_state_spaces == NULL) {
        air_set_invariant_error(
            error_message,
            "AIR has lifecycle state-space count without lifecycle array");
        return false;
    }
    for (size_t i = 0; i < air_lifecycle_state_space_count(air); i++) {
        const AIRLifecycleStateSpace *space =
            air_lifecycle_state_space_at(air, i);
        if (space == NULL) {
            air_set_invariant_error(
                error_message,
                "AIR lifecycle state-space %zu is missing",
                i);
            return false;
        }
        if (air_name_is_empty(space->subject)) {
            air_set_invariant_error(
                error_message,
                "AIR lifecycle state-space %zu has empty subject",
                i);
            return false;
        }
        if (space->state_count == 0
            || space->state_count > AIR_LIFECYCLE_MAX_STATES) {
            air_set_invariant_error(
                error_message,
                "AIR lifecycle state-space %zu has invalid state count",
                i);
            return false;
        }
        if (space->op_count == 0
            || space->op_count > AIR_LIFECYCLE_MAX_OPS) {
            air_set_invariant_error(
                error_message,
                "AIR lifecycle state-space %zu has invalid op count",
                i);
            return false;
        }
        for (size_t s = 0; s < space->state_count; s++) {
            if (air_name_is_empty(space->states[s])) {
                air_set_invariant_error(
                    error_message,
                    "AIR lifecycle state-space %zu has empty state %zu",
                    i,
                    s);
                return false;
            }
        }
        for (size_t o = 0; o < space->op_count; o++) {
            if (air_name_is_empty(space->ops[o].name)) {
                air_set_invariant_error(
                    error_message,
                    "AIR lifecycle state-space %zu has empty op %zu",
                    i,
                    o);
                return false;
            }
        }
    }
    return true;
}

static bool
air_validate_function_param_flow_summary_inventory(
    const AIRProgram *air,
    char **error_message)
{
    const size_t count = air_function_param_flow_summary_count(air);

    if (count > 0 && air->function_param_flow_summaries == NULL) {
        air_set_invariant_error(
            error_message,
            "AIR has function parameter flow summary count without array");
        return false;
    }
    if (air->has_function_param_flow_facts != (count > 0)) {
        air_set_invariant_error(
            error_message,
            "AIR function parameter flow presence flag does not match rows");
        return false;
    }
    if (count > 0 && !air_has_mir_input(air)) {
        air_set_invariant_error(
            error_message,
            "AIR function parameter flow rows require MIR input");
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const AIRFunctionParamFlowSummary *row =
            air_function_param_flow_summary_at(air, i);
        if (row == NULL || row->source_syntax_id == 0
            || air_name_is_empty(row->routine)
            || row->parameter_count == 0
            || row->parameter_index >= row->parameter_count) {
            air_set_invariant_error(
                error_message,
                "AIR function parameter flow row %zu is incomplete",
                i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const AIRFunctionParamFlowSummary *prior =
                air_function_param_flow_summary_at(air, j);
            if (prior != NULL
                && prior->source_syntax_id == row->source_syntax_id
                && prior->parameter_index == row->parameter_index) {
                air_set_invariant_error(
                    error_message,
                    "AIR function parameter flow row %zu duplicates stable identity row %zu",
                    i,
                    j);
                return false;
            }
            if (prior != NULL
                && prior->source_syntax_id == row->source_syntax_id
                && (!air_name_matches(prior->routine, row->routine)
                    || prior->parameter_count != row->parameter_count)) {
                air_set_invariant_error(
                    error_message,
                    "AIR function parameter flow rows disagree on routine identity");
                return false;
            }
        }
    }
    return true;
}

static bool
air_validate_capability_machine(const AIRProgram *air,
                                char **error_message)
{
    return air_validate_slot_site_inventory(air, error_message)
        && air_validate_effect_site_inventory(air, error_message)
        && air_validate_machine_layer_site_inventory(air, error_message)
        && air_validate_lifecycle_state_space_inventory(air, error_message)
        && air_validate_function_param_flow_summary_inventory(air,
                                                              error_message);
}

bool
air_validate(const AIRProgram *air, char **error_message)
{
    if (air == NULL) {
        air_set_invariant_error(error_message, "AIR validation requires a program");
        return false;
    }
    if (air->mir_evidence_bound
        && (!air->mir_evidence_collection_started
            || !air_has_mir_input(air)
            || air->mir_evidence_binding_fingerprint == 0)) {
        air_set_invariant_error(
            error_message,
            "AIR MIR evidence binding is incomplete");
        return false;
    }
    if (air->mir_evidence_collection_started && !air_has_mir_input(air)) {
        air_set_invariant_error(
            error_message,
            "AIR MIR evidence collection started without MIR input");
        return false;
    }
    if (!air_intent_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has intent count without intent array");
        return false;
    }
    if (!air_boundary_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has boundary count without boundary array");
        return false;
    }
    if (!air_drift_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has drift count without drift array");
        return false;
    }
    if (!air_evidence_inventory_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has evidence count without evidence array");
        return false;
    }
    if (!air_propagation_requirement_storage_valid(air)) {
        air_set_invariant_error(
            error_message,
            "AIR has propagation requirement count without requirement array");
        return false;
    }
    if (!air_validate_capability_machine(air, error_message))
        return false;
    for (size_t i = 0; i < air_intent_node_count(air); i++) {
        const AIRIntentNode *intent = air_intent_node_at(air, i);
        if (intent == NULL) {
            air_set_invariant_error(error_message, "AIR intent node %zu is missing", i);
            return false;
        }
        if (air_name_is_empty(intent->intent_owner)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no owner name", i);
            return false;
        }
        if (air_name_is_empty(intent->step_name)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no step name", i);
            return false;
        }
        if (intent->sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown sync class", i);
            return false;
        }
        if (intent->failure_class == AIR_FAILURE_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown failure class", i);
            return false;
        }
        if (air_intent_compression_budget(air, i) == AIR_COMPRESSION_UNKNOWN) {
            air_set_invariant_error(error_message,
                                    "AIR intent node %zu has unknown compression budget",
                                    i);
            return false;
        }
    }
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        const AIRIntentNode *intent = NULL;
        bool has_intent_binding;
        if (boundary == NULL) {
            air_set_invariant_error(error_message, "AIR boundary node %zu is missing", i);
            return false;
        }
        if (boundary->kind == AIR_BOUNDARY_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown kind", i);
            return false;
        }
        has_intent_binding = boundary->intent_index != SIZE_MAX;
        if (has_intent_binding
            && boundary->intent_index >= air_intent_node_count(air)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu references missing intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (has_intent_binding)
            intent = air_intent_node_at(air, boundary->intent_index);
        if (has_intent_binding && intent == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu references missing intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (has_intent_binding && boundary->step_index != intent->step_index) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu step index does not match intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (air_name_is_empty(boundary->owner_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no owner name", i);
            return false;
        }
        if (has_intent_binding
            && !air_name_matches(boundary->owner_name, intent->intent_owner)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu owner does not match intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (air_name_is_empty(boundary->source_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no source name", i);
            return false;
        }
        if (boundary->sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown sync class", i);
            return false;
        }
        if (!air_boundary_sync_shape_valid(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has invalid sync class %s for %s boundary",
                                    i,
                                    air_sync_class_name(boundary->sync_class),
                                    air_boundary_kind_name(boundary->kind));
            return false;
        }
        if (air_boundary_compression_budget(boundary)
            == AIR_COMPRESSION_UNKNOWN) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has unknown compression budget",
                                    i);
            return false;
        }
        if (boundary->kind == AIR_BOUNDARY_WORLD
            && !boundary->source_from_transfer) {
            air_set_invariant_error(error_message,
                                    "AIR world boundary node %zu has no transfer provenance",
                                    i);
            return false;
        }
        if (boundary->source_from_action
            && boundary->kind != AIR_BOUNDARY_ZONE) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has action-inherited source provenance on non-zone boundary",
                                    i);
            return false;
        }
        if (boundary->source_from_transfer
            && boundary->kind != AIR_BOUNDARY_ZONE
            && boundary->kind != AIR_BOUNDARY_WORLD) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has transfer source provenance on non-zone/world boundary",
                                    i);
            return false;
        }
        if (!air_boundary_authority_storage_valid(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has authority count without names",
                                    i);
            return false;
        }
        if (!air_boundary_required_ability_storage_valid(boundary)) {
            air_set_invariant_error(
                error_message,
                "AIR boundary node %zu has required ability count without abilities",
                i);
            return false;
        }
        for (size_t j = 0;
             j < air_boundary_authority_name_count(boundary);
             j++) {
            const char *authority_name =
                air_boundary_authority_name_at(boundary, j);
            if (air_name_is_empty(authority_name)) {
                air_set_invariant_error(error_message,
                                        "AIR boundary node %zu has empty authority name %zu",
                                        i,
                                        j);
                return false;
            }
        }
        for (size_t j = 0;
             j < air_boundary_required_ability_count(boundary);
             j++) {
            const char *required_ability =
                air_boundary_required_ability_at(boundary, j);
            if (air_name_is_empty(required_ability)) {
                air_set_invariant_error(
                    error_message,
                    "AIR boundary node %zu has empty required ability %zu",
                    i,
                    j);
                return false;
            }
        }
        if (boundary->authority_required
            && air_boundary_authority_name_count(boundary) == 0) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu requires authority but has no participant",
                                    i);
            return false;
        }
        if (!boundary->authority_required
            && air_boundary_authority_name_count(boundary) > 0) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has authority participants but does not require authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_zone && !boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has legacy zone-authority field without authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_action && !boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has action-inherited authority without authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_zone
            && boundary->kind != AIR_BOUNDARY_ZONE
            && boundary->kind != AIR_BOUNDARY_WORLD) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has legacy zone-authority field on non-zone/world boundary",
                                    i);
            return false;
        }
        if (!air_validate_boundary_summary_shape(air, i, error_message))
            return false;
    }
    for (size_t i = 0; i < air_drift_count(air); i++) {
        const AIRDrift *drift = air_drift_at(air, i);
        if (drift == NULL) {
            air_set_invariant_error(error_message, "AIR drift node %zu is missing", i);
            return false;
        }
        if (!air_drift_kind_valid(drift->kind)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has invalid kind", i);
            return false;
        }
        if (drift->intent_index >= air_intent_node_count(air)
            && !(drift->intent_index == SIZE_MAX
                 && (air_drift_kind_is_global(drift->kind)
                     || drift->boundary_index < air_boundary_node_count(air)))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing intent node %zu",
                                    i,
                                    drift->intent_index);
            return false;
        }
        if (drift->boundary_index >= air_boundary_node_count(air)
            && !(drift->boundary_index == SIZE_MAX
                 && air_drift_kind_is_global(drift->kind))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing boundary node %zu",
                                    i,
                                    drift->boundary_index);
            return false;
        }
        if (air_name_is_empty(drift->message)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has no message", i);
            return false;
        }
    }
    if (!air_validate_evidence_inventory(air, error_message))
        return false;
    for (size_t i = 0; i < air_propagation_requirement_count(air); i++) {
        const AIRPropagationRequirement *requirement =
            air_propagation_requirement_at(air, i);
        if (requirement == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR propagation requirement %zu is missing",
                                    i);
            return false;
        }
        if (requirement->kind != AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
            && requirement->kind != AIR_EVIDENCE_RIR_RELATION_PROPAGATION) {
            air_set_invariant_error(
                error_message,
                "AIR propagation requirement %zu has invalid evidence kind",
                i);
            return false;
        }
        if (air_name_is_empty(requirement->provider_name)
            || air_name_is_empty(requirement->subject_name)) {
            air_set_invariant_error(
                error_message,
                "AIR propagation requirement %zu has empty provenance",
                i);
            return false;
        }
    }
    return true;
}
