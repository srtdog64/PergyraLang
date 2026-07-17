#include <stdint.h>
#include <stdlib.h>

#include "air_internal.h"

/* This fingerprint is an AIR/MIR binding token, not a cryptographic digest.
 * It covers the stable routine and instruction identity plus every MIR fact
 * that the AIR evidence pass currently consumes.  AIR copies the resulting
 * evidence and retains the token so a later planner cannot silently pair it
 * with a different MIR inventory. */
#define AIR_MIR_BINDING_FNV_OFFSET UINT64_C(1469598103934665603)
#define AIR_MIR_BINDING_FNV_PRIME  UINT64_C(1099511628211)

static void
air_mir_binding_mix_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= AIR_MIR_BINDING_FNV_PRIME;
}

static void
air_mir_binding_mix_u64(uint64_t *hash, uint64_t value)
{
    for (size_t i = 0; i < sizeof(value); i++) {
        air_mir_binding_mix_byte(hash, (unsigned char)(value & UINT64_C(0xff)));
        value >>= 8;
    }
}

static void
air_mir_binding_mix_bool(uint64_t *hash, bool value)
{
    air_mir_binding_mix_byte(hash, value ? 1u : 0u);
}

static void
air_mir_binding_mix_text(uint64_t *hash, const char *text)
{
    if (text == NULL) {
        air_mir_binding_mix_byte(hash, 0u);
        return;
    }
    air_mir_binding_mix_byte(hash, 1u);
    while (*text != '\0')
        air_mir_binding_mix_byte(hash, (unsigned char)*text++);
    air_mir_binding_mix_byte(hash, 0u);
}

uint64_t
air_mir_evidence_binding_fingerprint(const MIRProgram *mir)
{
    uint64_t hash = AIR_MIR_BINDING_FNV_OFFSET;
    MIRRoutineInventory inventory;

    if (mir == NULL)
        return 0;

    air_mir_binding_mix_text(&hash, "pgy.air.mir-binding.v1");
    air_mir_binding_mix_u64(&hash, mir->routine_count);
    air_mir_binding_mix_bool(&hash, mir->has_resource_flow_facts);
    air_mir_binding_mix_bool(&hash, mir->has_function_param_flow_facts);
    air_mir_binding_mix_bool(&hash, mir->has_loop_flow_facts);
    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            air_mir_binding_mix_byte(&hash, 0xffu);
            continue;
        }
        air_mir_binding_mix_text(&hash, routine->name);
        air_mir_binding_mix_text(&hash, routine->owner_name);
        air_mir_binding_mix_u64(&hash, routine->source_syntax_id);
        air_mir_binding_mix_u64(&hash, routine->owner_ast_type);
        air_mir_binding_mix_bool(&hash, routine->has_signature);
        air_mir_binding_mix_u64(&hash, routine->param_count);
        air_mir_binding_mix_u64(&hash,
                                routine->function_param_flow_summary_count);
        if (routine->function_param_flow_summary_count > 0
            && routine->function_param_flow_summaries == NULL) {
            air_mir_binding_mix_byte(&hash, 0xfeu);
        }
        for (size_t j = 0;
             routine->function_param_flow_summaries != NULL
             && j < routine->function_param_flow_summary_count;
             j++) {
            const MIRFunctionParamFlowSummary *row =
                &routine->function_param_flow_summaries[j];
            air_mir_binding_mix_u64(&hash, row->parameter_index);
            air_mir_binding_mix_u64(&hash, row->mask);
        }
        air_mir_binding_mix_u64(&hash, routine->block_count);
        if (routine->block_count > 0 && routine->blocks == NULL) {
            air_mir_binding_mix_byte(&hash, 0xfdu);
            continue;
        }
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            if (block == NULL) {
                air_mir_binding_mix_byte(&hash, 0xfcu);
                continue;
            }
            air_mir_binding_mix_u64(&hash, block->id);
            air_mir_binding_mix_bool(&hash, block->is_cleanup);
            air_mir_binding_mix_bool(&hash, block->is_pin_region);
            air_mir_binding_mix_u64(&hash, block->instruction_count);
            if (block->instruction_count > 0
                && block->instructions == NULL) {
                air_mir_binding_mix_byte(&hash, 0xfbu);
                continue;
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *instruction = &block->instructions[k];
                air_mir_binding_mix_u64(&hash, instruction->id);
                air_mir_binding_mix_u64(&hash, instruction->kind);
                air_mir_binding_mix_u64(&hash, instruction->source_stable_id);
                air_mir_binding_mix_bool(&hash,
                                         instruction->machine_layer_fact_required);
                air_mir_binding_mix_bool(&hash,
                                         instruction->machine_layer_fact_present);
                air_mir_binding_mix_text(
                    &hash, instruction->machine_layer_manifest_id);
                air_mir_binding_mix_text(
                    &hash, instruction->machine_layer_physical_grant_id);
                air_mir_binding_mix_u64(&hash,
                                        instruction->machine_layer_physical_base);
                air_mir_binding_mix_u64(&hash,
                                        instruction->machine_layer_physical_size);
                air_mir_binding_mix_text(
                    &hash, instruction->machine_layer_physical_mode);
                air_mir_binding_mix_text(
                    &hash, instruction->machine_layer_runtime_operation);
                air_mir_binding_mix_bool(&hash,
                                         instruction->machine_layer_hardware_adequate);
                air_mir_binding_mix_bool(&hash,
                                         instruction->machine_layer_authority_required);
                air_mir_binding_mix_bool(&hash,
                                         instruction->machine_layer_live_lease_required);
                air_mir_binding_mix_bool(&hash,
                                         instruction->has_lifecycle_guard_fact);
                air_mir_binding_mix_u64(&hash, instruction->lifecycle_guard_kind);
                air_mir_binding_mix_u64(&hash, instruction->lifecycle_valid_mask);
                air_mir_binding_mix_u64(&hash, instruction->lifecycle_to_state);
                air_mir_binding_mix_text(&hash, instruction->lifecycle_receiver_name);
                air_mir_binding_mix_text(&hash, instruction->lifecycle_op);
                air_mir_binding_mix_text(&hash, instruction->lifecycle_subject);
            }
        }
    }
    return hash != 0 ? hash : UINT64_C(1);
}

