#include "hir.h"

#include "../common/string_compat.h"
#include "../semantic/semantic.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_append_match_binding_type_fact(HIRRoutine *routine,
                                   const PgyMatchBindingTypeFact *fact,
                                   char **error_message)
{
    size_t next_capacity;
    HIRMatchBindingTypeFact *grown;
    HIRMatchBindingTypeFact *copy;

    if (routine == NULL || fact == NULL
        || fact->function_syntax_id != routine->source_syntax_id
        || fact->match_case_syntax_id == 0 || fact->binding_count == 0
        || fact->binding_index >= fact->binding_count
        || fact->binding_type_name == NULL
        || fact->binding_type_name[0] == '\0')
        return false;
    for (size_t i = 0; i < routine->match_binding_type_fact_count; i++) {
        const HIRMatchBindingTypeFact *existing =
            &routine->match_binding_type_facts[i];
        if (existing->match_case_syntax_id == fact->match_case_syntax_id
            && existing->binding_index == fact->binding_index) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "duplicate HIR match binding type fact identity");
            return false;
        }
    }
    if (routine->match_binding_type_fact_count
        == routine->match_binding_type_fact_capacity) {
        next_capacity = routine->match_binding_type_fact_capacity == 0
            ? 8
            : routine->match_binding_type_fact_capacity * 2;
        if (next_capacity < routine->match_binding_type_fact_capacity
            || next_capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(routine->match_binding_type_facts,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->match_binding_type_facts = grown;
        routine->match_binding_type_fact_capacity = next_capacity;
    }
    copy = &routine->match_binding_type_facts[
        routine->match_binding_type_fact_count];
    *copy = *fact;
    copy->binding_type_name = pergyra_strdup(fact->binding_type_name);
    if (copy->binding_type_name == NULL)
        return false;
    routine->match_binding_type_fact_count++;
    return true;
}

bool
hir_attach_match_binding_type_facts(HIRProgram *hir,
                                    const PgyMatchBindingTypeFact *facts,
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
                    "Match binding type fact references an unknown HIR routine");
            return false;
        }
        if (!hir_append_match_binding_type_fact(
                routine, &facts[i], error_message)) {
            if (error_message != NULL && *error_message == NULL)
                *error_message = pergyra_strdup(
                    "Invalid or unallocatable HIR match binding type fact");
            return false;
        }
    }
    hir->has_match_binding_type_facts = fact_count != 0;
    return true;
}

HIRProgram *
hir_lower_with_semantic_facts(const SemanticResult *semantic,
                              HIRSemanticProjectionFailure *failure,
                              char **error_message)
{
    HIRProgram *hir;

    if (failure != NULL)
        *failure = HIR_SEMANTIC_PROJECTION_NONE;
    if (error_message != NULL)
        *error_message = NULL;
    if (semantic == NULL || semantic->annotated_ast == NULL) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOWER;
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR semantic fact projection requires semantic facts");
        return NULL;
    }

    hir = hir_lower_with_resource_and_param_flow_facts(
        semantic->annotated_ast,
        semantic->resource_flow_facts,
        semantic->resource_flow_fact_count,
        semantic->function_param_flow_facts,
        semantic->function_param_flow_fact_count,
        error_message);
    if (hir == NULL) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOWER;
        return NULL;
    }
    if (!hir_attach_loop_flow_facts(
            hir,
            semantic->loop_flow_summary_facts,
            semantic->loop_flow_summary_fact_count,
            semantic->loop_flow_state_facts,
            semantic->loop_flow_state_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOOP_FLOW;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_iteration_type_facts(
            hir,
            semantic->iteration_type_facts,
            semantic->iteration_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_ITERATION_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_destructure_type_facts(
            hir,
            semantic->destructure_type_facts,
            semantic->destructure_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_DESTRUCTURE_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_match_binding_type_facts(
            hir,
            semantic->match_binding_type_facts,
            semantic->match_binding_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_MATCH_BINDING_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_region_escape_facts(
            hir,
            semantic->region_escape_facts,
            semantic->region_escape_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_REGION_ESCAPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_validate(hir, error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_VALIDATE;
        hir_destroy(hir);
        return NULL;
    }
    return hir;
}
