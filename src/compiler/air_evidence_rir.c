/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection orchestrator.
 */

#include "air_internal.h"

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