static bool
air_function_param_flow_row_exists(const AIRProgram *air,
                                    uint32_t source_syntax_id,
                                    size_t parameter_index)
{
    for (size_t i = 0; i < air->function_param_flow_summary_count; i++) {
        const AIRFunctionParamFlowSummary *row =
            &air->function_param_flow_summaries[i];
        if (row != NULL
            && row->source_syntax_id == source_syntax_id
            && row->parameter_index == parameter_index) {
            return true;
        }
    }
    return false;
}

bool
air_collect_function_param_flow_summaries(AIRProgram *air,
                                           const MIRRoutine *routine,
                                           const char *routine_name,
                                           char **error_message)
{
    size_t count;

    if (air == NULL || routine == NULL)
        return true;
    count = routine->function_param_flow_summary_count;
    if (count == 0)
        return true;
    if (routine->source_syntax_id == 0 || air_name_is_empty(routine_name)) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow requires stable routine identity");
        return false;
    }
    if (routine->function_param_flow_summaries == NULL) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow rows require storage");
        return false;
    }
    if (routine->function_param_flow_summary_capacity < count) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow rows exceed routine storage capacity");
        return false;
    }
    if (count > mir_routine_param_count(routine)) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow row count exceeds routine parameter count");
        return false;
    }
    if (count > SIZE_MAX - air->function_param_flow_summary_count) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow row count overflow");
        return false;
    }
    if (air->function_param_flow_summary_count + count
            > air->function_param_flow_summary_capacity) {
        size_t required = air->function_param_flow_summary_count + count;
        size_t capacity = air->function_param_flow_summary_capacity;
        while (capacity < required) {
            if (!air_next_capacity(&capacity, 8,
                                   sizeof(AIRFunctionParamFlowSummary))) {
                air_set_error(error_message,
                              "AIR MIR function parameter flow row allocation overflow");
                return false;
            }
        }
        AIRFunctionParamFlowSummary *rows = (AIRFunctionParamFlowSummary *)
            realloc(air->function_param_flow_summaries,
                    capacity * sizeof(AIRFunctionParamFlowSummary));
        if (rows == NULL) {
            air_set_error(error_message,
                          "AIR MIR function parameter flow row allocation failed");
            return false;
        }
        air->function_param_flow_summaries = rows;
        air->function_param_flow_summary_capacity = capacity;
    }

    for (size_t i = 0; i < count; i++) {
        const MIRFunctionParamFlowSummary *source =
            &routine->function_param_flow_summaries[i];
        if (source == NULL
            || source->parameter_index >= mir_routine_param_count(routine)) {
            air_set_error(error_message,
                          "AIR MIR function parameter flow row %zu has invalid parameter index",
                          i);
            return false;
        }
        if (air_function_param_flow_row_exists(
                air, routine->source_syntax_id, source->parameter_index)) {
            air_set_error(error_message,
                          "AIR MIR function parameter flow has duplicate stable identity row");
            return false;
        }
        AIRFunctionParamFlowSummary *target =
            &air->function_param_flow_summaries[
                air->function_param_flow_summary_count++];
        target->source_syntax_id = routine->source_syntax_id;
        target->routine = air_program_owned_name(air, routine_name);
        target->parameter_index = source->parameter_index;
        target->parameter_count = mir_routine_param_count(routine);
        target->mask = source->mask;
        if (target->routine == NULL) {
            air_set_error(error_message,
                          "AIR MIR function parameter flow routine name allocation failed");
            return false;
        }
    }
    return true;
}

