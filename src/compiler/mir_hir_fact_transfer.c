#include "mir_hir_fact_transfer.h"

#include "mir.h"

#include "../common/string_compat.h"

#include <stdlib.h>

bool
mir_copy_function_param_flow_summaries(MIRRoutine *routine,
                                       const HIRRoutine *hir_routine,
                                       char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->function_param_flow_summary_count;
    if (count == 0)
        return true;
    if (routine->ast == NULL || routine->ast->type != AST_FUNC_DECL
        || routine->source_syntax_id == 0
        || count > routine->param_count) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR function parameter flow summary has invalid routine identity or parameter count");
        return false;
    }

    routine->function_param_flow_summaries = calloc(
        count, sizeof(*routine->function_param_flow_summaries));
    if (routine->function_param_flow_summaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->function_param_flow_summary_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRFunctionParamFlowSummary *summary =
            &hir_routine->function_param_flow_summaries[i];
        if (summary->parameter_index >= routine->param_count) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR function parameter flow summary has out-of-range parameter index");
            free(routine->function_param_flow_summaries);
            routine->function_param_flow_summaries = NULL;
            routine->function_param_flow_summary_capacity = 0;
            return false;
        }
        for (size_t j = 0; j < routine->function_param_flow_summary_count; j++) {
            if (routine->function_param_flow_summaries[j].parameter_index
                == summary->parameter_index) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR function parameter flow summaries share parameter identity");
                free(routine->function_param_flow_summaries);
                routine->function_param_flow_summaries = NULL;
                routine->function_param_flow_summary_capacity = 0;
                routine->function_param_flow_summary_count = 0;
                return false;
            }
        }
        routine->function_param_flow_summaries[
            routine->function_param_flow_summary_count].parameter_index =
            summary->parameter_index;
        routine->function_param_flow_summaries[
            routine->function_param_flow_summary_count].mask = summary->mask;
        routine->function_param_flow_summary_count++;
    }
    return true;
}

bool
mir_copy_resource_flow_symbols(MIRRoutine *routine,
                               const HIRRoutine *hir_routine,
                               char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->resource_flow_symbol_count;
    if (count == 0)
        return true;
    if (hir_routine->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR ResourceFlowUniverse has incomplete HIR storage");
        return false;
    }
    routine->resource_flow_symbols = calloc(
        count, sizeof(*routine->resource_flow_symbols));
    if (routine->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->resource_flow_symbol_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRResourceFlowSymbol *source =
            &hir_routine->resource_flow_symbols[i];
        MIRResourceFlowSymbol *target = &routine->resource_flow_symbols[i];
        if (source->name == NULL || source->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR ResourceFlowUniverse row has no name");
            goto fail;
        }
        for (size_t j = 0; j < i; j++) {
            const MIRResourceFlowSymbol *prior =
                &routine->resource_flow_symbols[j];
            if (prior->stable_index == source->stable_index
                || (source->is_parameter && prior->is_parameter
                    && prior->parameter_index == source->parameter_index)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR ResourceFlowUniverse rows have duplicate identity");
                goto fail;
            }
        }
        *target = (MIRResourceFlowSymbol){
            .stable_index = source->stable_index,
            .declaration_syntax_id = source->declaration_syntax_id,
            .line = source->line,
            .column = source->column,
            .symbol_kind = source->symbol_kind,
            .is_parameter = source->is_parameter,
            .parameter_index = source->parameter_index,
            .name = pergyra_strdup(source->name)
        };
        if (target->name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
    }
    routine->resource_flow_symbol_count = count;
    return true;

fail:
    for (size_t i = 0; i < count; i++)
        free(routine->resource_flow_symbols[i].name);
    free(routine->resource_flow_symbols);
    routine->resource_flow_symbols = NULL;
    routine->resource_flow_symbol_count = 0;
    routine->resource_flow_symbol_capacity = 0;
    return false;
}

void
mir_free_resource_flow_symbols(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++)
        free(routine->resource_flow_symbols[i].name);
    free(routine->resource_flow_symbols);
    routine->resource_flow_symbols = NULL;
    routine->resource_flow_symbol_count = 0;
    routine->resource_flow_symbol_capacity = 0;
}

