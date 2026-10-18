#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common/intent_observability_abi.h"
#include "compiler/mir_surface_usage.h"
#include "compiler/air_evidence_certificate.h"
#include "compiler/machine_layer_manifest.h"
#include "compiler/compiler_internal.h"
#include "compiler/verified_projection_plan.h"
#include "runtime/pgy_runtime_machine_layer_inline.h"

/* The probe links the certificate owner as a small standalone unit.  Keep
 * these owner accessors local so the probe does not accidentally link the
 * whole AIR synthesis object graph. These fixtures have no native origin;
 * only the real AIR integration suite may establish native source binding. */
bool
air_source_program_current(const AIRProgram *air, const ASTNode *expected)
{
    (void)air;
    (void)expected;
    return false;
}

bool
air_mir_source_current(const AIRProgram *air, const MIRProgram *mir)
{
    return air != NULL && mir != NULL &&
        air->source_program_syntax_id == 0 && air->source_origin == NULL &&
        mir->collection_effect_source_program == NULL &&
        mir->collection_effect_snapshot == NULL;
}

/* This standalone row/ABI unit supplies a test double for an empty MIR only.
 * It does not issue native evidence or duplicate the production fingerprint.
 * Real post-certificate inventory mutation is tested with the actual issuer
 * and planners in test_air. Each surface change here starts a new fixture. */
static uint64_t
probe_empty_mir_identity(const MIRProgram *mir)
{
    if (mir == NULL || mir->routine_count != 0 || mir->routines != NULL
        || mir->parallel_capture_boundary_count != 0
        || mir->parallel_capture_boundaries != NULL
        || mir->collection_effect_source_program != NULL
        || mir->collection_effect_snapshot != NULL)
        return 0;
    return UINT64_C(0x70726f6265000000)
        | (mir->has_inventory_surface_usage_facts ? UINT64_C(1) : 0)
        | (mir->inventory_uses_thread_pool_surface ? UINT64_C(2) : 0)
        | (mir->inventory_uses_intent_observability_surface ? UINT64_C(4) : 0);
}

bool
air_mir_evidence_binding_current(const AIRProgram *air, const MIRProgram *mir)
{
    uint64_t token = probe_empty_mir_identity(mir);
    return token != 0 && air_mir_source_current(air, mir)
        && air->mir_evidence_collection_started && air->mir_evidence_bound
        && air->mir_evidence_binding_fingerprint == token;
}

static bool
probe_new_empty_mir_fixture(AIRProgram *air, const MIRProgram *mir,
                            const char **error)
{
    *air = (AIRProgram){0};
    air->has_hir_input = true;
    air->has_rir_input = true;
    air->has_mir_input = true;
    air->mir_evidence_collection_started = true;
    air->mir_evidence_bound = true;
    air->mir_evidence_binding_fingerprint = probe_empty_mir_identity(mir);
    return air->mir_evidence_binding_fingerprint != 0
        && pgy_air_evidence_certificate_issue(air, error);
}

bool
air_requires_strict_evidence(const AIRProgram *air)
{
    return air != NULL && air->strict_evidence;
}

const AIRFunctionParamFlowSummary *
air_function_param_flow_summary_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || air->function_param_flow_summaries == NULL ||
        index >= air->function_param_flow_summary_count)
        return NULL;
    return &air->function_param_flow_summaries[index];
}

bool
air_intent_storage_valid(const AIRProgram *air)
{
    return air != NULL && (air->intent_count == 0 || air->intents != NULL);
}

bool
air_boundary_storage_valid(const AIRProgram *air)
{
    return air != NULL
        && (air->boundary_count == 0 || air->boundaries != NULL);
}

bool
air_drift_storage_valid(const AIRProgram *air)
{
    return air != NULL && (air->drift_count == 0 || air->drifts != NULL);
}

bool
air_evidence_inventory_storage_valid(const AIRProgram *air)
{
    return air != NULL
        && (air->evidence_count == 0 || air->evidence_nodes != NULL);
}

