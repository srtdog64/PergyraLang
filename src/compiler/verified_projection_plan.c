#include "verified_projection_plan.h"

#include <stdlib.h>
#include <string.h>

#include "air.h"
#include "mir_surface_usage.h"
#include "air_evidence_certificate.h"
#include "machine_layer_manifest.h"
#include "target_capability_contract.h"

static uint64_t
verified_projection_plan_mix_byte(uint64_t hash, uint8_t byte)
{
    hash ^= (uint64_t)byte;
    return hash * UINT64_C(1099511628211);
}

static uint64_t
verified_projection_plan_mix_u32(uint64_t hash, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        hash = verified_projection_plan_mix_byte(hash,
            (uint8_t)((value >> (i * 8)) & 0xffu));
    return hash;
}

static uint64_t
verified_projection_plan_mix_u64(uint64_t hash, uint64_t value)
{
    for (unsigned i = 0; i < 8; i++)
        hash = verified_projection_plan_mix_byte(hash,
            (uint8_t)((value >> (i * 8)) & UINT64_C(0xff)));
    return hash;
}

static uint64_t
verified_projection_plan_mix_bool(uint64_t hash, bool value)
{
    return verified_projection_plan_mix_byte(hash, value ? 1u : 0u);
}

static uint64_t
verified_projection_plan_mix_text(uint64_t hash, const char *text)
{
    if (text == NULL)
        return verified_projection_plan_mix_byte(hash, 0);
    hash = verified_projection_plan_mix_byte(hash, 1);
    while (*text != '\0')
        hash = verified_projection_plan_mix_byte(hash, (uint8_t)*text++);
    return verified_projection_plan_mix_byte(hash, 0);
}

uint64_t
pgy_verified_projection_plan_digest(const PgyVerifiedProjectionPlanRow *row)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (row == NULL)
        return 0;
    hash = verified_projection_plan_mix_u32(hash,
        row->projection_plan_revision);
    hash = verified_projection_plan_mix_u32(hash, row->projection_plan_id);
    hash = verified_projection_plan_mix_u32(hash, (uint32_t)row->target);
    hash = verified_projection_plan_mix_u32(hash, (uint32_t)row->axis);
    hash = verified_projection_plan_mix_u32(hash,
        (uint32_t)row->disposition);
    hash = verified_projection_plan_mix_u32(hash,
        (uint32_t)row->runtime_profile);
    hash = verified_projection_plan_mix_text(hash, row->reason);
    hash = verified_projection_plan_mix_text(hash,
        row->air_certificate_schema);
    hash = verified_projection_plan_mix_u64(hash,
        row->air_certificate_fingerprint);
    hash = verified_projection_plan_mix_u64(hash,
        row->target_capability_fingerprint);
    hash = verified_projection_plan_mix_u64(hash,
        row->machine_layer_manifest_fingerprint);
    hash = verified_projection_plan_mix_u64(hash,
        row->machine_layer_physical_manifest_fingerprint);
    hash = verified_projection_plan_mix_u64(hash,
        row->machine_layer_physical_grant_base);
    hash = verified_projection_plan_mix_u64(hash,
        row->machine_layer_physical_grant_size);
    hash = verified_projection_plan_mix_u32(hash,
        row->machine_layer_physical_grant_mode);
    hash = verified_projection_plan_mix_bool(hash,
        row->machine_layer_runtime_provider_required);
    return verified_projection_plan_mix_bool(hash, row->verified);
}

bool
pgy_verified_projection_plan_identity_ready(
    const PgyVerifiedProjectionPlanRow *row)
{
    return row != NULL
        && row->verified
        && row->projection_plan_revision
            == PGY_VERIFIED_PROJECTION_PLAN_REVISION
        && row->projection_plan_digest != 0
        && row->projection_plan_digest
            == pgy_verified_projection_plan_digest(row);
}

static bool
verified_projection_plan_intent_observability_from_mir(
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
    memset(row_out, 0, sizeof(*row_out));
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
    row_out->projection_plan_revision =
        PGY_VERIFIED_PROJECTION_PLAN_REVISION;
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
    row_out->air_certificate_schema = NULL;
    row_out->air_certificate_fingerprint = 0;
    row_out->target_capability_fingerprint = 0;
    row_out->machine_layer_manifest_fingerprint = 0;
    row_out->machine_layer_physical_manifest_fingerprint = 0;
    row_out->machine_layer_physical_grant_base = 0;
    row_out->machine_layer_physical_grant_size = 0;
    row_out->machine_layer_physical_grant_mode = 0;
    row_out->machine_layer_runtime_provider_required = false;
    row_out->verified = true;
    row_out->projection_plan_digest =
        pgy_verified_projection_plan_digest(row_out);
    return true;
}

