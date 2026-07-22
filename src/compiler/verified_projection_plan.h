#ifndef PERGYRA_VERIFIED_PROJECTION_PLAN_H
#define PERGYRA_VERIFIED_PROJECTION_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mir.h"
#include "air_evidence_certificate.h"
#include "../common/execution_lane_kind.h"

typedef enum PgyProjectionTarget {
    PGY_PROJECTION_TARGET_C,
    PGY_PROJECTION_TARGET_LLVM
} PgyProjectionTarget;

typedef enum PgyProjectionAxis {
    PGY_PROJECTION_AXIS_INTENT_OBSERVABILITY
} PgyProjectionAxis;

typedef enum PgyProjectionDisposition {
    PGY_PROJECTION_ERASE,
    PGY_PROJECTION_MATERIALIZE,
    PGY_PROJECTION_RETAIN,
    PGY_PROJECTION_SUMMARIZE,
    PGY_PROJECTION_REJECT
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

/* Parallel capture is a separate verified row family.  It is deliberately
 * not folded into the intent-observability row: capture facts have different
 * owners, consumers, and dispositions, while both families share the same
 * AIR certificate admission boundary. */
typedef struct PgyVerifiedParallelCaptureRow {
    uint32_t source_stable_id;
    const char *name; /* borrowed from the MIR owner for this compilation */
    MIRParallelCaptureDispositionKind kind;
    size_t writer_task;
    PgyProjectionDisposition disposition;
} PgyVerifiedParallelCaptureRow;

typedef struct PgyVerifiedParallelCapturePlan {
    uint32_t revision;
    uint64_t digest;
    uint64_t air_certificate_fingerprint;
    PgyVerifiedParallelCaptureRow *rows; /* owned; released by ..._dispose */
    size_t row_count;
    bool verified;
} PgyVerifiedParallelCapturePlan;

#define PGY_VERIFIED_PARALLEL_CAPTURE_PLAN_REVISION UINT32_C(1)

uint64_t pgy_verified_parallel_capture_plan_digest(
    const PgyVerifiedParallelCapturePlan *plan);
bool pgy_verified_parallel_capture_plan_identity_ready(
    const PgyVerifiedParallelCapturePlan *plan);
bool pgy_verified_parallel_capture_plan_from_air(
    const PgyAirVerification *air,
    const MIRProgram *mir,
    PgyVerifiedParallelCapturePlan *plan_out,
    const char **error_out);
void pgy_verified_parallel_capture_plan_dispose(
    PgyVerifiedParallelCapturePlan *plan);
const PgyVerifiedParallelCaptureRow *
pgy_verified_parallel_capture_disposition_find(
    const PgyVerifiedParallelCapturePlan *plan,
    const MIRParallelCaptureBoundaryFact *boundary,
    const char *name,
    MIRParallelCaptureDispositionKind kind);

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

/*
 * Per-site spawn execution-lane plan: the second verified projection artifact.
 * AIR classifies every spawn boundary through the SEA decision table
 * (capture/effect/movability evidence, docs/146); this plan carries the
 * classified lane per spawn AST site into the backends.  Emitters must take
 * the lane from pgy_verified_spawn_lane_plan_lookup — recovering a lane from
 * source spelling inside a backend is the drift this artifact removes.
 */
typedef struct PgySpawnLaneFactRow {
    uint32_t              source_stable_id;
    PgyExecutionLane      lane;   /* classified lane; never the rejected lane */
} PgySpawnLaneFactRow;

typedef struct PgySpawnLanePlan {
    uint32_t             revision;
    PgySpawnLaneFactRow *rows;      /* owned; released by ..._dispose */
    size_t               row_count;
    bool                 verified;
} PgySpawnLanePlan;

#define PGY_SPAWN_LANE_PLAN_REVISION UINT32_C(1)

/* Fail-closed producer: requires a certified AIR verification, refuses a
 * rejected-lane spawn boundary, and refuses conflicting lanes for one site. */
bool pgy_verified_spawn_lane_plan_from_air(
    const PgyAirVerification *air,
    PgySpawnLanePlan *plan_out,
    const char **error_out);
void pgy_verified_spawn_lane_plan_dispose(PgySpawnLanePlan *plan);
bool pgy_verified_spawn_lane_plan_lookup(
    const PgySpawnLanePlan *plan,
    uint32_t source_stable_id,
    PgyExecutionLane *lane_out);

#endif /* PERGYRA_VERIFIED_PROJECTION_PLAN_H */
