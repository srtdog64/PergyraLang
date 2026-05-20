/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection orchestrator.
 */

#include "air_internal.h"

static bool
air_count_rir_authority_summaries(AIRProgram *air,
                                  const RIRScope *scope,
                                  char **error_message)
{
    if (air == NULL || scope == NULL)
        return true;
    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].kind != RIR_FACT_AUTHORITY)
            continue;
        if (!air_increment_evidence_summary_count(
                air,
                AIR_EVIDENCE_RIR_AUTHORITY)) {
            air_set_error(error_message,
                          "AIR RIR authority evidence counter overflow");
            return false;
        }
    }
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind != RIR_OP_AUTHORIZE)
            continue;
        if (!air_increment_evidence_summary_count(
                air,
                AIR_EVIDENCE_RIR_AUTHORITY)) {
            air_set_error(error_message,
                          "AIR RIR authority evidence counter overflow");
            return false;
        }
    }
    return true;
}

bool
air_collect_rir_evidence(AIRProgram *air,
                         const RIRProgram *rir,
                         char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;

    air_mark_rir_input(air);
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];

        if (!air_count_rir_authority_summaries(air,
                                               scope,
                                               error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_propagation_evidence(air,
                                                        scope,
                                                        error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_boundary_evidence(air,
                                                     scope,
                                                     error_message)) {
            return false;
        }
    }
    return true;
}
