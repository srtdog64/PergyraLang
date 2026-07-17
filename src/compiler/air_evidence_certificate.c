#include "air_evidence_certificate.h"

#include "air_internal.h"

#include <stddef.h>

/* FNV-1a is used only as a deterministic in-process certificate fingerprint.
 * It is an identity/mutation guard, not a cryptographic attestation.  The
 * proof-carrying artifact layer remains responsible for SHA-256 payload
 * digests when a certificate crosses a process or cache boundary. */
#define PGY_AIR_CERTIFICATE_FNV_OFFSET UINT64_C(1469598103934665603)
#define PGY_AIR_CERTIFICATE_FNV_PRIME  UINT64_C(1099511628211)

static void
mix_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= PGY_AIR_CERTIFICATE_FNV_PRIME;
}

static void
mix_u64(uint64_t *hash, uint64_t value)
{
    for (size_t i = 0; i < sizeof(value); i++) {
        mix_byte(hash, (unsigned char)(value & UINT64_C(0xff)));
        value >>= 8;
    }
}

static void
mix_bool(uint64_t *hash, bool value)
{
    mix_byte(hash, value ? 1u : 0u);
}

static void
mix_text(uint64_t *hash, const char *text)
{
    if (text == NULL) {
        mix_byte(hash, 0u);
        return;
    }
    mix_byte(hash, 1u);
    while (*text != '\0')
        mix_byte(hash, (unsigned char)*text++);
    mix_byte(hash, 0u);
}

static void
mix_capture(uint64_t *hash, const BoundaryCaptureFact *capture)
{
    if (capture == NULL) {
        mix_byte(hash, 0u);
        return;
    }
    mix_byte(hash, 1u);
    mix_bool(hash, capture->captures_raw_slot);
    mix_bool(hash, capture->captures_live_view);
    mix_bool(hash, capture->captures_raw_channel);
    mix_bool(hash, capture->captures_pin);
    mix_bool(hash, capture->captures_value_only);
    mix_bool(hash, capture->requires_movability);
    mix_bool(hash, capture->is_deterministic_fork_join);
    mix_bool(hash, capture->is_await_heavy_local);
    mix_bool(hash, capture->has_io_or_ffi_effect);
    mix_bool(hash, capture->is_concurrent_site);
    mix_bool(hash, capture->crosses_authority_boundary);
}

