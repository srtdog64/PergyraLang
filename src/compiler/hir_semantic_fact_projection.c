#include "hir.h"

#include "../common/string_compat.h"
#include "../semantic/semantic.h"

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
    if (!hir_validate(hir, error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_VALIDATE;
        hir_destroy(hir);
        return NULL;
    }
    return hir;
}
