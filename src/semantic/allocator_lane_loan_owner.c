/* Allocator lane loans.
 *
 * A String finished with a result lane allocator owns heap storage of its
 * own. Finished with any other allocator, a pool among them, it lives in
 * that allocator until AllocatorDestroy frees it. Such a finish, and a call
 * whose callee's summary returns the allocator formal it passes, gives a
 * result that is a loan of the allocator binding.
 *
 * Once the summaries are final, this owner checks each loan of a local
 * allocator: the loan must stay in the routine, and each AllocatorDestroy of
 * that allocator must be a top-level statement of the routine body, after
 * which the routine reads nothing that could hold the loan. A loan of a
 * formal allocator is reported to the routine's callers by its summary
 * (docs/truth/decisions/2026-10-19_finished_text_borrows_its_allocator.md). */

#include "allocator_lane_loan_owner.h"

#include <stdint.h>
#include <stdlib.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "slot_summary.h"
#include "type_checker/type_checker_internal.h"

typedef struct {
    ASTNode *call;
    uint32_t allocator;
} AllocatorUse;

typedef struct {
    SemanticContext *ctx;
    ASTNode *routine;
    AllocatorUse *loans, *destroys;
    size_t loan_count, loan_capacity, destroy_count, destroy_capacity;
    bool failed, unsupported;
    ASTNode *stale_summary;
} LaneWalk;

static bool use_add(LaneWalk *walk, AllocatorUse **rows, size_t *count, size_t *capacity,
    ASTNode *call, uint32_t allocator)
{
    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        AllocatorUse *grown = realloc(*rows, next * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        *rows = grown; *capacity = next;
    }
    (*rows)[(*count)++] = (AllocatorUse){call, allocator};
    return true;
}

/* The binding of an Allocator argument named by an identifier, or 0. */
static uint32_t allocator_argument(SemanticContext *ctx, ASTNode *call, size_t ordinal)
{
    ASTNode *arg = ast_call_argument(call, ordinal);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, arg);
    if (arg == NULL || !value.ok || value.meta || value.type != TYPE_ALLOCATOR) return 0;
    return semantic_param_publication_binding(ctx, arg);
}

static bool routine_formal(const ASTNode *routine, uint32_t binding, ParamMode *mode)
{
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == binding) {
            if (mode != NULL) *mode = param->mode;
            return true;
        }
    }
    return false;
}

/* Record the call when its result is a loan of an allocator argument. */
static void record_call(LaneWalk *walk, ASTNode *call)
{
    SemanticContext *ctx = walk->ctx;
    uint32_t kind = 0;
    if (!semantic_param_publication_checked_call(ctx, call)) return;
    if (ast_call_semantic_callee_builtin_kind(call, &kind) && kind == (uint32_t)BUILTIN_ALLOCATOR_DESTROY) {
        uint32_t allocator = ast_call_arg_count(call) == 1 ? allocator_argument(ctx, call, 0) : 0;
        if (allocator != 0) (void)use_add(walk, &walk->destroys, &walk->destroy_count, &walk->destroy_capacity, call, allocator);
        return;
    }
    if (ast_call_semantic_callee_builtin_kind(call, &kind) && kind == (uint32_t)BUILTIN_TEXT_BUILDER_FINISH) {
        uint32_t allocator = ast_call_arg_count(call) == 2 ? allocator_argument(ctx, call, 1) : 0;
        if (allocator != 0 && !semantic_param_publication_checked_finish_independent(ctx, call))
            (void)use_add(walk, &walk->loans, &walk->loan_count, &walk->loan_capacity, call, allocator);
        return;
    }
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    bool compiler_builtin = ast_call_semantic_callee_builtin_kind(call, &kind) &&
        kind != (uint32_t)BUILTIN_NOT_BUILTIN;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        uint32_t allocator = allocator_argument(ctx, call, p);
        if (allocator == 0 || semantic_param_publication_result_lane_argument(ctx, call, p)) continue;
        /* The loan is a callee's summary returning the allocator formal it
         * is passed. A compiler builtin has no such summary: finished text is
         * recorded above, and no other builtin returns text of its allocator. */
        if (compiler_builtin) continue;
        /* Only a declared routine has an exact outward-effect row. A function
         * value answers by its call summary. */
        unsigned outward_effects = function_param_flow_summary_for_call(&lookup, call, p);
        if (semantic_param_publication_callee(ctx, call, p) != NULL &&
            !function_param_flow_callee_outward_effects_current(&lookup, call,
                p, &outward_effects)) {
            walk->stale_summary = call;
            return;
        }
        if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0)
            (void)use_add(walk, &walk->loans, &walk->loan_count, &walk->loan_capacity, call, allocator);
    }
}

