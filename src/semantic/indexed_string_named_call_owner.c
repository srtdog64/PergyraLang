/* Non-retention of String values passed to declared callees.
 *
 * The indexed String owner refuses a String whose Array owner it cannot
 * prove alive past a consumer. For a declared callee the proof is the
 * callee's own parameter-flow summary, which is final only after Pass 2, so
 * typing records the argument here. A receipt is admitted when the callee
 * only reads the String formal (it neither keeps nor returns it), and every
 * other argument that could reach an Array is read-only or only returned:
 * the String's owner then stays alive and unchanged for the whole call. A
 * callee that also returns the String lends it to the call's result; an
 * Array payload consumer admits that through the loan proof
 * (semantic_indexed_string_named_call_lends).
 * A receipt that fails, or whose summary is missing or not final, is refused
 * with the diagnostic typing would have given. */

#include "indexed_string_named_call_owner.h"

#include <stdint.h>
#include <stdlib.h>

#include "diag_codes.h"
#include "indexed_string_definition_recipe.h"
#include "loan_copy_landing_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "slot_summary.h"
#include "type_checker/type_checker_internal.h"

typedef struct {
    ASTNode *expression, *call, *callee, *routine;
    uint32_t expression_id, call_id, callee_id, routine_id;
    size_t ordinal;
    bool nonretention_obligation;
    bool local_own_candidate;
} NamedCallReceipt;

struct IndexedStringNamedCallStore {
    NamedCallReceipt *rows;
    size_t count, capacity;
};

bool semantic_indexed_string_named_call_begin(SemanticContext *ctx)
{
    if (ctx == NULL) return false;
    semantic_indexed_string_named_call_destroy(ctx->indexed_string_named_calls);
    ctx->indexed_string_named_calls = calloc(1, sizeof(*ctx->indexed_string_named_calls));
    return ctx->indexed_string_named_calls != NULL;
}

void semantic_indexed_string_named_call_destroy(IndexedStringNamedCallStore *store)
{
    if (store == NULL) return;
    free(store->rows);
    free(store);
}

static bool named_call_record(SemanticContext *ctx,
    ASTNode *expression, ASTNode *call, ASTNode *callee, size_t ordinal,
    bool nonretention_obligation, bool local_own_candidate)
{
    IndexedStringNamedCallStore *store = ctx != NULL ? ctx->indexed_string_named_calls : NULL;
    if (store == NULL || expression == NULL || call == NULL || call->type != AST_CALL ||
        callee == NULL || callee->type != AST_FUNC_DECL || ctx->current_function_decl == NULL ||
        ordinal >= ast_call_arg_count(call) || ast_call_argument(call, ordinal) != expression ||
        ast_node_stable_id(expression) == 0 || ast_node_stable_id(call) == 0 ||
        ast_node_stable_id(callee) == 0) return false;
    for (size_t i = 0; i < store->count; i++) {
        NamedCallReceipt *row = &store->rows[i];
        if (row->call != call || row->ordinal != ordinal) continue;
        if (row->expression != expression || row->callee != callee ||
            row->routine != ctx->current_function_decl)
            return false;
        row->nonretention_obligation |= nonretention_obligation;
        row->local_own_candidate |= local_own_candidate;
        return true;
    }
    if (store->count == store->capacity) {
        size_t next = store->capacity == 0 ? 16 : store->capacity * 2;
        if (next < store->capacity || next > SIZE_MAX / sizeof(*store->rows)) return false;
        NamedCallReceipt *rows = realloc(store->rows, next * sizeof(*rows));
        if (rows == NULL) return false;
        store->rows = rows; store->capacity = next;
    }
    store->rows[store->count++] = (NamedCallReceipt){expression, call, callee,
        ctx->current_function_decl, ast_node_stable_id(expression), ast_node_stable_id(call),
        ast_node_stable_id(callee), ast_node_stable_id(ctx->current_function_decl),
        ordinal, nonretention_obligation, local_own_candidate};
    return true;
}

bool semantic_indexed_string_named_call_record(SemanticContext *ctx,
    ASTNode *expression, ASTNode *call, ASTNode *callee, size_t ordinal)
{
    return named_call_record(ctx, expression, call, callee, ordinal, true, false);
}

bool semantic_indexed_string_named_local_own_call_record(SemanticContext *ctx,
    ASTNode *expression, ASTNode *call, ASTNode *callee, size_t ordinal)
{
    return named_call_record(ctx, expression, call, callee, ordinal, false, true);
}

/* The callee's final summary for `formal` shows at most `allowed` effects.
 * An inout or own formal can change or free what it receives, so it never
 * qualifies. A missing, failed or unfinished summary does not qualify. */
