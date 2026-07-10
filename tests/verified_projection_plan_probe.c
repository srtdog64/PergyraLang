#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common/intent_observability_abi.h"
#include "compiler/mir_surface_usage.h"
#include "compiler/verified_projection_plan.h"

bool
mir_program_has_inventory_surface_usage_facts(const MIRProgram *mir)
{
    return mir != NULL && mir->has_inventory_surface_usage_facts;
}

bool
mir_program_recorded_inventory_uses_intent_observability_surface(
    const MIRProgram *mir)
{
    return mir != NULL && mir->inventory_uses_intent_observability_surface;
}

int
main(void)
{
    MIRProgram mir = {0};
    PgyVerifiedProjectionPlanRow plan = {0};
    const char *error = NULL;
    const char *previous = NULL;
    size_t count = pgy_intent_observability_abi_row_count();

    if (pgy_verified_projection_plan_intent_observability(
            &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 1;
    }
    if (error == NULL
        || strstr(error, "missing inventory surface usage facts") == NULL) {
        return 2;
    }

    mir.has_inventory_surface_usage_facts = true;
    if (!pgy_verified_projection_plan_intent_observability(
            &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 3;
    }
    if (!plan.verified || plan.projection_plan_id != 1
        || plan.disposition != PGY_PROJECTION_ERASE
        || plan.runtime_profile != PGY_PROJECTION_RUNTIME_OBS0) {
        return 4;
    }

    mir.inventory_uses_intent_observability_surface = true;
    if (!pgy_verified_projection_plan_intent_observability(
            &mir, PGY_PROJECTION_TARGET_LLVM, &plan, &error)) {
        return 5;
    }
    if (!plan.verified || plan.target != PGY_PROJECTION_TARGET_LLVM
        || plan.disposition != PGY_PROJECTION_MATERIALIZE
        || plan.runtime_profile != PGY_PROJECTION_RUNTIME_OBS1) {
        return 6;
    }

    if (count != 51)
        return 7;
    for (size_t i = 0; i < count; i++) {
        const PgyIntentObservabilityAbiRow *row =
            pgy_intent_observability_abi_row_at(i);
        if (row == NULL || row->runtime_call_abi_id == 0
            || row->source_name == NULL || row->runtime_name == NULL
            || row->arg_count > 2
            || pgy_intent_observability_return_type_name(row->return_kind)
                == NULL
            || pgy_intent_observability_abi_row_by_source(row->source_name)
                != row) {
            return 8;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyIntentObservabilityAbiRow *previous_row =
                pgy_intent_observability_abi_row_at(j);
            if (previous_row == NULL
                || previous_row->runtime_call_abi_id
                    == row->runtime_call_abi_id) {
                return 9;
            }
        }
        for (size_t j = 0; j < row->arg_count; j++) {
            PgyIntentObservabilityArgumentKind kind =
                pgy_intent_observability_argument_kind_at(row, j);
            if (kind != PGY_INTENT_OBSERVABILITY_ARGUMENT_INT
                || pgy_intent_observability_argument_type_name(kind)
                    == NULL) {
                return 10;
            }
        }
        if (previous != NULL && strcmp(previous, row->source_name) >= 0)
            return 11;
        previous = row->source_name;
    }
    puts("[verified-projection-plan] OBS0 erase and OBS1 materialize rows verified");
    return 0;
}