static void collect(LaneWalk *walk, ASTNode *node)
{
    if (node == NULL || walk->failed || walk->stale_summary != NULL) return;
    if (node->type == AST_FUNC_DECL || node->type == AST_CLASS_DECL || node->type == AST_ENUM_DECL) return;
    if (node->type == AST_CALL) record_call(walk, node);
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) { walk->unsupported = true; return; }
    for (size_t c = 0; c < count; c++) collect(walk, function_param_flow_claim_child(node, c));
}

/* Every binding an identifier below `node` names, added to `ids`. */
static bool names_below(LaneWalk *walk, ASTNode *node, uint32_t **ids, size_t *count, size_t *capacity)
{
    if (node == NULL) return true;
    uint32_t id = node->type == AST_IDENTIFIER ? semantic_param_publication_binding(walk->ctx, node)
        : node->type == AST_LET_DECL ? ast_node_stable_id(node) : 0;
    if (id != 0) {
        if (*count == *capacity) {
            size_t next = *capacity == 0 ? 16 : *capacity * 2;
            uint32_t *grown = realloc(*ids, next * sizeof(*grown));
            if (grown == NULL) { walk->failed = true; return false; }
            *ids = grown; *capacity = next;
        }
        (*ids)[(*count)++] = id;
    }
    bool supported = false;
    size_t children = function_param_flow_claim_child_count(node, &supported);
    if (!supported || children == SIZE_MAX) return false;
    for (size_t c = 0; c < children; c++)
        if (!names_below(walk, function_param_flow_claim_child(node, c), ids, count, capacity)) return false;
    return true;
}

/* A read below `node`, after the allocator was destroyed, of a value that
 * could hold its loan: the allocator itself, or a binding that is not a
 * physical scalar or a non-inout formal and that the statements between the
 * allocator's birth and its destroy name. */
static ASTNode *late_read(LaneWalk *walk, ASTNode *node, uint32_t allocator,
    const uint32_t *touched, size_t touched_count)
{
    if (node == NULL) return NULL;
    if (node->type == AST_IDENTIFIER) {
        uint32_t id = semantic_param_publication_binding(walk->ctx, node);
        ParamMode mode = PARAM_MODE_DEFAULT;
        if (id == allocator) return node;
        if (id != 0 && !semantic_param_publication_value_is_physical_scalar(walk->ctx, node) &&
            !(routine_formal(walk->routine, id, &mode) && mode != PARAM_MODE_MUT_REF))
            for (size_t i = 0; i < touched_count; i++) if (touched[i] == id) return node;
    }
    bool supported = false;
    size_t children = function_param_flow_claim_child_count(node, &supported);
    if (!supported || children == SIZE_MAX) return node;
    for (size_t c = 0; c < children; c++) {
        ASTNode *hit = late_read(walk, function_param_flow_claim_child(node, c), allocator, touched, touched_count);
        if (hit != NULL) return hit;
    }
    return NULL;
}

static size_t top_level_index(ASTNode *body, const ASTNode *node)
{
    for (size_t i = 0; body != NULL && i < ast_block_statement_count(body); i++)
        if (ast_block_statement(body, i) == node) return i;
    return SIZE_MAX;
}

static size_t top_level_let(ASTNode *body, uint32_t binding)
{
    for (size_t i = 0; body != NULL && i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL && ast_node_stable_id(stmt) == binding) return i;
    }
    return SIZE_MAX;
}

