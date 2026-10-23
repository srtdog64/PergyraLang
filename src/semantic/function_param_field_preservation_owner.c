/* Field preservation of settled struct formals: whether a callee writes the
 * storage one top-level field of its formal names. The storage walk proves
 * it over the formal's settled program points, ignoring every other field's
 * paths; the verdicts are kept per function, formal and field. */
#include "function_param_flow_summary_internal.h"
#include "slot_summary.h"
#include "../parser/ast_api.h"
#include <stdlib.h>
#include <string.h>

typedef struct FunctionParamFlowFieldVerdict {
    uint32_t function_id;
    size_t formal;
    const char *field;
    unsigned failures;
    bool computing;
} FunctionParamFlowFieldVerdict;

/* Verdicts are kept by index: a nested field demand of the walk may grow
 * the table. A verdict still being computed is a cycle and reads as
 * unproved. */
unsigned
function_param_flow_field_preservation_failures(SemanticContext *ctx, ASTNode *decl, size_t ordinal,
    const char *field)
{
    unsigned whole = function_param_flow_array_preservation_failures(ctx, decl, ordinal);
    FunctionParamFlowSummaryStore *store = ctx != NULL ? ctx->function_param_flow_summaries : NULL;
    if (whole == 0 || field == NULL || store == NULL || store->solving || store->failed) return whole;
    uint32_t id = ast_node_stable_id(decl);
    for (size_t i = 0; i < store->field_verdict_count; i++) {
        const FunctionParamFlowFieldVerdict *row = &store->field_verdicts[i];
        if (row->function_id == id && row->formal == ordinal && strcmp(row->field, field) == 0)
            return row->computing ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : row->failures;
    }
    if (store->field_verdict_count == store->field_verdict_capacity) {
        size_t capacity = store->field_verdict_capacity == 0 ? 16 : store->field_verdict_capacity * 2;
        FunctionParamFlowFieldVerdict *rows = realloc(store->field_verdicts, capacity * sizeof(*rows));
        if (rows == NULL) return whole;
        store->field_verdicts = rows;
        store->field_verdict_capacity = capacity;
    }
    size_t at = store->field_verdict_count++;
    store->field_verdicts[at] = (FunctionParamFlowFieldVerdict){id, ordinal, field, whole, true};
    size_t entry = function_param_flow_find(store, id, ordinal);
    FunctionParamFlowProgramPointIndex *points = NULL;
    unsigned failures = whole;
    if (entry != SIZE_MAX && function_param_flow_program_point_index_build(store, decl, &points) &&
        points != NULL && ordinal < points->param_count) {
        SlotSummaryOrigin origin = {0};
        origin.function_decl = decl;
        origin.param_index = ordinal;
        origin.param_name = ast_func_param(decl, ordinal)->name;
        SlotFunctionLookup lookup = {ctx, store->program_root};
        if (!function_param_array_storage_field_unproved_in_program_points(points->dependency_roots,
                points->dependency_root_count, points->roots_by_param[ordinal], points->root_counts[ordinal],
                &origin, &lookup, store->entries[entry].mask, field, &failures)) failures = whole;
    }
    store->field_verdicts[at].failures = failures;
    store->field_verdicts[at].computing = false;
    return failures;
}
