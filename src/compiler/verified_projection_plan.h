#ifndef PERGYRA_VERIFIED_PROJECTION_PLAN_H
#define PERGYRA_VERIFIED_PROJECTION_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "mir.h"

typedef enum PgyProjectionTarget {
    PGY_PROJECTION_TARGET_C,
    PGY_PROJECTION_TARGET_LLVM
} PgyProjectionTarget;

typedef enum PgyProjectionAxis {
    PGY_PROJECTION_AXIS_INTENT_OBSERVABILITY
} PgyProjectionAxis;

typedef enum PgyProjectionDisposition {
    PGY_PROJECTION_ERASE,
    PGY_PROJECTION_MATERIALIZE
} PgyProjectionDisposition;

typedef enum PgyProjectionRuntimeProfile {
    PGY_PROJECTION_RUNTIME_OBS0,
    PGY_PROJECTION_RUNTIME_OBS1
} PgyProjectionRuntimeProfile;

typedef struct PgyVerifiedProjectionPlanRow {
    uint32_t projection_plan_id;
    PgyProjectionTarget target;
    PgyProjectionAxis axis;
    PgyProjectionDisposition disposition;
    PgyProjectionRuntimeProfile runtime_profile;
    const char *reason;
    bool verified;
} PgyVerifiedProjectionPlanRow;

bool pgy_verified_projection_plan_intent_observability(
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out);

#endif /* PERGYRA_VERIFIED_PROJECTION_PLAN_H */