/* The destroy of a pool its routine holds loans of: refused unless every
 * loan is provably dead when it runs. */
static void check_destroy(LaneWalk *walk, const AllocatorUse *destroy)
{
    ASTNode *body = ast_func_body(walk->routine);
    size_t born = top_level_let(body, destroy->allocator);
    size_t at = top_level_index(body, destroy->call);
    ASTNode *bad = NULL;
    if (born == SIZE_MAX || at == SIZE_MAX || at <= born) bad = destroy->call;
    uint32_t *touched = NULL;
    size_t touched_count = 0, touched_capacity = 0;
    for (size_t i = born + 1; bad == NULL && i < at; i++)
        if (!names_below(walk, ast_block_statement(body, i), &touched, &touched_count, &touched_capacity))
            bad = destroy->call;
    for (size_t i = at + 1; bad == NULL && i < ast_block_statement_count(body); i++)
        bad = late_read(walk, ast_block_statement(body, i), destroy->allocator, touched, touched_count);
    free(touched);
    if (bad == NULL || walk->failed) return;
    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE, bad,
        "A String finished with a non-result allocator may be read after AllocatorDestroy frees it.\n"
        "Reason:\n"
        "- text finished with this allocator lives in the allocator's storage\n"
        "- the destroy must be a statement of the routine body after every use of that text\n"
        "Fix:\n"
        "- destroy the allocator after the last use of the text\n"
        "- or finish the text with AllocatorResult()");
}

static void check_routine(LaneWalk *walk)
{
    for (size_t l = 0; l < walk->loan_count; l++) {
        const AllocatorUse *loan = &walk->loans[l];
        if (routine_formal(walk->routine, loan->allocator, NULL)) continue;
        if (walk->unsupported || !function_param_flow_call_result_stays(walk->ctx, walk->routine, loan->call))
            semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE, loan->call,
                "A String finished with a non-result allocator cannot leave the routine that owns the allocator.\n"
                "Reason:\n"
                "- text finished with this allocator lives in the allocator's storage\n"
                "- the allocator is a local of this routine\n"
                "Fix:\n"
                "- finish the text with AllocatorResult()\n"
                "- or take the allocator as a parameter");
    }
    for (size_t d = 0; d < walk->destroy_count; d++) {
        bool loaned = false;
        for (size_t l = 0; l < walk->loan_count; l++)
            if (walk->loans[l].allocator == walk->destroys[d].allocator) loaned = true;
        if (loaned) check_destroy(walk, &walk->destroys[d]);
    }
}

bool semantic_allocator_lane_loan_finalize(SemanticContext *ctx)
{
    SemanticParamPublicationView view;
    if (ctx == NULL || ctx->has_error) return ctx != NULL && !ctx->has_error;
    if (!semantic_param_publication_admitted_view(ctx, &view)) {
        semantic_error(ctx, ctx->program_root, "Allocator lane loan check requires the admitted parameter view");
        return false;
    }
    ASTNode *previous_decl = ctx->current_function_decl;
    const char *previous_module = ctx->current_module_path;
    LaneWalk walk = {.ctx = ctx};
    for (size_t r = 0; r < view.routine_count && !walk.failed && walk.stale_summary == NULL; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (row->decl == NULL || ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        walk.routine = row->decl;
        walk.loan_count = walk.destroy_count = 0;
        walk.unsupported = false;
        collect(&walk, ast_func_body(row->decl));
        if (!walk.failed && walk.stale_summary == NULL) check_routine(&walk);
    }
    free(walk.loans);
    free(walk.destroys);
    ctx->current_function_decl = previous_decl;
    ctx->current_module_path = previous_module;
    if (walk.failed) semantic_error(ctx, ctx->program_root, "Allocator lane loan check allocation failed");
    else if (walk.stale_summary != NULL)
        semantic_error(ctx, walk.stale_summary,
            "Allocator lane loan check has no current outward-effect summary for this call's allocator argument");
    return !ctx->has_error;
}
