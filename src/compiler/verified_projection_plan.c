#include "verified_projection_plan.h"

#include "mir_surface_usage.h"

bool
pgy_verified_projection_plan_intent_observability(
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out)
{
    bool materialize;

    if (error_out != NULL)
        *error_out = NULL;
    if (row_out == NULL) {
        if (error_out != NULL)
            *error_out = "verified projection plan: missing output row";
        return false;
    }
    if (mir == NULL) {
        if (error_out != NULL)
            *error_out = "verified projection plan: missing MIR program";
        return false;
    }
    if (target != PGY_PROJECTION_TARGET_C
        && target != PGY_PROJECTION_TARGET_LLVM) {
        if (error_out != NULL)
            *error_out = "verified projection plan: unsupported projection target";
        return false;
    }
    if (!mir_program_has_inventory_surface_usage_facts(mir)) {
        if (error_out != NULL) {
            *error_out =
                "verified projection plan: MIR program is missing inventory surface usage facts";
        }
        return false;
    }

    materialize =
        mir_program_recorded_inventory_uses_intent_observability_surface(mir);
    row_out->projection_plan_id = 1;
    row_out->target = target;
    row_out->axis = PGY_PROJECTION_AXIS_INTENT_OBSERVABILITY;
    row_out->disposition = materialize
        ? PGY_PROJECTION_MATERIALIZE : PGY_PROJECTION_ERASE;
    row_out->runtime_profile = materialize
        ? PGY_PROJECTION_RUNTIME_OBS1 : PGY_PROJECTION_RUNTIME_OBS0;
    row_out->reason = materialize
        ? "mir:inventory:intent_observability_surface"
        : "mir:inventory:no_intent_observability_surface";
    row_out->verified = true;
    return true;
}
