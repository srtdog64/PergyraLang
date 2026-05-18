/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection orchestrator.
 */

#include "air_internal.h"

static void
air_count_rir_authority_summaries(AIRProgram *air, const RIRScope *scope)
{
    if (air == NULL || scope == NULL)
        return;
    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].kind == RIR_FACT_AUTHORITY)
            air->rir_authority_evidence_count++;
    }
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == RIR_OP_AUTHORIZE)
            air->rir_authority_evidence_count++;
    }
}

bool
air_collect_rir_evidence(AIRProgram *air,
                         const RIRProgram *rir,
                         char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;

    air->has_rir_input = true;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];

        air_count_rir_authority_summaries(air, scope);
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