bool
pgy_verified_projection_plan_intent_observability(
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out)
{
    /* Kept for the MIR-only unit probe.  Production C/LLVM entrypoints use
       the AIR-bound function below and therefore cannot bypass certification. */
    return verified_projection_plan_intent_observability_from_mir(
        mir, target, row_out, error_out);
}

bool
pgy_verified_projection_plan_intent_observability_with_air(
    const PgyAirVerification *air,
    const MIRProgram *mir,
    PgyProjectionTarget target,
    PgyVerifiedProjectionPlanRow *row_out,
    const char **error_out)
{
    const char *certificate_error = NULL;
    const char *target_error = NULL;
    const char *machine_error = NULL;
    const PgyMachineLayerPhysicalManifest *physical_manifest;
    const PgyMachineLayerPhysicalGrant *device_grant;
    uint64_t target_capability_fingerprint = 0;
    uint64_t machine_layer_manifest_fingerprint = 0;
    uint64_t machine_layer_physical_manifest_fingerprint = 0;

    if (!pgy_air_evidence_certificate_ready(air, &certificate_error)) {
        if (error_out != NULL)
            *error_out = certificate_error != NULL
                ? certificate_error
                : "verified projection plan: AIR evidence certificate is missing";
        return false;
    }
    if (!pgy_target_capability_ready_for_projection(
            target == PGY_PROJECTION_TARGET_C ? "cpu-c" : "cpu-llvm",
            &target_capability_fingerprint, &target_error)) {
        if (error_out != NULL)
            *error_out = target_error != NULL
                ? target_error
                : "verified projection plan: target capability envelope is missing";
        return false;
    }
    if (!pgy_machine_layer_physical_projection_ready_for_backend(
            target == PGY_PROJECTION_TARGET_C ? "cpu-c" : "cpu-llvm",
            &machine_error)) {
        if (error_out != NULL)
            *error_out = machine_error != NULL
                ? machine_error
                : "verified projection plan: machine-layer projection is missing";
        return false;
    }
    machine_layer_manifest_fingerprint =
        pgy_machine_layer_manifest_fingerprint(
            pgy_machine_layer_target_manifest());
    if (machine_layer_manifest_fingerprint == 0) {
        if (error_out != NULL)
            *error_out =
            "verified projection plan: machine-layer manifest fingerprint is missing";
        return false;
    }
    machine_layer_physical_manifest_fingerprint =
        pgy_machine_layer_physical_manifest_fingerprint(
            pgy_machine_layer_physical_manifest());
    if (machine_layer_physical_manifest_fingerprint == 0) {
        if (error_out != NULL)
            *error_out =
                "verified projection plan: physical machine declaration fingerprint is missing";
        return false;
    }
    physical_manifest = pgy_machine_layer_physical_manifest();
    device_grant = pgy_machine_layer_physical_manifest_grant(
        physical_manifest, physical_manifest->device_grant_id);
    if (device_grant == NULL) {
        if (error_out != NULL)
            *error_out =
                "verified projection plan: physical device grant is missing";
        return false;
    }
    if (!verified_projection_plan_intent_observability_from_mir(
            mir, target, row_out, error_out)) {
        return false;
    }
    row_out->air_certificate_schema = PGY_AIR_EVIDENCE_CERTIFICATE_SCHEMA;
    row_out->air_certificate_fingerprint =
        air->verification_certificate_fingerprint;
    row_out->target_capability_fingerprint = target_capability_fingerprint;
    row_out->machine_layer_manifest_fingerprint =
        machine_layer_manifest_fingerprint;
    row_out->machine_layer_physical_manifest_fingerprint =
        machine_layer_physical_manifest_fingerprint;
    row_out->machine_layer_physical_grant_base = device_grant->base;
    row_out->machine_layer_physical_grant_size = device_grant->size;
    row_out->machine_layer_physical_grant_mode = (uint32_t)device_grant->mode;
    row_out->machine_layer_runtime_provider_required =
        physical_manifest->manifest_id != NULL
        && strcmp(physical_manifest->manifest_id,
                  PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_ID) != 0;
    if (target_capability_fingerprint == 0) {
        if (error_out != NULL)
            *error_out =
                "verified projection plan: target capability fingerprint is missing";
        return false;
    }
    row_out->projection_plan_digest =
        pgy_verified_projection_plan_digest(row_out);
    return true;
}