bool
air_has_hir_input(const AIRProgram *air)
{
    return air != NULL && air->has_hir_input;
}

bool
air_has_rir_input(const AIRProgram *air)
{
    return air != NULL && air->has_rir_input;
}

bool
air_has_mir_input(const AIRProgram *air)
{
    return air != NULL && air->has_mir_input;
}

size_t
air_intent_node_count(const AIRProgram *air)
{
    return air != NULL ? air->intent_count : 0;
}

const AIRIntentNode *
air_intent_node_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->intent_count)
        return NULL;
    return &air->intents[index];
}

size_t
air_boundary_node_count(const AIRProgram *air)
{
    return air != NULL ? air->boundary_count : 0;
}

const AIRBoundaryNode *
air_boundary_node_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->boundary_count)
        return NULL;
    return &air->boundaries[index];
}

size_t
air_drift_count(const AIRProgram *air)
{
    return air != NULL ? air->drift_count : 0;
}

size_t
air_evidence_node_count(const AIRProgram *air)
{
    return air != NULL ? air->evidence_count : 0;
}

const AIREvidenceNode *
air_evidence_node_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->evidence_count)
        return NULL;
    return &air->evidence_nodes[index];
}

size_t
air_propagation_requirement_count(const AIRProgram *air)
{
    return air != NULL ? air->propagation_requirement_count : 0;
}

const AIRPropagationRequirement *
air_propagation_requirement_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->propagation_requirement_count)
        return NULL;
    return &air->propagation_requirements[index];
}

size_t
air_machine_layer_site_count(const AIRProgram *air)
{
    return air != NULL ? air->machine_layer_site_count : 0;
}

const AIRMachineLayerSite *
air_machine_layer_site_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->machine_layer_site_count)
        return NULL;
    return &air->machine_layer_sites[index];
}

AIREvidenceKind
air_evidence_node_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->kind : AIR_EVIDENCE_HIR_ROUTINE;
}

AIREvidenceProviderKind
air_evidence_node_provider_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->provider_kind
                            : AIR_EVIDENCE_PROVIDER_UNKNOWN;
}

AIREvidenceSubjectKind
air_evidence_node_subject_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->subject_kind
                            : AIR_EVIDENCE_SUBJECT_UNKNOWN;
}

size_t
air_evidence_node_boundary_index_or(const AIREvidenceNode *evidence,
                                    size_t fallback)
{
    return evidence != NULL ? evidence->boundary_index : fallback;
}

const char *
air_evidence_node_provider_name_or(const AIREvidenceNode *evidence,
                                    const char *fallback)
{
    return evidence != NULL && evidence->provider_name != NULL
        ? evidence->provider_name : fallback;
}

const char *
air_evidence_node_subject_name_or(const AIREvidenceNode *evidence,
                                   const char *fallback)
{
    return evidence != NULL && evidence->subject_name != NULL
        ? evidence->subject_name : fallback;
}

bool
air_evidence_node_has_boundary_shape(const AIREvidenceNode *evidence)
{
    return evidence != NULL && evidence->has_boundary_shape;
}

AIRBoundaryKind
air_evidence_node_boundary_kind_or(const AIREvidenceNode *evidence,
                                   AIRBoundaryKind fallback)
{
    return evidence != NULL && evidence->has_boundary_shape
        ? evidence->boundary_kind : fallback;
}

const char *
air_evidence_node_boundary_owner_name_or(const AIREvidenceNode *evidence,
                                         const char *fallback)
{
    return evidence != NULL && evidence->boundary_owner_name != NULL
        ? evidence->boundary_owner_name : fallback;
}

const char *
air_evidence_node_boundary_source_name_or(const AIREvidenceNode *evidence,
                                          const char *fallback)
{
    return evidence != NULL && evidence->boundary_source_name != NULL
        ? evidence->boundary_source_name : fallback;
}