static bool formal_effects_within(SemanticContext *ctx, ASTNode *callee, size_t formal, unsigned allowed)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode == PARAM_MODE_MUT_REF || param->mode == PARAM_MODE_OWN ||
        ast_func_body(callee) == NULL || callee->is_async_decl) return false;
    unsigned mask = function_param_flow_summary_for_param(ctx, callee, formal);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    return index != SIZE_MAX && store->entries[index].state == FUNCTION_PARAM_FLOW_COMPLETE &&
        (mask & ~allowed) == 0;
}

/* No argument other than `ordinal` can change or release an Array during
 * the call: each is a scalar, a String, or only read or returned. */
static bool other_arguments_read(SemanticContext *ctx, ASTNode *call, ASTNode *callee, size_t ordinal)
{
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        if (p == ordinal) continue;
        ASTNode *argument = ast_call_argument(call, p);
        size_t other = 0;
        if (!semantic_param_publication_formal_for_actual(ctx, call, p, &other)) return false;
        FuncParam *param = ast_func_param(callee, other);
        if (param == NULL || param->mode == PARAM_MODE_MUT_REF || param->mode == PARAM_MODE_OWN) return false;
        if (semantic_param_publication_value_is_physical_scalar(ctx, argument)) continue;
        SemanticPublishedValue value = semantic_param_publication_value(ctx, argument);
        if (!value.ok) return false;
        if (value.type == TYPE_STRING) continue;
        if (!formal_effects_within(ctx, callee, other,
                SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) return false;
    }
    return true;
}

/* The String formal at `ordinal` shows at most `allowed` effects. */
static bool string_formal_within(SemanticContext *ctx, ASTNode *call, ASTNode *callee, size_t ordinal,
    unsigned allowed)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL || callee == NULL ||
        callee->type != AST_FUNC_DECL || ordinal >= ast_call_arg_count(call) ||
        ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(callee)) return false;
    size_t formal = 0;
    return semantic_param_publication_formal_for_actual(ctx, call, ordinal, &formal) &&
        formal_effects_within(ctx, callee, formal, allowed);
}

bool semantic_indexed_string_named_call_reads_only(SemanticContext *ctx,
    ASTNode *call, ASTNode *callee, size_t ordinal)
{
    return string_formal_within(ctx, call, callee, ordinal, SLOT_PARAM_SUMMARY_READ) &&
        other_arguments_read(ctx, call, callee, ordinal);
}

bool semantic_indexed_string_named_call_lends(SemanticContext *ctx, ASTNode *routine,
    ASTNode *call, ASTNode *callee, size_t ordinal, uint32_t owner)
{
    if (owner == 0 || routine == NULL ||
        !string_formal_within(ctx, call, callee, ordinal,
            SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE) ||
        !other_arguments_read(ctx, call, callee, ordinal)) return false;
    FunctionParamFlowLoanFailure failure = {0};
    if (function_param_flow_lent_argument_admitted(ctx, routine, call,
            ast_call_argument(call, ordinal), owner, &failure)) return true;
    /* A preceding owner may already have selected the exact carriers where
     * this loan leaves, with which the lent proof found it staying (a store
     * the store owner selected is not among them: that owner copies it).
     * The current analysis is still rejected and the driver will reload
     * those Clones; do not also clone the borrowed element before the
     * selected exits have had a chance to prove the loan. */
    bool selected = failure.stop == FUNCTION_PARAM_FLOW_LOAN_STAYS_ONCE_COPIED;
    for (size_t c = 0; selected && c < failure.escapes.count; c++)
        selected = semantic_loan_copy_landing_selected(ctx, failure.escapes.at[c]);
    return selected;
}

static bool receipt_current(const NamedCallReceipt *row)
{
    return ast_node_stable_id(row->expression) == row->expression_id &&
        ast_node_stable_id(row->call) == row->call_id &&
        ast_node_stable_id(row->callee) == row->callee_id &&
        ast_node_stable_id(row->routine) == row->routine_id &&
        row->ordinal < ast_call_arg_count(row->call) &&
        ast_call_argument(row->call, row->ordinal) == row->expression;
}

static bool receipt_admitted(SemanticContext *ctx, const NamedCallReceipt *row)
{
    return receipt_current(row) &&
        semantic_indexed_string_named_call_reads_only(ctx, row->call, row->callee, row->ordinal);
}