static bool
mir_loop_flow_resource_index_known(const MIRRoutine *routine,
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
mir_copy_loop_flow_facts(MIRRoutine *routine,
                         const HIRRoutine *hir_routine,
                         char **error_message)
{
    size_t summary_count;
    size_t state_count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    summary_count = hir_routine->loop_flow_summary_count;
    state_count = hir_routine->loop_flow_state_count;
    if (summary_count == 0) {
        if (state_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary states exist without summaries");
            return false;
        }
        return true;
    }
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->loop_flow_summaries == NULL
        || (state_count > 0 && hir_routine->loop_flow_states == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR LoopFlowSummary has incomplete routine identity or storage");
        return false;
    }

    routine->loop_flow_states = state_count > 0
        ? calloc(state_count, sizeof(*routine->loop_flow_states))
        : NULL;
    routine->loop_flow_summaries = calloc(
        summary_count, sizeof(*routine->loop_flow_summaries));
    if ((state_count > 0 && routine->loop_flow_states == NULL)
        || routine->loop_flow_summaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        free(routine->loop_flow_states);
        free(routine->loop_flow_summaries);
        routine->loop_flow_states = NULL;
        routine->loop_flow_summaries = NULL;
        return false;
    }
    routine->loop_flow_state_capacity = state_count;
    routine->loop_flow_summary_capacity = summary_count;
    for (size_t i = 0; i < state_count; i++) {
        const PgyLoopFlowStateFact *state = &hir_routine->loop_flow_states[i];
        if (!mir_loop_flow_resource_index_known(routine, state->stable_index)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary state references an unknown ResourceFlow index");
            goto fail;
        }
        routine->loop_flow_states[i] = *state;
    }
    for (size_t i = 0; i < summary_count; i++) {
        const PgyLoopFlowSummaryFact *summary =
            &hir_routine->loop_flow_summaries[i];
        if (summary->function_syntax_id != routine->source_syntax_id
            || summary->loop_syntax_id == 0
            || summary->kind > 1u
            || summary->entry_state_start > state_count
            || summary->entry_state_count
                > state_count - summary->entry_state_start
            || summary->exit_state_start > state_count
            || summary->exit_state_count
                > state_count - summary->exit_state_start) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary has an invalid identity or state range");
            goto fail;
        }
        routine->loop_flow_summaries[i] = *summary;
    }
    routine->loop_flow_state_count = state_count;
    routine->loop_flow_summary_count = summary_count;
    return true;

fail:
    free(routine->loop_flow_states);
    free(routine->loop_flow_summaries);
    routine->loop_flow_states = NULL;
    routine->loop_flow_summaries = NULL;
    routine->loop_flow_state_capacity = 0;
    routine->loop_flow_summary_capacity = 0;
    routine->loop_flow_state_count = 0;
    routine->loop_flow_summary_count = 0;
    return false;
}

bool
mir_copy_iteration_type_facts(MIRRoutine *routine,
                              const HIRRoutine *hir_routine,
                              char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->iteration_type_fact_count;
    if (count == 0)
        return true;
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->iteration_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR iteration type facts have incomplete routine identity or storage");
        return false;
    }
    routine->iteration_type_facts = calloc(
        count, sizeof(*routine->iteration_type_facts));
    if (routine->iteration_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->iteration_type_fact_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRIterationTypeFact *source =
            &hir_routine->iteration_type_facts[i];
        MIRIterationTypeFact *target = &routine->iteration_type_facts[i];
        if (source->function_syntax_id != routine->source_syntax_id
            || source->iteration_syntax_id == 0
            || source->binding_type_name == NULL
            || source->iterable_type_name == NULL
            || source->binding_type_name[0] == '\0'
            || source->iterable_type_name[0] == '\0'
            || mir_routine_iteration_type_fact(routine,
                                               source->iteration_syntax_id)
                != NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR iteration type facts have invalid or duplicate identity");
            goto fail;
        }
        target->function_syntax_id = source->function_syntax_id;
        target->iteration_syntax_id = source->iteration_syntax_id;
        target->binding_type_name = pergyra_strdup(source->binding_type_name);
        target->iterable_type_name = pergyra_strdup(source->iterable_type_name);
        target->collection_hoisted = source->collection_hoisted;
        if (target->binding_type_name == NULL
            || target->iterable_type_name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
        routine->iteration_type_fact_count++;
    }
    return true;

fail:
    for (size_t i = 0; i < count; i++) {
        free(routine->iteration_type_facts[i].binding_type_name);
        free(routine->iteration_type_facts[i].iterable_type_name);
    }
    free(routine->iteration_type_facts);
    routine->iteration_type_facts = NULL;
    routine->iteration_type_fact_count = 0;
    routine->iteration_type_fact_capacity = 0;
    return false;
}

void
mir_free_iteration_type_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->iteration_type_fact_count; i++) {
        free(routine->iteration_type_facts[i].binding_type_name);
        free(routine->iteration_type_facts[i].iterable_type_name);
    }
    free(routine->iteration_type_facts);
    routine->iteration_type_facts = NULL;
    routine->iteration_type_fact_count = 0;
    routine->iteration_type_fact_capacity = 0;
}
