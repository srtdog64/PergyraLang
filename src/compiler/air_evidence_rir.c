/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection orchestrator.
 */

#include "air_internal.h"

/* A zone owns authority only through an explicit `authority` declaration,
 * which the RIR builder records as an AUTHORIZED authority-handle fact on the
 * zone scope. Stamp that ownership onto each zone boundary before evidence
 * collection runs, so the intent-alias admission in
 * air_rir_scope_provides_boundary_evidence can require it: a step-local
 * `authorized by:` against a zone that declares no authority proves nothing,
 * and the boundary must keep failing closed as evidence-missing. */
static void
air_stamp_zone_authority_ownership(AIRProgram *air,
                                   const RIRScopeInventory *inventory)
{
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        if (boundary == NULL || boundary->kind != AIR_BOUNDARY_ZONE)
            continue;
        for (size_t j = 0; j < inventory->count && !boundary->zone_owns_rir_authority; j++) {
            const RIRScope *scope = rir_scope_inventory_get(inventory, j);
            if (scope == NULL || scope->kind != RIR_SCOPE_ZONE
                || !air_name_matches(scope->name, boundary->source_name))
                continue;
            for (size_t k = 0; k < rir_scope_fact_count(scope); k++) {
                const RIRFact *fact = rir_scope_fact_at(scope, k);
                if (fact != NULL
                    && fact->kind == RIR_FACT_AUTHORITY
                    && fact->resource_kind == RIR_RESOURCE_AUTHORITY_HANDLE
                    && fact->state == RIR_STATE_AUTHORIZED) {
                    boundary->zone_owns_rir_authority = true;
                    break;
                }
            }
        }
    }
}

bool
air_collect_rir_evidence(AIRProgram *air,
                         const RIRProgram *rir,
                         char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;

    air_mark_rir_input(air);
    RIRScopeInventory inventory;
    rir_scope_inventory_from_program(rir, &inventory);

    air_stamp_zone_authority_ownership(air, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);

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
