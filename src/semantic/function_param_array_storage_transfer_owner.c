/* Settled Array preservation queries over the function flow summary. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "formal_string_store_landing_owner.h"
#include "collection_owned_element_requirement_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_ownership_internal.h"
#include "typed_map_query_fact_owner.h"
#include "../parser/ast_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "function_param_array_storage_walk_internal.h"
static unsigned
array_preservation_bits(const FunctionParamFlowSummaryEntry *entry)
{
    return (entry->array_storage_unproved ? FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED : 0)
        | (entry->array_elements_unproved ? FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED : 0);
}

/* A settled entry's verdict, read without demanding a summary. Every entry
 * is walked in the first pass, and the walk refuses a formal that is not an
 * Array storage formal. */
unsigned
function_param_flow_array_preservation_settled(FunctionParamFlowSummaryStore *store, size_t index)
{
    const FunctionParamFlowSummaryEntry *entry = &store->entries[index];
    if (entry->state != FUNCTION_PARAM_FLOW_COMPLETE || entry->analysis_unsupported
        || !entry->output_dependencies_ready || !function_param_flow_output_current(store, index, false))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return array_preservation_bits(entry);
}

bool
function_param_flow_array_buffer_kept(SemanticContext *ctx, ASTNode *function_decl, size_t param_index)
{
    bool paths = true;
    if (ctx == NULL || function_decl == NULL || ast_func_body(function_decl) == NULL || function_decl->is_async_decl ||
        !function_param_array_storage_formal_kind(function_decl, param_index, &paths) || paths) return false;
    (void)function_param_flow_summary_for_param(ctx, function_decl, param_index);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    size_t index = store == NULL || store->failed ? SIZE_MAX
        : function_param_flow_find(store, ast_node_stable_id(function_decl), param_index);
    if (index == SIZE_MAX || store->entries[index].function_decl != function_decl) return false;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[index];
    return !entry->array_buffer_unkept && (store->solving || (entry->state == FUNCTION_PARAM_FLOW_COMPLETE &&
        !entry->analysis_unsupported && entry->output_dependencies_ready && function_param_flow_output_current(store, index, false)));
}

unsigned
function_param_flow_array_preservation_failures(SemanticContext *ctx,
    ASTNode *function_decl, size_t param_index)
{
    FuncParam *param = ast_func_param(function_decl, param_index);
    bool paths = false;
    if (ctx == NULL || function_decl == NULL || param == NULL
        || ast_func_body(function_decl) == NULL || function_decl->is_async_decl
        || !function_param_array_storage_formal_kind(function_decl, param_index, &paths))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    (void)function_param_flow_summary_for_param(ctx, function_decl, param_index);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    size_t index = function_param_flow_find(store, ast_node_stable_id(function_decl), param_index);
    if (index == SIZE_MAX || store->entries[index].function_decl != function_decl)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return store->solving ? array_preservation_bits(&store->entries[index])
        : function_param_flow_array_preservation_settled(store, index);
}

bool
function_param_flow_preserves_array_storage(SemanticContext *ctx, ASTNode *decl, size_t ordinal)
{
    return (function_param_flow_array_preservation_failures(ctx, decl, ordinal)
        & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
}