static bool
spawn_lane_boundary_is_spawn_site(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && boundary->ast != NULL
        && boundary->ast->type == AST_SPAWN_EXPR;
}

bool
pgy_verified_spawn_lane_plan_from_air(const PgyAirVerification *air,
                                      PgySpawnLanePlan *plan_out,
                                      const char **error_out)
{
    const char *certificate_error = NULL;
    size_t spawn_count = 0;

    if (error_out != NULL)
        *error_out = NULL;
    if (plan_out == NULL) {
        if (error_out != NULL)
            *error_out = "verified spawn-lane plan: missing output plan";
        return false;
    }
    memset(plan_out, 0, sizeof(*plan_out));
    if (!pgy_air_evidence_certificate_ready(air, &certificate_error)) {
        if (error_out != NULL)
            *error_out = certificate_error != NULL
                ? certificate_error
                : "verified spawn-lane plan: AIR evidence certificate is missing";
        return false;
    }

    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        if (spawn_lane_boundary_is_spawn_site(air_boundary_node_at(air, i)))
            spawn_count++;
    }
    if (spawn_count > 0) {
        plan_out->rows = calloc(spawn_count, sizeof(*plan_out->rows));
        if (plan_out->rows == NULL) {
            if (error_out != NULL)
                *error_out = "verified spawn-lane plan: row allocation failed";
            return false;
        }
    }

    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        bool duplicate = false;

        if (!spawn_lane_boundary_is_spawn_site(boundary))
            continue;
        uint32_t source_stable_id = boundary->source_stable_id;
        if (source_stable_id == 0) {
            pgy_verified_spawn_lane_plan_dispose(plan_out);
            if (error_out != NULL)
                *error_out =
                    "verified spawn-lane plan: spawn boundary is missing stable source identity";
            return false;
        }
        /* A rejected lane means contradictory evidence (e.g. a pinned capture
           on a movable executor).  It must fail the compile here, observably,
           not surface as a runtime null-handle panic. */
        if (boundary->execution_lane == PGY_LANE_REJECT) {
            pgy_verified_spawn_lane_plan_dispose(plan_out);
            if (error_out != NULL)
                *error_out =
                    "verified spawn-lane plan: a spawn boundary classifies to the rejected lane (contradictory capture/movability evidence)";
            return false;
        }
        for (size_t j = 0; j < plan_out->row_count; j++) {
            if (plan_out->rows[j].source_stable_id != source_stable_id)
                continue;
            if (plan_out->rows[j].lane != boundary->execution_lane) {
                pgy_verified_spawn_lane_plan_dispose(plan_out);
                if (error_out != NULL)
                    *error_out =
                        "verified spawn-lane plan: conflicting lane facts for one spawn site";
                return false;
            }
            duplicate = true;
            break;
        }
        if (duplicate)
            continue;
        plan_out->rows[plan_out->row_count].source_stable_id = source_stable_id;
        plan_out->rows[plan_out->row_count].lane = boundary->execution_lane;
        plan_out->row_count++;
    }

    plan_out->revision = PGY_SPAWN_LANE_PLAN_REVISION;
    plan_out->verified = true;
    return true;
}

void
pgy_verified_spawn_lane_plan_dispose(PgySpawnLanePlan *plan)
{
    if (plan == NULL)
        return;
    free(plan->rows);
    memset(plan, 0, sizeof(*plan));
}

bool
pgy_verified_spawn_lane_plan_lookup(const PgySpawnLanePlan *plan,
                                    uint32_t source_stable_id,
                                    PgyExecutionLane *lane_out)
{
    if (plan == NULL || !plan->verified || source_stable_id == 0)
        return false;
    for (size_t i = 0; i < plan->row_count; i++) {
        if (plan->rows[i].source_stable_id == source_stable_id) {
            if (lane_out != NULL)
                *lane_out = plan->rows[i].lane;
            return true;
        }
    }
    return false;
}
