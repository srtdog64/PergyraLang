#include "mir.h"

#include <string.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_fact_validate.h"
#include "mir_machine_layer.h"
#include "mir_domain_topology.h"
#include "mir_public_surface.h"
#include "mir_parallel_capture_facts.h"
#include "mir_region_escape_facts.h"
#include "mir_generic_method_specialization.h"
#include "mir_validation.h"

static bool
mir_validate_non_cfg_fallback_state(const MIRRoutine *routine,
                                    char **error_message)
{
    if (routine == NULL)
        return true;
    if (routine->non_cfg_body_fallback_count > 0
        && !routine->used_non_cfg_body_fallback) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' records non-CFG body fallback count without fallback flag",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->used_non_cfg_body_fallback
        && routine->non_cfg_body_fallback_count == 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' records non-CFG body fallback flag without fallback count",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->hir_routine != NULL
        && routine->hir_routine->has_cfg
        && routine->used_non_cfg_body_fallback) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' is CFG-backed but used non-CFG body fallback",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    return true;
}

static bool
mir_validate_function_param_flow_summaries(const MIRRoutine *routine,
                                           char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->function_param_flow_summary_count == 0)
        return routine->function_param_flow_summaries == NULL
            || routine->function_param_flow_summary_capacity != 0;
    if (routine->function_param_flow_summaries == NULL
        || routine->function_param_flow_summary_count
            > routine->function_param_flow_summary_capacity
        || routine->ast == NULL
        || routine->ast->type != AST_FUNC_DECL
        || routine->source_syntax_id == 0
        || (routine->hir_routine != NULL
            && routine->hir_routine->source_syntax_id
                != routine->source_syntax_id)) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has incomplete function parameter flow summary identity",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->function_param_flow_summary_count > routine->param_count) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has too many function parameter flow summaries",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    for (size_t i = 0; i < routine->function_param_flow_summary_count; i++) {
        const MIRFunctionParamFlowSummary *summary =
            &routine->function_param_flow_summaries[i];
        if (summary->parameter_index >= routine->param_count) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' function parameter flow summary index %zu is out of range",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->function_param_flow_summaries[j].parameter_index
                == summary->parameter_index) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' function parameter flow summaries share index %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                return false;
            }
        }
    }
    return true;
}

static bool
mir_validate_resource_flow_symbols(const MIRRoutine *routine,
                                   char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->resource_flow_symbol_count == 0)
        return routine->resource_flow_symbols == NULL
            || routine->resource_flow_symbol_capacity != 0;
    if (routine->resource_flow_symbols == NULL
        || routine->resource_flow_symbol_count
            > routine->resource_flow_symbol_capacity) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has incomplete resource-flow symbol storage",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++) {
        const MIRResourceFlowSymbol *symbol =
            &routine->resource_flow_symbols[i];
        if (symbol->name == NULL || symbol->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' resource-flow symbol[%zu] has no name",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const MIRResourceFlowSymbol *prior =
                &routine->resource_flow_symbols[j];
            if (prior->stable_index == symbol->stable_index
                || (symbol->is_parameter && prior->is_parameter
                    && prior->parameter_index == symbol->parameter_index)) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' resource-flow rows share identity",
                        routine->name != NULL ? routine->name : "(anonymous)");
                return false;
            }
        }
    }
    return true;
}

static bool
mir_validate_loop_flow_resource_index(const MIRRoutine *routine,
                                      size_t stable_index)
{
    if (routine == NULL)
        return false;
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++) {
        if (routine->resource_flow_symbols[i].stable_index == stable_index)
            return true;
    }
    return false;
}

