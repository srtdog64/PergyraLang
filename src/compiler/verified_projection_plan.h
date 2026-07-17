#ifndef PERGYRA_VERIFIED_PROJECTION_PLAN_H
#define PERGYRA_VERIFIED_PROJECTION_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "mir.h"
#include "air_evidence_certificate.h"

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
    /* Artifact identity is anchored to the planner revision and the complete
     * derived row.  The digest is an identity/mutation guard, not a security
     * primitive; consumers must still validate the row before use. */
    uint32_t projection_plan_revision;
    uint64_t projection_plan_digest;
    uint32_t projection_plan_id;
    PgyProjectionTarget target;
    PgyProjectionAxis axis;
    PgyProjectionDisposition disposition;
    PgyProjectionRuntimeProfile runtime_profile;
    const char *reason;
    const char *air_certificate_schema;
    uint64_t air_certificate_fingerprint;
    uint64_t target_capability_fingerprint;
    uint64_t machine_layer_manifest_fingerprint;
    /* Target-owned MachineDeclaration/refinement identity.  Backends may only
     * consume this derived fingerprint; they must not reinterpret addresses. */
    uint64_t machine_layer_physical_manifest_fingerprint;
    /* The selected device grant is the last physical-shape fact carried by
     * the planner.  Backends pass these owner values to the runtime startup
     * bind; they must not recover a window from a grant name or literal. */
    uint64_t machine_layer_physical_grant_base;
    uint64_t machine_layer_physical_grant_size;
    uint32_t machine_layer_physical_grant_mode;
    /* Non-host declarations require an embedder-owned runtime mapping
     * provider before generated code may touch the declared window. */
    bool machine_layer_runtime_provider_required;
    bool verified;
} PgyVerifiedProjectionPlanRow;

#define PGY_VERIFIED_PROJECTION_PLAN_REVISION UINT32_C(1)

uint64_t pgy_verified_projection_plan_digest(
    const PgyVerifiedProjectionPlanRow *row);
bool pgy_verified_projection_plan_identity_ready(
    const PgyVerifiedProjectionPlanRow *row);

bool pgy_verified_projection_plan_intent_observability(
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out);

/* Production planner entrypoint.  The AIR certificate is the only permitted
 * evidence bridge; C/LLVM callers must not derive this row from source or
 * backend-local observations. */
bool pgy_verified_projection_plan_intent_observability_with_air(
    const PgyAirVerification *air,
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out);

#endif /* PERGYRA_VERIFIED_PROJECTION_PLAN_H */