uint64_t
pgy_air_evidence_certificate_fingerprint(const AIRProgram *air)
{
    uint64_t hash = PGY_AIR_CERTIFICATE_FNV_OFFSET;

    if (air == NULL
        || (air->intent_count > 0 && air->intents == NULL)
        || (air->boundary_count > 0 && air->boundaries == NULL)
        || (air->drift_count > 0 && air->drifts == NULL)
        || (air->evidence_count > 0 && air->evidence_nodes == NULL)
        || (air->propagation_requirement_count > 0
            && air->propagation_requirements == NULL)) {
        return 0;
    }

    mix_text(&hash, PGY_AIR_EVIDENCE_CERTIFICATE_SCHEMA);
    mix_bool(&hash, air_requires_strict_evidence(air));
    mix_bool(&hash, air->has_hir_input);
    mix_bool(&hash, air->has_rir_input);
    mix_bool(&hash, air->has_mir_input);
    mix_bool(&hash, air->mir_evidence_collection_started);
    mix_bool(&hash, air->mir_evidence_bound);
    mix_u64(&hash, air->mir_evidence_binding_fingerprint);
    mix_u64(&hash, air->intent_count);
    mix_u64(&hash, air->boundary_count);
    mix_u64(&hash, air->drift_count);
    mix_u64(&hash, air->evidence_count);
    mix_u64(&hash, air->propagation_requirement_count);
    mix_u64(&hash, air->hir_routine_evidence_count);
    mix_u64(&hash, air->hir_cfg_evidence_count);
    mix_u64(&hash, air->rir_boundary_evidence_count);
    mix_u64(&hash, air->rir_authority_evidence_count);
    mix_u64(&hash, air->mir_cleanup_evidence_count);
    mix_u64(&hash, air->mir_pin_cleanup_evidence_count);
    mix_u64(&hash, air->mir_terminator_evidence_count);
    mix_u64(&hash, air->mir_select_receive_evidence_count);
    mix_u64(&hash, air->dag_metadata_evidence_count);
    mix_u64(&hash, air->dag_generic_evidence_count);
    mix_u64(&hash, air->dag_ability_evidence_count);
    mix_u64(&hash, air->rir_effect_propagation_required_count);
    mix_u64(&hash, air->rir_effect_propagation_evidence_count);
    mix_u64(&hash, air->rir_relation_propagation_required_count);
    mix_u64(&hash, air->rir_relation_propagation_evidence_count);
    mix_u64(&hash, air->observability_schema_evidence_count);
    mix_u64(&hash, air->runtime_frontier_policy_evidence_count);
    mix_u64(&hash, air->unproven_retain_count);
    mix_u64(&hash, air->inherent_concurrency_count);
    mix_u64(&hash, air->slot_capability_retain_count);
    mix_u64(&hash, air->program_capabilities);
    mix_u64(&hash, air->slot_site_count);
    mix_u64(&hash, air->machine_layer_site_count);
    mix_u64(&hash, air->effect_site_count);
    mix_u64(&hash, air->function_param_flow_summary_count);
    mix_bool(&hash, air->has_function_param_flow_facts);
    mix_u64(&hash, air->lifecycle_state_space_count);

    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        mix_text(&hash, intent->intent_owner);
        mix_text(&hash, intent->step_name);
        mix_u64(&hash, intent->step_index);
        mix_u64(&hash, intent->sync_class);
        mix_u64(&hash, intent->failure_class);
        mix_text(&hash, intent->compensation_hook);
        mix_bool(&hash, intent->who_from_intent_default);
        mix_bool(&hash, intent->who_from_on_receiver);
        mix_bool(&hash, intent->who_from_single_participant);
        mix_bool(&hash, intent->requires_from_action);
        mix_bool(&hash, intent->causes_from_action);
    }

    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        mix_u64(&hash, boundary->kind);
        mix_text(&hash, boundary->owner_name);
        mix_text(&hash, boundary->source_name);
        mix_u64(&hash, boundary->intent_index);
        mix_u64(&hash, boundary->step_index);
        mix_u64(&hash, boundary->sync_class);
        mix_bool(&hash, boundary->authority_required);
        mix_bool(&hash, boundary->source_from_intent_default);
        mix_bool(&hash, boundary->source_from_action);
        mix_bool(&hash, boundary->source_from_transfer);
        mix_bool(&hash, boundary->authority_from_zone);
        mix_bool(&hash, boundary->authority_from_action);
        mix_u64(&hash, boundary->authority_name_count);
        for (size_t j = 0; j < boundary->authority_name_count; j++)
            mix_text(&hash, boundary->authority_names[j]);
        mix_u64(&hash, boundary->required_ability_count);
        for (size_t j = 0; j < boundary->required_ability_count; j++)
            mix_text(&hash, boundary->required_abilities[j]);
        mix_bool(&hash, boundary->has_hir_routine_evidence);
        mix_bool(&hash, boundary->has_hir_cfg_evidence);
        mix_bool(&hash, boundary->has_rir_boundary_evidence);
        mix_bool(&hash, boundary->has_rir_authority_evidence);
        mix_bool(&hash, boundary->has_mir_pin_cleanup_evidence);
        mix_bool(&hash, boundary->has_rir_await_local_evidence);
        mix_bool(&hash, boundary->has_rir_movability_requirement_evidence);
        mix_bool(&hash, boundary->has_rir_deterministic_fork_join_evidence);
        mix_bool(&hash, boundary->has_rir_zone_pin_evidence);
        mix_bool(&hash, boundary->has_rir_live_view_capture_evidence);
        mix_bool(&hash, boundary->has_rir_raw_slot_capture_evidence);
        mix_bool(&hash, boundary->has_rir_raw_channel_capture_evidence);
        mix_bool(&hash, boundary->has_mir_value_capture_evidence);
        mix_text(&hash, boundary->hir_routine_evidence_name);
        mix_text(&hash, boundary->rir_boundary_evidence_scope);
        mix_text(&hash, boundary->rir_authority_evidence_name);
        mix_capture(&hash, &boundary->boundary_capture);
        mix_u64(&hash, boundary->execution_lane);
    }

    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        mix_u64(&hash, air_evidence_node_kind(evidence));
        mix_u64(&hash, air_evidence_node_provider_kind(evidence));
        mix_u64(&hash, air_evidence_node_subject_kind(evidence));
        mix_u64(&hash, air_evidence_node_boundary_index_or(evidence, SIZE_MAX));
        mix_text(&hash, air_evidence_node_provider_name_or(evidence, NULL));
        mix_text(&hash, air_evidence_node_subject_name_or(evidence, NULL));
        mix_bool(&hash, air_evidence_node_has_boundary_shape(evidence));
        mix_u64(&hash,
                air_evidence_node_boundary_kind_or(evidence,
                                                   AIR_BOUNDARY_UNKNOWN));
        mix_text(&hash,
                 air_evidence_node_boundary_owner_name_or(evidence, NULL));
        mix_text(&hash,
                 air_evidence_node_boundary_source_name_or(evidence, NULL));
        mix_u64(&hash, air_evidence_node_fact_count(evidence));
        mix_u64(&hash, air_evidence_node_fallback_count(evidence));
    }

    for (size_t i = 0; i < air->propagation_requirement_count; i++) {
        const AIRPropagationRequirement *requirement =
            &air->propagation_requirements[i];
        mix_u64(&hash, requirement->kind);
        mix_text(&hash, requirement->provider_name);
        mix_text(&hash, requirement->subject_name);
    }

    for (size_t i = 0; i < air->machine_layer_site_count; i++) {
        const AIRMachineLayerSite *site = &air->machine_layer_sites[i];
        mix_text(&hash, site->slot);
        mix_text(&hash, site->operation);
        mix_text(&hash, site->manifest_id);
        mix_text(&hash, site->physical_grant_id);
        mix_u64(&hash, site->physical_base);
        mix_u64(&hash, site->physical_size);
        mix_text(&hash, site->physical_mode);
        mix_text(&hash, site->runtime_operation);
        mix_text(&hash, site->routine);
        mix_bool(&hash, site->hardware_adequate);
        mix_bool(&hash, site->authority_required);
        mix_bool(&hash, site->live_lease_required);
    }

    return hash != 0 ? hash : UINT64_C(1);
}

