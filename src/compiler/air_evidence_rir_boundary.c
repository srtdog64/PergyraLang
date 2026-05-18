/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR boundary and authority evidence owner.
 */

#include "air_internal.h"

static bool
air_boundary_authority_matches(const AIRBoundaryNode *boundary,
                               const char *authority_name)
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
air_append_rir_boundary_evidence(AIRProgram *air,
                                 AIRBoundaryNode *boundary,
                                 size_t boundary_index,
                                 const char *scope_name,
                                 char **error_message)
{
    if (boundary->has_rir_boundary_evidence)
        return true;
    if (!air_assign_first_owned_name(air,
                                     &boundary->rir_boundary_evidence_scope,
                                     scope_name,
                                     error_message,
                                     "RIR boundary")) {
        return false;
    }
    if (!air_append_evidence_node(air,
                                  AIR_EVIDENCE_RIR_BOUNDARY,
                                  boundary_index,
                                  scope_name,
                                  boundary->source_name,
                                  error_message)) {
        return false;
    }
    boundary->has_rir_boundary_evidence = true;
    air->rir_boundary_evidence_count++;
    return true;
}

static bool
air_append_rir_authority_evidence(AIRProgram *air,
                                  AIRBoundaryNode *boundary,
                                  size_t boundary_index,
                                  const char *scope_name,
                                  const char *authority_name,
                                  char **error_message)
{
    if (air_boundary_has_evidence_kind_subject(air,
                                               boundary_index,
                                               AIR_EVIDENCE_RIR_AUTHORITY,
                                               authority_name)) {
        return true;
    }
    if (!air_assign_first_owned_name(air,
                                     &boundary->rir_authority_evidence_name,
                                     authority_name,
                                     error_message,
                                     "RIR authority")) {
        return false;
    }
    if (!air_append_evidence_node(air,
                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                  boundary_index,
                                  scope_name,
                                  authority_name,
                                  error_message)) {
        return false;
    }
    boundary->has_rir_authority_evidence = true;
    return true;
}

static bool
air_collect_rir_scope_fact_authority(AIRProgram *air,
                                     AIRBoundaryNode *boundary,
                                     size_t boundary_index,
                                     const RIRScope *scope,
                                     const char *scope_name,
                                     char **error_message)
{
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind != RIR_FACT_AUTHORITY)
            continue;
        if (!air_boundary_authority_matches(boundary, fact->name))
            continue;
        if (!air_append_rir_authority_evidence(air,
                                               boundary,
                                               boundary_index,
                                               scope_name,
                                               fact->name,
                                               error_message)) {
            return false;
        }
    }
    return true;
}

static bool
air_collect_rir_scope_op_authority(AIRProgram *air,
                                   AIRBoundaryNode *boundary,
                                   size_t boundary_index,
                                   const RIRScope *scope,
                                   const char *scope_name,
                                   char **error_message)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind != RIR_OP_AUTHORIZE)
            continue;
        if (!air_boundary_authority_matches(boundary, op->subject))
            continue;
        if (!air_append_rir_authority_evidence(air,
                                               boundary,
                                               boundary_index,
                                               scope_name,
                                               op->subject,
                                               error_message)) {
            return false;
        }
    }
    return true;
}

bool
air_collect_rir_scope_boundary_evidence(AIRProgram *air,
                                        const RIRScope *scope,
                                        char **error_message)
{
    const char *scope_name;

    if (air == NULL || scope == NULL)
        return true;
    scope_name = air_rir_scope_provider_name(scope);
    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];

        if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
            continue;
        if (!air_require_rir_scope_provider(scope, error_message))
            return false;
        if (!air_append_rir_boundary_evidence(air,
                                              boundary,
                                              i,
                                              scope_name,
                                              error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_fact_authority(air,
                                                  boundary,
                                                  i,
                                                  scope,
                                                  scope_name,
                                                  error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_op_authority(air,
                                                boundary,
                                                i,
                                                scope,
                                                scope_name,
                                                error_message)) {
            return false;
        }
    }
    return true;
}
