/* Default-mode aggregate call rules: which values share storage when they
 * are copied, and which default-mode formals move or lend what they
 * receive. The callee's final parameter-flow summary gives the formal's
 * effect class; the generated call hand-off rule (call_handoff_rule.def)
 * reduces it to a kind and an action. The move owner
 * (default_aggregate_move_owner.c) applies them in execution order. */

#include "default_aggregate_move_rule.h"

#include <stdint.h>
#include <string.h>

#include "call_handoff_rule.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "slot_summary.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_ownership_internal.h"

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

/* The effect class of a default-mode formal with a body (study 2.1, native
 * column), read from its final summary. A summary that is missing or not
 * final is UNPROVED. For an Array formal the descriptor-transfer owner
 * answers exactly: element values that escape (Strings are immutable) are
 * the String owners' concern, so only a release and the descriptor bit
 * count, and that bit cannot tell a return from a growth (divergence
 * NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY). Any other aggregate also counts the
 * descriptor bit: a store through its access paths that changes the
 * storage it shares with the caller. */
bool semantic_default_aggregate_formal_class(SemanticContext *ctx,
    ASTNode *callee, size_t formal, unsigned *members)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || members == NULL || param->mode != PARAM_MODE_DEFAULT
        || ast_func_body(callee) == NULL)
        return false;
    unsigned mask = function_param_flow_summary_for_param(ctx, callee, formal);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    *members = PGY_CALL_HANDOFF_MEMBER_UNPROVED;
    if (store == NULL || store->failed) return true;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return true;
    unsigned descriptor = store->entries[index].array_storage_unproved
        ? PGY_CALL_HANDOFF_MEMBER_GROW : 0u;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0)
        *members = descriptor | ((mask & SLOT_PARAM_SUMMARY_RELEASE) != 0
            ? PGY_CALL_HANDOFF_MEMBER_RELEASE : 0u);
    else
        *members = pgy_call_handoff_members_from_native_summary(mask) | descriptor;
    return true;
}

/* An own formal consumes the aggregate regardless of what its body does. A
 * default-mode formal moves unless the rule lends its class for the call:
 * only a formal that reads the storage it shares with the caller does. */
bool semantic_default_aggregate_formal_moves(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL) return false;
    if (param->mode == PARAM_MODE_OWN) return true;
    unsigned members = 0u;
    if (!semantic_default_aggregate_formal_class(ctx, callee, formal, &members))
        return false;
    return pgy_call_handoff_action_any_use(pgy_call_handoff_kind(members),
            PGY_CALL_HANDOFF_CONTEXT_SYNC, PGY_CALL_HANDOFF_ELEMENTS_AS_GIVEN)
        != PGY_CALL_HANDOFF_ACTION_LEND_FOR_CALL;
}

/* The class of a formal from the outward facet of its summary: the escapes
 * its backing frame publishes. Internal carrier moves contribute CALL to the
 * public access mask, and only the backing frame can distinguish that
 * bookkeeping from storage really retained by a call. The mask still gives
 * the read, write, release and unknown members. */
static bool
formal_outward_class(FunctionParamFlowSummaryStore *store, size_t index,
    unsigned *members)
{
    unsigned outward = SLOT_PARAM_SUMMARY_NONE;
    if (!function_param_flow_entry_outward_effects_current(store, index,
            &outward))
        return false;
    const unsigned escapes = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    *members = pgy_call_handoff_members_from_native_summary(
        (outward & escapes) | (store->entries[index].mask & ~escapes));
    return true;
}

/* The callee only reads the storage and keeps it (a call or channel
 * escape): its outward class is Keep only. */
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
    unsigned members = 0u;
    return formal_outward_class(store, index, &members) &&
        pgy_call_handoff_kind(members) == PGY_CALL_HANDOFF_KIND_KEEP_ONLY;
}

/* The callee only reads this default-mode argument and returns it or a part
 * of it: its outward class is Return only. A call the caller follows with
 * another use of the argument lends it instead of moving it
 * (2026-10-19_by_value_getter_lends_when_used_again). The loan proof
 * refuses the lend when that call or a later one changes the storage. An
 * Array formal keeps the descriptor-transfer answer. A formal of a type
 * parameter never lends: a generic routine that returns its argument moves
 * it at every call, whatever the call binds the type to (2026-11-16 user
 * decision on generic formals). A formal whose published type is unknown
 * does not lend either. */
bool semantic_default_aggregate_formal_lends(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode != PARAM_MODE_DEFAULT || ast_func_body(callee) == NULL) return false;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0) return false;
    const PendingRoutine *routine = ctx->function_param_publications != NULL
        ? publication_routine(ctx->function_param_publications,
            ast_node_stable_id(callee)) : NULL;
    if (routine == NULL || routine->decl != callee || formal >= routine->count
        || routine->types[formal] == NULL
        || routine->types[formal]->kind == TYPE_KIND_GENERIC)
        return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return false;
    /* Write, release and unknown access bits are fatal to lending; the
     * outward facet gives the escape choice. */
    unsigned members = 0u;
    return formal_outward_class(store, index, &members) &&
        pgy_call_handoff_kind(members) == PGY_CALL_HANDOFF_KIND_RETURN_ONLY;
}
