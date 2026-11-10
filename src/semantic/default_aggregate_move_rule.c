/* Default-mode aggregate call rules: which values share storage when they
 * are copied, and which default-mode formals move or lend what they
 * receive, read from the callee's final parameter-flow summary. The move
 * owner (default_aggregate_move_owner.c) applies them in execution order. */

#include "default_aggregate_move_rule.h"

#include <stdint.h>
#include <string.h>

#include "function_param_flow_summary_internal.h"
#include "slot_summary.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"

/* An enum value is copied with its payloads, so one whose payloads hold
 * storage shares it as a struct does. */
bool semantic_default_aggregate_value(SemanticContext *ctx, const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN) return false;
    if (type->kind == TYPE_KIND_CLASS) return type->nominal_flavor == TYPE_NOMINAL_STRUCT;
    if (type->kind == TYPE_KIND_ENUM)
        return semantic_classify_ownership_type(type, ctx) == OWNERSHIP_TYPE_BORROW_TRACKED;
    return type->kind == TYPE_KIND_TUPLE || type->kind == TYPE_KIND_CONSTRUCTED;
}

/* An own formal consumes the aggregate regardless of what its body does. A
 * default-mode formal moves only when the callee changes, keeps, returns or
 * releases storage it receives. For an Array formal the descriptor-transfer
 * owner answers exactly: element values that escape (Strings are immutable)
 * are the String owners' concern, not a move. Any other aggregate moves
 * unless its summary only reads it and the same owner proves that no store
 * through its access paths changes the storage it shares with the caller.
 * A summary that is missing or not final counts as a change. */
bool semantic_default_aggregate_formal_moves(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL) return false;
    if (param->mode == PARAM_MODE_OWN) return true;
    if (param->mode != PARAM_MODE_DEFAULT || ast_func_body(callee) == NULL)
        return false;
    unsigned mask = function_param_flow_summary_for_param(ctx, callee, formal);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return true;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return true;
    if ((mask & SLOT_PARAM_SUMMARY_RELEASE) != 0) return true;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0)
        return store->entries[index].array_storage_unproved;
    return (mask & ~SLOT_PARAM_SUMMARY_READ) != 0 || store->entries[index].array_storage_unproved;
}

bool semantic_default_aggregate_formal_borrow_handoff(
    SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode != PARAM_MODE_DEFAULT ||
        ast_func_body(callee) == NULL)
        return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store,
        ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX ||
        store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE)
        return false;
    const unsigned escape = SLOT_PARAM_SUMMARY_CALL_ESCAPE |
        SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    unsigned outward = SLOT_PARAM_SUMMARY_NONE;
    if (!function_param_flow_entry_outward_effects_current(store, index,
            &outward))
        return false;
    unsigned mask = store->entries[index].mask;
    return (outward & escape) != 0 &&
        (outward & ~escape) == 0 &&
        (mask & ~(SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            escape)) == 0;
}

/* The callee only reads this default-mode argument and returns it or a part
 * of it. A call the caller follows with another use of the argument lends
 * it instead of moving it (2026-10-19_by_value_getter_lends_when_used_again).
 * The loan proof refuses the lend when that call or a later one changes the
 * storage. An Array formal keeps the descriptor-transfer answer. */
bool semantic_default_aggregate_formal_lends(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode != PARAM_MODE_DEFAULT || ast_func_body(callee) == NULL) return false;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return false;
    unsigned outward = SLOT_PARAM_SUMMARY_NONE;
    if (!function_param_flow_entry_outward_effects_current(store, index,
            &outward))
        return false;
    /* Internal carrier moves contribute CALL to the public access mask.
     * Only the backing frame can distinguish that bookkeeping from storage
     * really retained by a call. Keep write/release/unknown access bits fatal
     * to lending, and use the positive outward facet for the escape choice. */
    unsigned mask = store->entries[index].mask;
    const unsigned escapes = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    return outward == SLOT_PARAM_SUMMARY_RETURN_ESCAPE &&
        (mask & ~(SLOT_PARAM_SUMMARY_READ | escapes)) == 0;
}