bool semantic_indexed_string_named_call_prepare(SemanticContext *ctx)
{
    IndexedStringNamedCallStore *store = ctx != NULL ? ctx->indexed_string_named_calls : NULL;
    if (ctx == NULL) return false;
    /* This phase selects copy landings; it does not own parameter-flow
     * diagnostics.  A prior fail-closed summary can already have recorded an
     * error while its exact borrow diagnostic still needs the publication
     * consumer below to run.  Skip selection for that invalid generation,
     * but do not turn the skipped optional work into an early return that
     * hides the owning diagnostic. */
    if (ctx->has_error) return true;
    if (store == NULL) return true;
    ASTNode *saved = ctx->current_function_decl;
    for (size_t i = 0; i < store->count; i++) {
        const NamedCallReceipt *row = &store->rows[i];
        if (!row->local_own_candidate && !row->nonretention_obligation)
            continue;
        if (!receipt_current(row)) {
            semantic_error(ctx, row->expression,
                "String named-call receipt identity is no longer current");
            break;
        }
        size_t formal = 0;
        if (!semantic_param_publication_formal_for_actual(
                ctx, row->call, row->ordinal, &formal)) {
            semantic_error(ctx, row->expression,
                "String own-call receipt has no current formal identity");
            break;
        }
        FuncParam *param = ast_func_param(row->callee, formal);
        ctx->current_function_decl = row->routine;
        /* An inout actual must remain an addressable caller place.  If that
         * local can still denote an Array<String> element, detach each exact
         * reaching definition instead of wrapping the call argument in
         * Clone(...), which would destroy the place identity and any
         * write-back.  Exact aliases for which the definition owner cannot
         * issue such a copy remain refused by finalize below. */
        if (row->nonretention_obligation && param != NULL &&
            param->mode == PARAM_MODE_MUT_REF) {
            uint32_t binding = row->expression->type == AST_IDENTIFIER
                ? ast_identifier_binding_syntax_id(row->expression) : 0;
            bool detached = binding != 0 &&
                semantic_indexed_string_definition_select_independent_storage(
                    ctx, binding);
            if (detached) {
                semantic_loan_copy_landing_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->expression,
                    "An inout String local that may borrow an Array element is copied at each reaching definition");
                break;
            }
            continue;
        }
        if (!row->local_own_candidate) continue;
        if (param == NULL || param->mode != PARAM_MODE_OWN) {
            semantic_error(ctx, row->expression,
                "String own-call receipt no longer names an own formal");
            break;
        }
        if (!function_param_flow_value_borrows_ref_formal_current(
                ctx, row->routine, row->expression))
            continue;
        if (!semantic_loan_copy_landing_carrier_copies(
                ctx, row->expression))
            continue;
        if (!semantic_loan_copy_landing_select_carrier(ctx, row->expression)) {
            semantic_error(ctx, row->expression,
                "String own-call copy landing selection failed");
            break;
        }
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->expression,
            "A local String that still borrows a ref parameter is copied before an own handoff");
    }
    ctx->current_function_decl = saved;
    return !ctx->has_error;
}

bool semantic_indexed_string_named_call_finalize(SemanticContext *ctx)
{
    IndexedStringNamedCallStore *store = ctx != NULL ? ctx->indexed_string_named_calls : NULL;
    if (store == NULL) return ctx != NULL && !ctx->has_error;
    if (ctx->has_error) return false;
    ASTNode *saved = ctx->current_function_decl;
    for (size_t i = 0; i < store->count; i++) {
        const NamedCallReceipt *row = &store->rows[i];
        if (!row->nonretention_obligation) continue;
        ctx->current_function_decl = row->routine;
        if (receipt_admitted(ctx, row)) continue;
        size_t formal = 0;
        FuncParam *param = NULL;
        if (semantic_param_publication_formal_for_actual(
                ctx, row->call, row->ordinal, &formal))
            param = ast_func_param(row->callee, formal);
        /* Clone(inout_place) is not an inout value-result operation: it is a
         * temporary and loses caller write-back.  Reaching-definition
         * detachment is selected in prepare; if that owner cannot prove and
         * issue it, fail closed at the original place. */
        if (param != NULL && param->mode == PARAM_MODE_MUT_REF) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->expression,
                "An inout String argument has no independent reaching-definition proof");
            continue;
        }
        /* A prior driver round already put a fully independent Clone at this
         * exact boundary. Re-selecting its synthetic wrapper would request
         * Clone(Clone(...)) outside the original stable-id namespace. */
        if (semantic_loan_copy_landing_satisfied(ctx, row->expression))
            continue;
        /* No proof admits it, so a next analysis passes a copy of the String
         * (2026-10-21_string_argument_no_proof_admits_is_copied). */
        bool selected = semantic_loan_copy_landing_select_carrier(
            ctx, row->expression);
        if (!selected)
            semantic_error(ctx, row->expression, "String argument copy landing selection failed");
        if (selected)
            semantic_loan_copy_landing_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->expression,
                "A String element cannot cross an opaque boundary without a current non-retention proof");
        else
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                row->expression,
                "A String element cannot cross an opaque boundary without a current non-retention proof");
    }
    ctx->current_function_decl = saved;
    return !ctx->has_error;
}
