/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR effect/relation propagation evidence owner.
 */

#include <stdint.h>
#include <stdlib.h>

#include "air_internal.h"

static bool
air_rir_propagation_kind_is_valid(AIREvidenceKind kind)
{
    return kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        || kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION;
}

bool
air_propagation_requirement_storage_valid(const AIRProgram *air)
{
    return air != NULL
        && (air->propagation_requirement_count == 0
            || air->propagation_requirements != NULL);
}

size_t
air_propagation_requirement_count(const AIRProgram *air)
{
    return air != NULL ? air->propagation_requirement_count : 0;
}

const AIRPropagationRequirement *
air_propagation_requirement_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->propagation_requirement_count)
        return NULL;
    return &air->propagation_requirements[index];
}

size_t
air_propagation_requirement_key_count(const AIRProgram *air,
                                      AIREvidenceKind kind,
                                      const char *provider_name,
                                      const char *subject_name)
{
    size_t count = 0;

    if (air == NULL
        || !air_rir_propagation_kind_is_valid(kind)
        || air_name_is_empty(provider_name)
        || air_name_is_empty(subject_name)) {
        return 0;
    }
    for (size_t i = 0; i < air_propagation_requirement_count(air); i++) {
        const AIRPropagationRequirement *requirement =
            air_propagation_requirement_at(air, i);
        if (requirement != NULL
            && requirement->kind == kind
            && air_name_matches(requirement->provider_name, provider_name)
            && air_name_matches(requirement->subject_name, subject_name)) {
            count++;
        }
    }
    return count;
}

bool
air_append_rir_propagation_requirement(AIRProgram *air,
                                       AIREvidenceKind kind,
                                       const char *provider_name,
                                       const char *subject_name,
                                       char **error_message)
{
    AIRPropagationRequirement *requirement;
    const char *owned_provider_name = NULL;
    const char *owned_subject_name = NULL;
    size_t owned_name_checkpoint;

    if (air == NULL) {
        air_set_error(error_message,
                      "AIR RIR propagation requirement requires a program");
        return false;
    }
    if (!air_rir_propagation_kind_is_valid(kind)) {
        air_set_error(error_message,
                      "AIR RIR propagation requirement has invalid evidence kind");
        return false;
    }
    if (air_name_is_empty(provider_name) || air_name_is_empty(subject_name)) {
        air_set_error(error_message,
                      "AIR RIR propagation requirement requires provider and subject provenance");
        return false;
    }
    if (air->propagation_requirement_count
        >= air->propagation_requirement_capacity) {
        AIRPropagationRequirement *next;
        size_t new_capacity = air->propagation_requirement_capacity;
        if (!air_next_capacity(&new_capacity,
                               16,
                               sizeof(AIRPropagationRequirement))) {
            air_set_error(error_message,
                          "AIR RIR propagation requirement allocation failed");
            return false;
        }
        next = (AIRPropagationRequirement *)realloc(
            air->propagation_requirements,
            new_capacity * sizeof(AIRPropagationRequirement));
        if (next == NULL) {
            air_set_error(error_message,
                          "AIR RIR propagation requirement allocation failed");
            return false;
        }
        air->propagation_requirements = next;
        air->propagation_requirement_capacity = new_capacity;
    }

    owned_name_checkpoint = air->owned_name_count;
    if (!air_assign_owned_name(air, &owned_provider_name, provider_name)
        || !air_assign_owned_name(air, &owned_subject_name, subject_name)) {
        for (size_t i = owned_name_checkpoint; i < air->owned_name_count; i++) {
            free(air->owned_names[i]);
            air->owned_names[i] = NULL;
        }
        air->owned_name_count = owned_name_checkpoint;
        air_set_error(error_message,
                      "AIR RIR propagation requirement provenance allocation failed");
        return false;
    }

    requirement =
        &air->propagation_requirements[air->propagation_requirement_count++];
    requirement->kind = kind;
    requirement->provider_name = owned_provider_name;
    requirement->subject_name = owned_subject_name;
    return true;
}

static bool
air_rir_name_or_anchor_matches(const char *name,
                               const char *slot_anchor,
                               const char *needle)
{
    return air_name_matches(name, needle)
        || air_name_matches(slot_anchor, needle);
}

static bool
air_rir_scope_has_propagation_state(const RIRScope *scope,
                                    const RIROp *op,
                                    RIRResourceKind resource_kind)
{
    if (scope == NULL || op == NULL)
        return false;
    for (size_t i = 0; i < rir_scope_state_summary_count(scope); i++) {
        const RIRStateSummary *summary =
            rir_scope_state_summary_at(scope, i);
        if (summary != NULL
            && summary->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(summary->name,
                                              summary->slot_anchor,
                                              op->subject)) {
            return true;
        }
    }
    for (size_t i = 0; i < rir_scope_fact_count(scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(scope, i);
        if (fact != NULL
            && fact->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(fact->name,
                                              fact->slot_anchor,
                                              op->subject)) {
            return true;
        }
    }
    return false;
}

static bool
air_collect_rir_propagation_evidence(AIRProgram *air,
                                     const RIRScope *scope,
                                     const RIROp *op,
                                     const char *scope_name,
                                     char **error_message)
{
    bool effect_op;
    bool relation_op;
    RIRResourceKind resource_kind;
    AIREvidenceKind evidence_kind;

    if (op == NULL)
        return true;

    effect_op = op->kind == RIR_OP_ATTACH_EFFECT
        || op->kind == RIR_OP_DETACH_EFFECT;
    relation_op = op->kind == RIR_OP_LINK_RELATION
        || op->kind == RIR_OP_UNLINK_RELATION;

    if (!effect_op && !relation_op)
        return true;

    resource_kind = effect_op
        ? RIR_RESOURCE_EFFECT_INSTANCE
        : RIR_RESOURCE_RELATION_INSTANCE;
    evidence_kind = effect_op
        ? AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        : AIR_EVIDENCE_RIR_RELATION_PROPAGATION;

    if (!air_require_rir_scope_provider(scope, error_message))
        return false;
    if (!air_append_rir_propagation_requirement(air,
                                                evidence_kind,
                                                scope_name,
                                                op->subject,
                                                error_message)) {
        return false;
    }
    if (!air_increment_evidence_required_count(air, evidence_kind)) {
        air_set_error(error_message,
                      "AIR RIR propagation required counter overflow");
        return false;
    }

    if (!air_rir_scope_has_propagation_state(scope, op, resource_kind))
        return true;

    if (!air_append_evidence_node(air,
                                  evidence_kind,
                                  SIZE_MAX,
                                  scope_name,
                                  op->subject,
                                  error_message)) {
        return false;
    }
    if (!air_increment_evidence_summary_count(air, evidence_kind)) {
        air_set_error(error_message,
                      "AIR RIR propagation evidence counter overflow");
        return false;
    }
    return true;
}

bool
air_collect_rir_scope_propagation_evidence(AIRProgram *air,
                                           const RIRScope *scope,
                                           char **error_message)
{
    const char *scope_name;

    if (air == NULL || scope == NULL)
        return true;
    scope_name = air_rir_scope_provider_name(scope);
    for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
        const RIROp *op = rir_scope_op_at(scope, i);
        if (!air_collect_rir_propagation_evidence(air,
                                                  scope,
                                                  op,
                                                  scope_name,
                                                  error_message)) {
            return false;
        }
    }
    return true;
}
