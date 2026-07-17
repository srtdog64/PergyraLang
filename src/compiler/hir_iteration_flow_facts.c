#include "hir.h"

#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_fact_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    if (*capacity == 0) {
        next = initial;
    } else {
        if (*capacity > SIZE_MAX / 2)
            return false;
        next = *capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static bool
hir_append_loop_state(HIRRoutine *routine,
                      const PgyLoopFlowStateFact *state)
{
    size_t next_capacity;
    HIRLoopFlowStateFact *grown;

    if (routine == NULL || state == NULL)
        return false;
    if (routine->loop_flow_state_count
        == routine->loop_flow_state_capacity) {
        next_capacity = routine->loop_flow_state_capacity;
        if (!hir_fact_next_capacity(&next_capacity, 8,
                                    sizeof(HIRLoopFlowStateFact)))
            return false;
        grown = realloc(routine->loop_flow_states,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->loop_flow_states = grown;
        routine->loop_flow_state_capacity = next_capacity;
    }
    routine->loop_flow_states[routine->loop_flow_state_count++] = *state;
    return true;
}

bool
hir_attach_loop_flow_facts(HIRProgram *hir,
                           const PgyLoopFlowSummaryFact *facts,
                           size_t fact_count,
                           const PgyLoopFlowStateFact *states,
                           size_t state_count,
                           char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL || (facts == NULL && fact_count != 0)
        || (states == NULL && state_count != 0))
        return false;
    if (state_count != 0 && fact_count == 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "LoopFlowSummary state facts have no summary owner");
        return false;
    }
    for (size_t i = 0; i < fact_count; i++) {
        const PgyLoopFlowSummaryFact *fact = &facts[i];
        HIRRoutine *routine = NULL;
        size_t entry_start;
        size_t exit_start;
        size_t local_entry_start;
        size_t local_exit_start;

        if (fact->entry_state_start > state_count
            || fact->entry_state_count > state_count - fact->entry_state_start
            || fact->exit_state_start > state_count
            || fact->exit_state_count > state_count - fact->exit_state_start
            || fact->kind > 1u) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "LoopFlowSummary fact has an invalid state range or loop kind");
            return false;
        }
        for (size_t r = 0; r < hir->routine_count; r++) {
            if (hir->routines[r].source_syntax_id == fact->function_syntax_id) {
                routine = &hir->routines[r];
                break;
            }
        }
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "LoopFlowSummary fact references an unknown HIR routine");
            return false;
        }
        entry_start = fact->entry_state_start;
        exit_start = fact->exit_state_start;
        local_entry_start = routine->loop_flow_state_count;
        for (size_t s = 0; s < fact->entry_state_count; s++) {
            if (!hir_append_loop_state(routine, &states[entry_start + s]))
                goto oom;
        }
        local_exit_start = routine->loop_flow_state_count;
        for (size_t s = 0; s < fact->exit_state_count; s++) {
            if (!hir_append_loop_state(routine, &states[exit_start + s]))
                goto oom;
        }
        if (routine->loop_flow_summary_count
            == routine->loop_flow_summary_capacity) {
            size_t next_capacity = routine->loop_flow_summary_capacity;
            HIRLoopFlowSummaryFact *grown;
            if (!hir_fact_next_capacity(&next_capacity, 4,
                                        sizeof(HIRLoopFlowSummaryFact)))
                goto oom;
            grown = realloc(routine->loop_flow_summaries,
                            next_capacity * sizeof(*grown));
            if (grown == NULL)
                goto oom;
            routine->loop_flow_summaries = grown;
            routine->loop_flow_summary_capacity = next_capacity;
        }
        HIRLoopFlowSummaryFact *copy =
            &routine->loop_flow_summaries[routine->loop_flow_summary_count++];
        *copy = *fact;
        copy->function_syntax_id = routine->source_syntax_id;
        copy->entry_state_start = local_entry_start;
        copy->exit_state_start = local_exit_start;
    }
    hir->has_loop_flow_facts = fact_count != 0;
    return true;

oom:
    if (error_message != NULL && *error_message == NULL)
        *error_message = pergyra_strdup(
            "Out of memory while attaching loop-flow facts");
    return false;
}

static bool
hir_append_iteration_type_fact(HIRRoutine *routine,
                               const PgyIterationTypeFact *fact,
                               char **error_message)
{
    HIRIterationTypeFact *grown;
    size_t next_capacity;

    if (routine == NULL || fact == NULL || fact->iteration_syntax_id == 0
        || fact->binding_type_name == NULL
        || fact->iterable_type_name == NULL)
        return false;
    for (size_t i = 0; i < routine->iteration_type_fact_count; i++) {
        if (routine->iteration_type_facts[i].iteration_syntax_id
            == fact->iteration_syntax_id) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "duplicate HIR iteration type fact identity");
            return false;
        }
    }
    if (routine->iteration_type_fact_count
        == routine->iteration_type_fact_capacity) {
        next_capacity = routine->iteration_type_fact_capacity;
        if (!hir_fact_next_capacity(&next_capacity, 8,
                                    sizeof(HIRIterationTypeFact)))
            return false;
        grown = realloc(routine->iteration_type_facts,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->iteration_type_facts = grown;
        routine->iteration_type_fact_capacity = next_capacity;
    }
    HIRIterationTypeFact *copy =
        &routine->iteration_type_facts[routine->iteration_type_fact_count];
    memset(copy, 0, sizeof(*copy));
    copy->function_syntax_id = routine->source_syntax_id;
    copy->iteration_syntax_id = fact->iteration_syntax_id;
    copy->binding_type_name = pergyra_strdup(fact->binding_type_name);
    copy->iterable_type_name = pergyra_strdup(fact->iterable_type_name);
    copy->collection_hoisted = fact->collection_hoisted;
    if (copy->binding_type_name == NULL || copy->iterable_type_name == NULL) {
        free(copy->binding_type_name);
        free(copy->iterable_type_name);
        copy->binding_type_name = NULL;
        copy->iterable_type_name = NULL;
        return false;
    }
    routine->iteration_type_fact_count++;
    return true;
}

bool
hir_attach_iteration_type_facts(HIRProgram *hir,
                                const PgyIterationTypeFact *facts,
                                size_t fact_count,
                                char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL || (facts == NULL && fact_count != 0))
        return false;
    for (size_t i = 0; i < fact_count; i++) {
        HIRRoutine *routine = NULL;
        for (size_t r = 0; r < hir->routine_count; r++) {
            if (hir->routines[r].source_syntax_id
                == facts[i].function_syntax_id) {
                routine = &hir->routines[r];
                break;
            }
        }
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Iteration type fact references an unknown HIR routine");
            return false;
        }
        if (!hir_append_iteration_type_fact(routine, &facts[i],
                                            error_message)) {
            if (error_message != NULL && *error_message == NULL)
                *error_message = pergyra_strdup(
                    "Out of memory while attaching iteration type facts");
            return false;
        }
    }
    hir->has_iteration_type_facts = fact_count != 0;
    return true;
}