bool
pgy_air_evidence_certificate_issue(AIRProgram *air, const char **error_out)
{
    uint64_t fingerprint;

    if (error_out != NULL)
        *error_out = NULL;
    if (air == NULL) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: missing AIR program";
        return false;
    }
    air->verification_certificate_valid = false;
    air->verification_certificate_fingerprint = 0;
    if (!air->has_mir_input
        || (air->mir_evidence_collection_started
            && (!air->mir_evidence_bound
                || air->mir_evidence_binding_fingerprint == 0))) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: MIR evidence is missing";
        return false;
    }
    if (air->drift_count != 0) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: AIR drift is not discharged";
        return false;
    }
    fingerprint = pgy_air_evidence_certificate_fingerprint(air);
    if (fingerprint == 0) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: evidence storage is incomplete";
        return false;
    }
    air->verification_certificate_fingerprint = fingerprint;
    air->verification_certificate_valid = true;
    return true;
}

bool
pgy_air_evidence_certificate_ready(const AIRProgram *air, const char **error_out)
{
    uint64_t current;

    if (error_out != NULL)
        *error_out = NULL;
    if (air == NULL) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: missing AIR program";
        return false;
    }
    if (!air->verification_certificate_valid || !air->has_mir_input
        || (air->mir_evidence_collection_started
            && (!air->mir_evidence_bound
                || air->mir_evidence_binding_fingerprint == 0))) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: verified MIR evidence is missing";
        return false;
    }
    current = pgy_air_evidence_certificate_fingerprint(air);
    if (current == 0 || current != air->verification_certificate_fingerprint) {
        if (error_out != NULL)
            *error_out = "AIR evidence certificate: owner facts changed after verification";
        return false;
    }
    return true;
}