size_t
air_evidence_node_fact_count(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->fact_count : 0;
}

size_t
air_evidence_node_fallback_count(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->fallback_count : 0;
}

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

static int32_t
verified_plan_mapping_provider(uint64_t base, uint64_t size, uint32_t mode,
                               void *context)
{
    const uint64_t *expected = (const uint64_t *)context;
    return expected != NULL && base == expected[0] && size == expected[1]
        && mode == (uint32_t)expected[2];
}

int
main(void)
{
    MIRProgram mir = {0};
    PgyVerifiedProjectionPlanRow plan = {0};
    AIRProgram air = {0};
    const char *error = NULL;
    const char *previous = NULL;
    size_t count = pgy_intent_observability_abi_row_count();

    if (!probe_new_empty_mir_fixture(&air, &mir, &error))
        return 12;
    if (pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 1;
    }
    if (error == NULL
        || strstr(error, "missing inventory surface usage facts") == NULL) {
        return 2;
    }

    mir.has_inventory_surface_usage_facts = true;
    if (pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)
        || !probe_new_empty_mir_fixture(&air, &mir, &error))
        return 22;
    if (!pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 3;
    }
    if (!plan.verified || plan.projection_plan_id != 1
        || plan.disposition != PGY_PROJECTION_ERASE
        || plan.runtime_profile != PGY_PROJECTION_RUNTIME_OBS0
        || !pgy_verified_projection_plan_identity_ready(&plan)) {
        return 4;
    }

    {
        CompilerResult *artifact = compiler_success("probe.c", NULL);
        const char *identity_error = NULL;
        PgyVerifiedProjectionPlanRow malformed = plan;
        malformed.projection_plan_digest = 0;
        if (artifact == NULL
            || !compiler_result_bind_artifact_identity(
                   artifact, &plan, "emitted_c")
            || !compiler_result_artifact_identity_ready(
                   artifact, &identity_error)
            || compiler_result_bind_artifact_identity(
                   artifact, &malformed, "emitted_c")) {
            compiler_result_destroy(artifact);
            return 21;
        }
        compiler_result_destroy(artifact);
    }

    if (!pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 13;
    }
    if (!plan.verified
        || plan.air_certificate_schema == NULL
        || plan.air_certificate_fingerprint == 0
        || plan.target_capability_fingerprint == 0
        || plan.machine_layer_manifest_fingerprint == 0
        || plan.machine_layer_physical_manifest_fingerprint == 0
        || plan.machine_layer_physical_grant_base
            != pgy_machine_layer_physical_manifest()->grants[0].base
        || plan.machine_layer_physical_grant_size
            != pgy_machine_layer_physical_manifest()->grants[0].size
        || plan.machine_layer_physical_grant_mode
            != (uint32_t)pgy_machine_layer_physical_manifest()->grants[0].mode) {
        return 14;
    }
    air.verification_certificate_fingerprint ^= UINT64_C(1);
    if (pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)) {
        return 15;
    }
    if (error == NULL || strstr(error, "owner facts changed") == NULL)
        return 16;
    if (!pgy_air_evidence_certificate_issue(&air, &error))
        return 17;

    mir.inventory_uses_intent_observability_surface = true;
    if (pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_LLVM, &plan, &error)
        || !probe_new_empty_mir_fixture(&air, &mir, &error))
        return 23;
    if (!pgy_verified_projection_plan_intent_observability_with_air(
            &air, &mir, PGY_PROJECTION_TARGET_LLVM, &plan, &error)) {
        return 5;
    }
    if (!plan.verified || plan.target != PGY_PROJECTION_TARGET_LLVM
        || plan.disposition != PGY_PROJECTION_MATERIALIZE
        || plan.runtime_profile != PGY_PROJECTION_RUNTIME_OBS1
        || plan.target_capability_fingerprint == 0
        || plan.machine_layer_manifest_fingerprint == 0
        || plan.machine_layer_physical_manifest_fingerprint == 0
        || plan.machine_layer_physical_grant_size == 0
        || plan.machine_layer_physical_grant_mode == 0) {
        return 6;
    }

    {
        static const PgyMachineLayerPhysicalGrant provider_grants[] = {
            { "probe-device0", UINT64_C(0x30000000), UINT64_C(0x3000),
              PGY_MACHINE_LAYER_ACCESS_VOLATILE, true },
        };
        static const PgyMachineLayerPhysicalManifest provider = {
            "pergyra.machine-declaration.probe-v1",
            "probe-device",
            "probe-board",
            "probe-boot.v1",
            "probe-linker.v1",
            UINT64_C(0x7fffffff),
            "probe-device0",
            provider_grants,
            1,
            true
        };
        const PgyMachineLayerPhysicalGrant *grant = provider_grants;
        const uint64_t expected[] = {
            grant->base, grant->size, (uint64_t)grant->mode
        };
        if (!pgy_machine_layer_physical_manifest_bind(&provider, &error)
            || pgy_machine_layer_physical_manifest() != &provider
            || !pgy_verified_projection_plan_intent_observability_with_air(
                   &air, &mir, PGY_PROJECTION_TARGET_C, &plan, &error)
            || plan.machine_layer_physical_grant_base != grant->base
            || plan.machine_layer_physical_grant_size != grant->size
            || plan.machine_layer_physical_grant_mode != (uint32_t)grant->mode
            || plan.machine_layer_physical_manifest_fingerprint
                != pgy_machine_layer_physical_manifest_fingerprint(&provider)) {
            return 18;
        }
        if (pgy_machine_layer_runtime_bind_mapping_export(
                plan.machine_layer_manifest_fingerprint,
                plan.machine_layer_physical_manifest_fingerprint,
                plan.machine_layer_physical_grant_base,
                plan.machine_layer_physical_grant_size,
                plan.machine_layer_physical_grant_mode,
                UINT32_C(1)) != 0) {
            return 19;
        }
        if (!pgy_machine_layer_runtime_provider_bind_export(
                verified_plan_mapping_provider, (void *)expected)) {
            return 20;
        }
        if (pgy_machine_layer_runtime_bind_mapping_export(
                plan.machine_layer_manifest_fingerprint,
                plan.machine_layer_physical_manifest_fingerprint,
                plan.machine_layer_physical_grant_base,
                plan.machine_layer_physical_grant_size,
                plan.machine_layer_physical_grant_mode,
                plan.machine_layer_runtime_provider_required ? 1u : 0u) != 1
            || pgy_machine_layer_runtime_bind_mapping_export(
                   plan.machine_layer_manifest_fingerprint,
                   plan.machine_layer_physical_manifest_fingerprint,
                   plan.machine_layer_physical_grant_base,
                   plan.machine_layer_physical_grant_size,
                   plan.machine_layer_physical_grant_mode,
                   plan.machine_layer_runtime_provider_required ? 1u : 0u) != 1
            || pgy_machine_layer_runtime_bind_mapping_export(
                   plan.machine_layer_manifest_fingerprint,
                   plan.machine_layer_physical_manifest_fingerprint,
                   plan.machine_layer_physical_grant_base + 1,
                   plan.machine_layer_physical_grant_size,
                   plan.machine_layer_physical_grant_mode,
                   plan.machine_layer_runtime_provider_required ? 1u : 0u) != 0) {
            return 20;
        }
    }

    if (count != 51)
        return 7;
    for (size_t i = 0; i < count; i++) {
        const PgyIntentObservabilityAbiRow *row =
            pgy_intent_observability_abi_row_at(i);
        size_t argument_count =
            pgy_intent_observability_argument_count(row);
        if (row == NULL || row->runtime_call_abi_id == 0
            || row->source_name == NULL || row->runtime_name == NULL
            || argument_count > 2
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
        for (size_t j = 0; j < argument_count; j++) {
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