static bool
mir_validate_loop_flow_facts(const MIRRoutine *routine,
                             char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->loop_flow_summary_count == 0) {
        if (routine->loop_flow_state_count != 0) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has loop-flow states without summaries",
                    routine->name != NULL ? routine->name : "(anonymous)");
            return false;
        }
        return routine->loop_flow_summaries == NULL
            || routine->loop_flow_summary_capacity == 0;
    }
    if (routine->loop_flow_summaries == NULL
        || (routine->loop_flow_state_count > 0
            && routine->loop_flow_states == NULL)
        || routine->loop_flow_summary_count
            > routine->loop_flow_summary_capacity
        || routine->loop_flow_state_count > routine->loop_flow_state_capacity
        || routine->source_syntax_id == 0) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has incomplete loop-flow storage or identity",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    for (size_t i = 0; i < routine->loop_flow_summary_count; i++) {
        const PgyLoopFlowSummaryFact *summary =
            &routine->loop_flow_summaries[i];
        if (summary->function_syntax_id != routine->source_syntax_id
            || summary->loop_syntax_id == 0
            || summary->kind > 1u
            || summary->entry_state_start > routine->loop_flow_state_count
            || summary->entry_state_count
                > routine->loop_flow_state_count - summary->entry_state_start
            || summary->exit_state_start > routine->loop_flow_state_count
            || summary->exit_state_count
                > routine->loop_flow_state_count - summary->exit_state_start) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has an invalid loop-flow identity or range",
                    routine->name != NULL ? routine->name : "(anonymous)");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->loop_flow_summaries[j].loop_syntax_id
                == summary->loop_syntax_id) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' has duplicate loop-flow identity %u",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        summary->loop_syntax_id);
                return false;
            }
        }
    }
    for (size_t i = 0; i < routine->loop_flow_state_count; i++) {
        const PgyLoopFlowStateFact *state = &routine->loop_flow_states[i];
        if (!mir_validate_loop_flow_resource_index(routine, state->stable_index)
            || state->slot_state < 0
            || state->semantic_state < 0) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has an invalid loop-flow state identity",
                    routine->name != NULL ? routine->name : "(anonymous)");
            return false;
        }
    }
    return true;
}

static bool
mir_validate_program_inventory_shape(const MIRProgram *mir,
                                     char **error_message)
{
    MIRRoutineInventory inventory;
    if (mir == NULL)
        return true;
    mir_routine_inventory_from_program(mir, &inventory);
    if (inventory.count > 0 && inventory.routines == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program records %zu routine(s) without routine inventory",
                inventory.count);
        }
        return false;
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
        if (routine->block_count > 0 && routine->blocks == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu block(s) without block inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->block_count);
            }
            return false;
        }
        if (routine->value_summary_count > 0
            && routine->value_summaries == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu value summaries without value-summary inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->value_summary_count);
            }
            return false;
        }
        if (routine->source_local_type_count
            > routine->source_local_type_capacity) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu source-local type facts above capacity %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->source_local_type_count,
                    routine->source_local_type_capacity);
            }
            return false;
        }
        if (routine->source_local_type_count > 0
            && routine->source_local_types == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu source-local type facts without source-local type inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->source_local_type_count);
            }
            return false;
        }
        if (routine->has_signature && routine->param_count > 0
            && routine->param_abi_facts == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records parameters without carriage facts",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->has_signature && routine->generic_param_count > 0
            && routine->generic_param_names == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records generic parameters without name facts",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        for (size_t j = 0;
             routine->has_signature && j < routine->generic_param_count; j++) {
            if (routine->generic_param_names[j] == NULL
                || routine->generic_param_names[j][0] == '\0') {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' generic parameter[%zu] has no name fact",
                        routine->name != NULL
                            ? routine->name
                            : "(anonymous)",
                        j);
                }
                return false;
            }
        }
        for (size_t j = 0;
             routine->has_signature && j < routine->param_count; j++) {
            MIRParamCarriage carriage = routine->param_abi_facts[j].carriage;
            MIRParamResourceKind resource_kind =
                routine->param_abi_facts[j].resource_kind;
            MIRParamResourceKind expected_resource_kind =
                mir_param_resource_kind_from_type_name(
                    routine->param_type_names != NULL
                        ? routine->param_type_names[j]
                        : NULL);
            if (carriage < MIR_PARAM_CARRIAGE_VALUE
                || carriage > MIR_PARAM_CARRIAGE_OWNER_HANDLE) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' parameter[%zu] has invalid carriage fact",
                        routine->name != NULL
                            ? routine->name
                            : "(anonymous)",
                        j);
                }
                return false;
            }
            if (resource_kind < MIR_PARAM_RESOURCE_NONE
                || resource_kind > MIR_PARAM_RESOURCE_DEVICE_SLOT
                || resource_kind != expected_resource_kind) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' parameter[%zu] has invalid resource ABI fact",
                        routine->name != NULL
                            ? routine->name
                            : "(anonymous)",
                        j);
                }
                return false;
            }
            if (routine->param_abi_facts[j].pass_indirect
                && carriage != MIR_PARAM_CARRIAGE_READONLY_REF) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' parameter[%zu] has indirect ABI without readonly-ref carriage",
                        routine->name != NULL
                            ? routine->name
                            : "(anonymous)",
                        j);
                }
                return false;
            }
        }
        for (size_t j = 0; j < routine->source_local_type_count; j++) {
            const MIRSourceLocalType *fact = &routine->source_local_types[j];
            if (fact->name == NULL || fact->type_name == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' source-local type fact[%zu] is incomplete",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j);
                }
                return false;
            }
            if (fact->is_callable) {
                if (fact->callable_return_type_name == NULL
                    || (fact->callable_param_count > 0
                        && fact->callable_param_type_names == NULL)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' source-local callable type fact[%zu] is incomplete",
                            routine->name != NULL
                                ? routine->name
                                : "(anonymous)",
                            j);
                    }
                    return false;
                }
                for (size_t k = 0; k < fact->callable_param_count; k++) {
                    if (fact->callable_param_type_names[k] == NULL) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' source-local callable type fact[%zu] has missing parameter metadata",
                                routine->name != NULL
                                    ? routine->name
                                    : "(anonymous)",
                                j);
                        }
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool
mir_validate_non_cfg_fallback_inventory(const MIRProgram *mir,
                                        char **error_message)
{
    size_t fallback_total = 0;
    size_t fallback_routines = 0;

    if (mir == NULL)
        return true;
    if (!mir->has_non_cfg_body_fallback_inventory)
        return true;

    mir_count_non_cfg_body_fallback_inventory(mir,
                                              &fallback_total,
                                              &fallback_routines);
    if (mir->non_cfg_body_fallback_total != fallback_total
        || mir->non_cfg_body_fallback_routine_count != fallback_routines) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program non-CFG fallback inventory is stale (recorded total=%zu routines=%zu, actual total=%zu routines=%zu)",
                mir->non_cfg_body_fallback_total,
                mir->non_cfg_body_fallback_routine_count,
                fallback_total,
                fallback_routines);
        }
        return false;
    }
    return true;
}