static bool
air_collect_mir_requires_routine_provider(const MIRRoutine *routine,
                                          char **error_message)
{
    if (air_mir_routine_provider_name(routine) != NULL)
        return true;
    air_set_error(error_message,
                  "AIR MIR evidence requires routine name or owner provenance");
    return false;
}

static bool
air_collect_mir_requires_routine_inventory(const MIRRoutine *routine,
                                           size_t index,
                                           char **error_message)
{
    if (routine == NULL) {
        air_set_error(error_message,
                      "AIR MIR evidence has invalid routine inventory row[%zu]",
                      index);
        return false;
    }
    if (routine->block_count > 0 && routine->blocks == NULL) {
        air_set_error(error_message,
                      "AIR MIR evidence requires block inventory for routine '%s'",
                      routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    return true;
}

static bool
air_boundary_has_resource_capture_evidence(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && (boundary->has_rir_raw_slot_capture_evidence
            || boundary->has_rir_live_view_capture_evidence
            || boundary->has_rir_raw_channel_capture_evidence
            || boundary->has_rir_zone_pin_evidence
            || boundary->has_mir_pin_cleanup_evidence);
}

static void
air_collect_mir_value_capture_evidence(AIRProgram *air)
{
    if (air == NULL)
        return;

    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        if (boundary == NULL)
            continue;
        if (boundary->kind != AIR_BOUNDARY_PARALLEL)
            continue;
        if (!boundary->has_rir_movability_requirement_evidence)
            continue;
        if (air_boundary_has_resource_capture_evidence(boundary))
            continue;
        boundary->has_mir_value_capture_evidence = true;
    }
}

bool
air_collect_mir_evidence(AIRProgram *air,
                         const MIRProgram *mir,
                         char **error_message)
{
    uint64_t binding_fingerprint;

    if (air == NULL || mir == NULL)
        return true;

    if (air->mir_evidence_collection_started) {
        air_set_error(error_message,
                      "AIR MIR evidence is already anchored and cannot be rebound");
        return false;
    }
    binding_fingerprint = air_mir_evidence_binding_fingerprint(mir);
    if (binding_fingerprint == 0) {
        air_set_error(error_message,
                      "AIR MIR evidence binding fingerprint could not be issued");
        return false;
    }
    /* Mark the one-shot boundary before consuming any rows.  A malformed MIR
       input therefore cannot be retried against the same AIR object with a
       partially retained evidence inventory. */
    air->mir_evidence_collection_started = true;

    air_mark_mir_input(air);
    air->has_function_param_flow_facts = mir->has_function_param_flow_facts;
    air->function_param_flow_summary_count = 0;

    MIRRoutineInventory inventory;
    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (!air_collect_mir_requires_routine_inventory(routine, i,
                                                        error_message)) {
            return false;
        }
        const char *routine_name = air_mir_routine_provider_name(routine);
        if (!air_collect_function_param_flow_summaries(
                air, routine, routine_name, error_message)) {
            return false;
        }
        /* Bucket C: accumulate this routine's unproven retains (lifecycle CHECK
           guards at ambiguous joins) into the program total. */
        if (!air_add_unproven_retain_count(
                air, air_mir_routine_unproven_retain_fact_count(routine))) {
            air_set_error(error_message,
                          "AIR MIR unproven retain counter overflow");
            return false;
        }
        /* Bucket A: accumulate inherent concurrency retains (parallel/channel). */
        if (!air_add_inherent_concurrency_retain_count(
                air,
                air_mir_routine_inherent_concurrency_fact_count(routine))) {
            air_set_error(error_message,
                          "AIR MIR inherent concurrency retain counter overflow");
            return false;
        }
        /* Bucket B: capability-bearing slot operations retained by policy. */
        if (!air_add_slot_capability_retain_count(
                air,
                air_mir_routine_slot_capability_retain_fact_count(routine))) {
            air_set_error(error_message,
                          "AIR MIR slot capability retain counter overflow");
            return false;
        }
        /* ...and capture their identity sites so AIR owns slot identity. */
        if (!air_collect_slot_sites(air, routine, routine_name)) {
            air_set_error(error_message,
                          "AIR MIR evidence failed to collect slot identity sites");
            return false;
        }
        if (!air_collect_machine_layer_sites(air, routine, routine_name)) {
            air_set_error(error_message,
                          "AIR MIR evidence failed to collect machine-layer sites");
            return false;
        }
        /* ...and the per-operation effect sites (gated builtin -> capability). */
        if (!air_collect_effect_sites(air, routine, routine_name)) {
            air_set_error(error_message,
                          "AIR MIR evidence failed to collect effect sites");
            return false;
        }
        size_t cleanup_fact_count = air_mir_routine_cleanup_fact_count(routine);
        size_t terminator_fact_count =
            air_mir_routine_terminator_fact_count(routine);
        size_t select_receive_fact_count =
            air_mir_routine_select_receive_fact_count(routine);
        AIREvidenceKind cleanup_kind = air_mir_cleanup_evidence_kind();
        AIREvidenceKind terminator_kind = air_mir_terminator_evidence_kind();
        AIREvidenceKind select_receive_kind =
            air_mir_select_receive_evidence_kind();
        bool had_terminator_evidence;
        bool had_select_receive_evidence;
        bool had_cleanup_evidence;

        if ((terminator_fact_count > 0
             || select_receive_fact_count > 0
             || cleanup_fact_count > 0)
            && !air_collect_mir_requires_routine_provider(routine,
                                                          error_message)) {
            return false;
        }
        if (terminator_fact_count > 0) {
            had_terminator_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    terminator_kind,
                    routine_name,
                    "cfg-terminator");
            if (!air_append_evidence_node_ex(air,
                                             terminator_kind,
                                             SIZE_MAX,
                                             routine_name,
                                             "cfg-terminator",
                                             terminator_fact_count,
                                             0,
                                             error_message)) {
                return false;
            }
            if (!had_terminator_evidence
                && !air_increment_evidence_summary_count(
                    air,
                    terminator_kind)) {
                air_set_error(error_message,
                              "AIR MIR terminator evidence counter overflow");
                return false;
            }
        }
        if (select_receive_fact_count > 0) {
            had_select_receive_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    select_receive_kind,
                    routine_name,
                    "select-receive");
            if (!air_append_evidence_node_ex(air,
                                             select_receive_kind,
                                             SIZE_MAX,
                                             routine_name,
                                             "select-receive",
                                             select_receive_fact_count,
                                             0,
                                             error_message)) {
                return false;
            }
            if (!had_select_receive_evidence
                && !air_increment_evidence_summary_count(
                    air,
                    select_receive_kind)) {
                air_set_error(error_message,
                              "AIR MIR select receive evidence counter overflow");
                return false;
            }
        }
        if (cleanup_fact_count > 0) {
            had_cleanup_evidence = air_has_global_evidence_provider_subject(
                air,
                cleanup_kind,
                routine_name,
                "cleanup-block");
            if (!air_append_evidence_node_ex(air,
                                             cleanup_kind,
                                             SIZE_MAX,
                                             routine_name,
                                             "cleanup-block",
                                             cleanup_fact_count,
                                             0,
                                             error_message)) {
                return false;
            }
            if (!had_cleanup_evidence
                && !air_increment_evidence_summary_count(
                    air,
                    cleanup_kind)) {
                air_set_error(error_message,
                              "AIR MIR cleanup evidence counter overflow");
                return false;
            }
        }

        /* The MIR inventory is the sole cross-stage owner for these facts.
         * Consume pin-block evidence in this same routine pass so AIR never
         * reopens the inventory as a second evidence source. */
        for (size_t j = 0; j < routine->block_count; j++) {
            if (!air_collect_mir_pin_block_evidence(air, routine,
                                                    &routine->blocks[j],
                                                    routine_name,
                                                    error_message)) {
                return false;
            }
        }
    }

    if (air->has_function_param_flow_facts
        != (air->function_param_flow_summary_count > 0)) {
        air_set_error(error_message,
                      "AIR MIR function parameter flow presence flag does not match rows");
        return false;
    }

    air_collect_mir_value_capture_evidence(air);
    air_refresh_execution_lane_facts(air);
    air->mir_evidence_binding_fingerprint = binding_fingerprint;
    air->mir_evidence_bound = true;
    return true;
}
