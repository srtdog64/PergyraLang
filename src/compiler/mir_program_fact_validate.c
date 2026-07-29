#include "mir_program_fact_validate.h"

#include <string.h>

#include "mir_base_helpers.h"
#include "mir_public_surface.h"

bool
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

bool
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

bool
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

bool
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

bool
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

bool
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

bool
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
