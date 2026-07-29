#include "mir.h"
#include "mir_program_fact_validate.h"

#include <string.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_fact_validate.h"
#include "mir_machine_layer.h"
#include "mir_domain_topology.h"
#include "mir_domain_runtime.h"
#include "mir_parallel_capture_facts.h"
#include "mir_region_escape_facts.h"
#include "mir_generic_method_specialization.h"
#include "mir_intent_execution.h"
#include "mir_validation.h"

bool
mir_validate(const MIRProgram *mir, char **error_message)
{
    MIRRoutineInventory inventory;
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    if (!mir_validate_decl_header_metadata(mir, error_message))
        return false;
    if ((mir->relation_count != 0 || mir->effect_count != 0
         || mir->zone_count != 0)
        && !mir_domain_topology_validate(mir, error_message)) {
        return false;
    }
    if ((mir->relation_count != 0 || mir->effect_count != 0
         || mir->zone_count != 0)
        && !mir->has_domain_runtime_facts) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR is missing DIR-owned domain runtime assignments");
        return false;
    }
    if (!mir_domain_runtime_validate(mir, error_message))
        return false;
    if (!mir_validate_program_inventory_shape(mir, error_message))
        return false;
    if (!mir_validate_receiver_carriage_facts(mir, error_message))
        return false;
    if (!mir_validate_inventory_surface_usage(mir, error_message))
        return false;
    if (!mir_validate_parallel_capture_facts(mir, error_message))
        return false;
    if (!mir_validate_region_escape_facts(mir, error_message))
        return false;
    if (!mir_generic_method_specializations_validate(mir, error_message))
        return false;

    mir_routine_inventory_from_program(mir, &inventory);
    {
        size_t resource_flow_symbol_total = 0;
        size_t function_param_flow_summary_total = 0;
        size_t loop_flow_summary_total = 0;
        for (size_t i = 0; i < inventory.count; i++) {
            const MIRRoutine *routine =
                mir_routine_inventory_get(&inventory, i);
            if (routine != NULL) {
                resource_flow_symbol_total +=
                    routine->resource_flow_symbol_count;
                function_param_flow_summary_total +=
                    routine->function_param_flow_summary_count;
                loop_flow_summary_total += routine->loop_flow_summary_count;
            }
        }
        if (mir->has_resource_flow_facts
            != (resource_flow_symbol_total > 0)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR ResourceFlowUniverse flag does not match carried rows");
            return false;
        }
        if (mir->has_function_param_flow_facts
            != (function_param_flow_summary_total > 0)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR function parameter flow summary flag does not match carried rows");
            return false;
        }
        if (mir->has_loop_flow_facts != (loop_flow_summary_total > 0)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary flag does not match carried rows");
            return false;
        }
    }
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR program routine inventory row[%zu] is invalid", i);
            }
            return false;
        }

        if (!mir_validate_non_cfg_fallback_state(routine, error_message))
            return false;
        if (!mir_validate_resource_flow_symbols(routine, error_message))
            return false;
        if (!mir_validate_function_param_flow_summaries(routine,
                                                        error_message))
            return false;
        if (!mir_validate_loop_flow_facts(routine, error_message))
            return false;
        if (!mir_validate_intent_execution_plan(routine, error_message))
            return false;
        if (!mir_validate_cfg_contract_state(routine, false, true, true,
                                             error_message)) {
            return false;
        }

        if (routine->block_count > 0 && !routine->has_liveness) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing liveness information",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_use_def_summary) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing use-def summary",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_dce) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing DCE pass state",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }

        for (size_t j = 0; j < routine->value_summary_count; j++) {
            const MIRValueSummary *summary = &routine->value_summaries[j];
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' value summary '%s' is missing slot anchor",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(anonymous)");
                }
                return false;
            }
        }

        if (!mir_validate_routine_emission_facts(routine, error_message))
            return false;

        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];

            if (j == routine->entry_block && block->predecessor_count > 0) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' entry block[%zu] has %zu predecessors (expected 0)",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j,
                        block->predecessor_count);
                }
                return false;
            }

            if (!mir_validate_block_liveness_sets(routine, block, j,
                                                  error_message)) {
                return false;
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                if (inst->kind == MIR_INST_PHI
                    && inst->phi_incoming_count != block->predecessor_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi has %zu incoming edges but %zu predecessors",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            inst->phi_incoming_count,
                            block->predecessor_count);
                    }
                    return false;
                }
                if (inst->kind == MIR_INST_RESOURCE_OP && inst->rir_op == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] RESOURCE_OP has null rir_op",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if (inst->machine_layer_fact_required
                    || (inst->rir_op != NULL
                        && rir_machine_contact_kind_is_present(
                               inst->rir_op->machine_contact_kind))) {
                    if (!mir_machine_layer_fact_is_valid(inst)
                        || (inst->rir_op != NULL
                            && rir_machine_contact_kind_is_present(
                                   inst->rir_op->machine_contact_kind)
                            && inst->machine_contact_kind
                                   != inst->rir_op->machine_contact_kind)) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] instruction[%zu] is missing valid machine-layer fact",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j,
                                k);
                        }
                        return false;
                    }
                }
                if ((inst->kind == MIR_INST_RESOURCE_OP
                     || (inst->kind == MIR_INST_CLEANUP_EDGE && inst->rir_op != NULL))
                    && inst->slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] is missing slot anchor",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if (inst->rir_op != NULL
                    && inst->slot_anchor != NULL
                    && inst->rir_op->slot_anchor != NULL
                    && strcmp(inst->slot_anchor, inst->rir_op->slot_anchor) != 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] slot anchor '%s' diverges from RIR '%s'",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k,
                            inst->slot_anchor,
                            inst->rir_op->slot_anchor);
                    }
                    return false;
                }
            }
            if (!mir_validate_instruction_uses(routine, block, j, error_message))
                return false;
        }
    }

    if (!mir_validate_intent_execution_program(mir, error_message))
        return false;

    if (!mir_validate_non_cfg_fallback_inventory(mir, error_message))
        return false;

    return true;
}