static bool
mir_validate_receiver_carriage_facts(const MIRProgram *mir,
                                     char **error_message)
{
    MIRRoutineInventory inventory;

    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        const char *owner_name;
        const MIRDeclHeader *owner = NULL;
        size_t owner_count = 0;
        MIRReceiverCarriage expected;

        if (routine == NULL)
            return false;
        owner_name = routine->owner_name;
        if (routine->kind != MIR_SCOPE_METHOD) {
            if (routine->receiver_carriage != MIR_RECEIVER_CARRIAGE_NONE) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR non-method routine '%s' carries a receiver",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                return false;
            }
            continue;
        }
        if (owner_name == NULL || owner_name[0] == '\0') {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR method '%s' receiver carriage has no declaration owner",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        for (size_t j = 0; j < mir->decl_header_count; j++) {
            const MIRDeclHeader *candidate = &mir->decl_headers[j];
            if (candidate->name != NULL
                && strcmp(candidate->name, owner_name) == 0) {
                owner = candidate;
                owner_count++;
            }
        }
        if (owner_count != 1 || owner == NULL
            || owner->ast_type != routine->owner_ast_type) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' receiver carriage has no unique exact declaration owner '%s'",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    owner_name);
            }
            return false;
        }
        expected = owner->uses_pointer_self
            ? MIR_RECEIVER_CARRIAGE_MUTABLE_IDENTITY
            : MIR_RECEIVER_CARRIAGE_VALUE;
        if (routine->receiver_carriage != expected) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' receiver carriage '%s' disagrees with declaration owner '%s'",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    mir_receiver_carriage_name(routine->receiver_carriage),
                    owner_name);
            }
            return false;
        }
    }
    return true;
}

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

    if (!mir_validate_non_cfg_fallback_inventory(mir, error_message))
        return false;

    return true;
}
