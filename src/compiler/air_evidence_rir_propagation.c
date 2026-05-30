/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR effect/relation propagation evidence owner.
 */

#include <stdint.h>

#include "air_internal.h"

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

    if (!air_increment_evidence_required_count(air, evidence_kind)) {
        air_set_error(error_message,
                      "AIR RIR propagation required counter overflow");
        return false;
    }

    if (!air_rir_scope_has_propagation_state(scope, op, resource_kind))
        return true;
    if (!air_require_rir_scope_provider(scope, error_message))
        return false;

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
