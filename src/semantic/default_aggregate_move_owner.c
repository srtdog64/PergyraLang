/* Default-mode aggregate move owner.
 *
 * A default-mode parameter of aggregate type is a shallow copy: the callee
 * and the caller share its Array storage. When the callee's parameter-flow
 * summary says it changes that storage in place or lets it out, the call
 * moves the argument (docs/truth/decisions/2026-10-18_default_aggregate_move.md).
 *
 * This owner runs once the summaries are final. It walks each routine body
 * in execution order, records the access path an argument names when the
 * call moves it, and refuses a later use of that path, of a part of it, or
 * of a value containing it, until an assignment re-initializes the path.
 * Branches merge by union; a loop body is walked twice so a use in the next
 * iteration sees a move made later in the body. */

#include "default_aggregate_move_owner.h"
#include "default_aggregate_move_internal.h"
#include "default_aggregate_move_rule.h"
#include "loan_copy_landing_owner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_read_path_owner.h"
#include "slot_summary.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"

static void state_free(MoveState *state)
{
    for (size_t i = 0; i < state->count; i++) { free(state->rows[i].fields); free(state->rows[i].carrier_fields); }
    free(state->rows);
    *state = (MoveState){0};
}

bool move_walk_state_add(MoveWalk *walk, MoveState *state, uint32_t binding, const char *fields,
    const char *callee, const char *param, ASTNode *lending_call, uint32_t carrier, const char *carrier_fields,
    ASTNode *argument)
{
    /* Each loan of a path is a row of its own, so a later use needs every
     * one admitted. A move on any path is a move, whatever lent it, and
     * names the call that moved it; moves by different arguments, as in two
     * branches, stay apart so that each one's copy is selected. Copy rows
     * are kept apart. */
    bool moved = false;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *row = &state->rows[i];
        if (row->binding != binding || strcmp(row->fields, fields) != 0 || (row->carrier != 0) != (carrier != 0)) continue;
        if (carrier != 0) {
            if (row->carrier == carrier && row->lending_call == lending_call &&
                strcmp(row->carrier_fields, carrier_fields) == 0) return true;
            continue;
        }
        if (lending_call == NULL) {
            if (row->lending_call == NULL && row->argument != argument) continue;
            *row = (MovedPath){binding, row->fields, callee, param, NULL, 0, NULL, argument}; moved = true;
        }
        else if (row->lending_call == NULL || row->lending_call == lending_call) return true;
    }
    if (moved) return true;
    if (state->count == state->capacity) {
        size_t capacity = state->capacity == 0 ? 8 : state->capacity * 2;
        MovedPath *grown = realloc(state->rows, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        state->rows = grown; state->capacity = capacity;
    }
    char *copy = malloc(strlen(fields) + 1);
    char *held = carrier != 0 ? malloc(strlen(carrier_fields) + 1) : NULL;
    if (copy == NULL || (carrier != 0 && held == NULL)) { free(copy); free(held); walk->failed = true; return false; }
    strcpy(copy, fields);
    if (held != NULL) strcpy(held, carrier_fields);
    state->rows[state->count++] = (MovedPath){binding, copy, callee, param, lending_call, carrier, held, argument};
    return true;
}

static bool state_copy(MoveWalk *walk, MoveState *out, const MoveState *in)
{
    *out = (MoveState){0};
    for (size_t i = 0; i < in->count; i++)
        if (!move_walk_state_add(walk, out, in->rows[i].binding, in->rows[i].fields,
                in->rows[i].callee, in->rows[i].param, in->rows[i].lending_call, in->rows[i].carrier,
                in->rows[i].carrier_fields, in->rows[i].argument)) return false;
    return true;
}

static bool state_union(MoveWalk *walk, MoveState *into, const MoveState *from)
{
    for (size_t i = 0; i < from->count; i++)
        if (!move_walk_state_add(walk, into, from->rows[i].binding, from->rows[i].fields,
                from->rows[i].callee, from->rows[i].param, from->rows[i].lending_call, from->rows[i].carrier,
                from->rows[i].carrier_fields, from->rows[i].argument)) return false;
    return true;
}

bool move_walk_path_contains(const char *prefix, const char *fields)
{
    size_t n = strlen(prefix);
    return strncmp(prefix, fields, n) == 0 && (fields[n] == '\0' || fields[n] == '.');
}

bool move_walk_access_path(const ASTNode *node, uint32_t *binding, char *buf, size_t size)
{
    if (node == NULL) return false;
    if (node->type == AST_IDENTIFIER) {
        *binding = ast_identifier_binding_syntax_id(node);
        if (size == 0) return false;
        buf[0] = '\0';
        return *binding != 0;
    }
    if (node->type == AST_ARRAY_ACCESS) return move_walk_access_path(ast_array_access_array(node), binding, buf, size);
    if (node->type != AST_MEMBER_ACCESS || ast_member_name(node) == NULL) return false;
    if (!move_walk_access_path(ast_member_object(node), binding, buf, size)) return false;
    size_t used = strlen(buf), name = strlen(ast_member_name(node));
    if (used + name + 2 > size) return false;
    buf[used] = '.';
    memcpy(buf + used + 1, ast_member_name(node), name + 1);
    return true;
}

bool move_walk_reported_once(MoveWalk *walk, const ASTNode *node)
{
    for (size_t i = 0; i < walk->reported_count; i++) if (walk->reported[i] == node) return false;
    if (walk->reported_count == walk->reported_capacity) {
        size_t capacity = walk->reported_capacity == 0 ? 8 : walk->reported_capacity * 2;
        const ASTNode **grown = realloc(walk->reported, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->reported = grown; walk->reported_capacity = capacity;
    }
    walk->reported[walk->reported_count++] = node;
    return true;
}

/* A path a read-and-return call lent may be used again when that loan stays
 * in the routine, or leaves it only as the formal's own return, and nothing
 * changes its owner. */
static bool lent_use_admitted(MoveWalk *walk, const MovedPath *moved)
{
    if (moved->lending_call == NULL || moved->callee == NULL) return false;
    for (size_t i = 0; i < walk->lent_count; i++)
        if (walk->lent[i].call == moved->lending_call) return walk->lent[i].admitted;
    FunctionParamFlowLoanFailure failure = {0};
    bool admitted = function_param_flow_lent_argument_admitted(walk->ctx, walk->routine,
        moved->lending_call, moved->binding, &failure);
    if (walk->lent_count == walk->lent_capacity) {
        size_t capacity = walk->lent_capacity == 0 ? 8 : walk->lent_capacity * 2;
        void *grown = realloc(walk->lent, capacity * sizeof(*walk->lent));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->lent = grown; walk->lent_capacity = capacity;
    }
    walk->lent[walk->lent_count].call = moved->lending_call;
    walk->lent[walk->lent_count].failure = failure;
    walk->lent[walk->lent_count++].admitted = admitted;
    return admitted;
}

static void refuse_use(MoveWalk *walk, const ASTNode *node, const MovedPath *moved)
{
    if (lent_use_admitted(walk, moved)) return;
    /* The refusal stands in this analysis; a next one copies each refused
     * loan: at every place it leaves in turn, when the walks found it staying
     * with those copies; otherwise once, where it leaves, where it is made,
     * or, where neither can stand, the lent argument where the call takes
     * it, as a moved one is. */
    for (size_t i = 0; moved->lending_call != NULL && i < walk->lent_count; i++) {
        const FunctionParamFlowLoanFailure *failure = &walk->lent[i].failure;
        if (walk->lent[i].call != moved->lending_call) continue;
        if (!failure->owner_changed && (failure->escapes.count != 0 || failure->stays_once_copied)) {
            for (size_t c = 0; c < failure->escapes.count; c++)
                if (!semantic_loan_copy_landing_select_carrier(walk->ctx, failure->escapes.at[c])) walk->failed = true;
            continue;
        }
        bool at_exit = !failure->owner_changed && failure->escape_carrier != NULL &&
            semantic_loan_copy_landing_carrier_copies(walk->ctx, failure->escape_carrier);
        bool made = !at_exit && semantic_loan_copy_landing_call_copies(walk->ctx, moved->lending_call);
        const ASTNode *copy = at_exit ? failure->escape_carrier
            : !made && moved->argument != NULL && semantic_loan_copy_landing_carrier_copies(walk->ctx, moved->argument)
            ? moved->argument : NULL;
        if (made && !semantic_loan_copy_landing_select(walk->ctx, moved->lending_call)) walk->failed = true;
        if (copy != NULL && !semantic_loan_copy_landing_select_carrier(walk->ctx, copy)) walk->failed = true;
        /* Only where none of these can stand are the places it leaves
         * copied while its owner changes. */
        for (size_t c = 0; !made && copy == NULL && c < failure->escapes.count; c++)
            if (!semantic_loan_copy_landing_select_carrier(walk->ctx, failure->escapes.at[c])) walk->failed = true;
    }
    /* A copy whose other side changed is copied where it was made. A moved
     * argument used again is copied where the call takes it
     * (2026-10-22_moved_argument_used_again_is_copied). */
    if (moved->callee == NULL && !semantic_loan_copy_landing_select(walk->ctx, moved->lending_call)) walk->failed = true;
    if (moved->lending_call == NULL && moved->argument != NULL &&
        !semantic_loan_copy_landing_select_carrier(walk->ctx, moved->argument))
        walk->failed = true;
    if (!move_walk_reported_once(walk, node)) return;
    const ASTNode *root = node;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
    char display[512];
    const char *name = root != NULL && root->type == AST_IDENTIFIER ? ast_identifier_name(root) : NULL;
    if (name != NULL && strlen(name) + strlen(moved->fields) < sizeof(display)) {
        strcpy(display, name);
        strcat(display, moved->fields);
        name = display;
    }
    if (moved->callee == NULL) {
        semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, (ASTNode *)node,
            "'%s' shares its storage with the copy made on line %u, and one of them changed it.\n"
            "Reason:\n"
            "- a copy of a struct or collection value shares the storage it holds\n"
            "- after one side grows or changes that storage, the other may read freed storage\n"
            "Fix:\n"
            "- change only one of the two while the other is still used\n"
            "- or build the copy from fresh values",
            name != NULL ? name : "value", moved->lending_call->line);
        return;
    }
    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
        PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, (ASTNode *)node,
        "'%s' was moved into '%s' and cannot be used again.\n"
        "Reason:\n"
        "- parameter '%s' of '%s' changes or returns the storage it receives\n"
        "- the caller's copy shares that storage, so it may already be freed or changed\n"
        "Fix:\n"
        "- reassign the value from the call result, as in x = %s(x)\n"
        "- or pass a fresh value to the call",
        name != NULL ? name : "value", moved->callee, moved->param, moved->callee, moved->callee);
}

static ParamMode
routine_param_mode(const ASTNode *routine, uint32_t binding, bool *found)
{
    *found = false;
    for (size_t p = 0;
         routine != NULL && p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == binding) {
            *found = true;
            return param->mode;
        }
    }
    return PARAM_MODE_DEFAULT;
}

/* A use of `node` conflicts with a moved path when either contains the other. */
static void check_use(MoveWalk *walk, const MoveState *state, const ASTNode *node)
{
    char fields[512];
    uint32_t binding = 0;
    if (!move_walk_access_path(node, &binding, fields, sizeof(fields))) return;
    for (size_t i = 0; i < state->count; i++) {
        const MovedPath *moved = &state->rows[i];
        if (moved->carrier == 0 && moved->binding == binding &&
            (move_walk_path_contains(moved->fields, fields) || move_walk_path_contains(fields, moved->fields))) {
            /* A copy changed below an inout formal's value a call takes is
             * used there only if the callee reads that part
             * (function_param_read_path_owner.c). The change reaches the
             * caller through the inout formal as written; under a by-value or
             * ref formal or a local, the copy lands instead, so the change
             * stays the routine's own. */
            size_t length = strlen(fields);
            bool inout = false;
            if (moved->callee == NULL && walk->argument_call != NULL &&
                routine_param_mode(walk->routine, binding, &inout) == PARAM_MODE_MUT_REF && inout &&
                node == ast_call_argument(walk->argument_call, walk->argument_ordinal) &&
                strlen(moved->fields) > length && move_walk_path_contains(fields, moved->fields) &&
                function_param_read_paths_miss(walk->ctx, walk->argument_call, walk->argument_ordinal,
                    moved->fields + length)) continue;
            refuse_use(walk, node, moved);
        }
    }
}

static bool walk_node(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated);

static void reinitialize(MoveWalk *walk, MoveState *state, ASTNode *target)
{
    char fields[512];
    uint32_t binding = 0;
    if (!move_walk_access_path(target, &binding, fields, sizeof(fields))) return;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *moved = &state->rows[i];
        /* Writing a part of a moved value writes into freed storage. */
        if (moved->carrier == 0 && moved->binding == binding && move_walk_path_contains(moved->fields, fields) &&
            strcmp(moved->fields, fields) != 0) {
            refuse_use(walk, target, moved);
            return;
        }
    }
    size_t kept = 0;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *moved = &state->rows[i];
        if (moved->binding == binding && move_walk_path_contains(fields, moved->fields)) {
            free(moved->fields); free(moved->carrier_fields); continue;
        }
        state->rows[kept++] = *moved;
    }
    state->count = kept;
    if (fields[0] == '\0') move_walk_copy_rebound(state, binding);
}

/* Uses inside a nested expression, then the index expressions of a path. */
static bool walk_expression(MoveWalk *walk, MoveState *state, ASTNode *node)
{
    if (node == NULL || walk->failed) return !walk->failed;
    char fields[512];
    uint32_t binding = 0;
    if (move_walk_access_path(node, &binding, fields, sizeof(fields))) {
        check_use(walk, state, node);
        for (ASTNode *at = node; at != NULL && at->type != AST_IDENTIFIER;
             at = at->type == AST_MEMBER_ACCESS ? ast_member_object(at) : ast_array_access_array(at))
            if (at->type == AST_ARRAY_ACCESS && !walk_expression(walk, state, ast_array_access_index(at))) return false;
        return !walk->failed;
    }
    bool terminated = false;
    return walk_node(walk, state, node, &terminated);
}

static bool walk_children(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated)
{
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) {
        /* A construct whose children this owner cannot enumerate may use a
         * moved value; refuse while any move is live rather than skip it. */
        for (size_t i = 0; i < state->count; i++) if (state->rows[i].carrier == 0) refuse_use(walk, node, &state->rows[i]);
        return !walk->failed;
    }
    for (size_t c = 0; c < count && !*terminated; c++) {
        ASTNode *child = function_param_flow_claim_child(node, c);
        if (child != NULL && !walk_node(walk, state, child, terminated)) return false;
    }
    return !walk->failed;
}

/* A nested body that may run later or elsewhere sees the current moves; its
 * own moves do not flow back. */
static bool walk_detached(MoveWalk *walk, const MoveState *state, ASTNode *node)
{
    MoveState copy;
    if (!state_copy(walk, &copy, state)) return false;
    bool terminated = false;
    bool ok = walk_children(walk, &copy, node, &terminated);
    state_free(&copy);
    return ok;
}

static bool walk_branches(MoveWalk *walk, MoveState *state, ASTNode **arms, size_t count,
    bool fallthrough, bool *terminated)
{
    MoveState merged = {0};
    bool any_live = false;
    if (fallthrough) { if (!state_copy(walk, &merged, state)) return false; any_live = true; }
    for (size_t a = 0; a < count; a++) {
        MoveState arm;
        if (!state_copy(walk, &arm, state)) { state_free(&merged); return false; }
        bool arm_terminated = false;
        if (arms[a] != NULL && !walk_node(walk, &arm, arms[a], &arm_terminated)) {
            state_free(&arm); state_free(&merged); return false;
        }
        if (!arm_terminated) { any_live = true; if (!state_union(walk, &merged, &arm)) { state_free(&arm); state_free(&merged); return false; } }
        state_free(&arm);
    }
    state_free(state);
    *state = merged;
    *terminated = !any_live;
    return !walk->failed;
}

static bool walk_loop(MoveWalk *walk, MoveState *state, ASTNode *condition, ASTNode *body)
{
    MoveState entry;
    if (!state_copy(walk, &entry, state)) return false;
    for (int pass = 0; pass < 2; pass++) {
        if (!walk_expression(walk, state, condition)) { state_free(&entry); return false; }
        bool terminated = false;
        if (body != NULL && !walk_node(walk, state, body, &terminated)) { state_free(&entry); return false; }
    }
    bool ok = state_union(walk, state, &entry);
    state_free(&entry);
    return ok;
}

static bool walk_node(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated)
{
    if (node == NULL || walk->failed) return !walk->failed;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t s = 0; s < ast_block_statement_count(node) && !*terminated; s++)
            if (!walk_node(walk, state, ast_block_statement(node, s), terminated)) return false;
        return true;
    case AST_IDENTIFIER: case AST_MEMBER_ACCESS: case AST_ARRAY_ACCESS: {
        char fields[512];
        uint32_t binding = 0;
        if (move_walk_access_path(node, &binding, fields, sizeof(fields))) return walk_expression(walk, state, node);
        /* Not a binding path (a callable name, a call result's field): only
         * its operand expressions can use a moved value. */
        if (node->type == AST_MEMBER_ACCESS) return walk_expression(walk, state, ast_member_object(node));
        if (node->type == AST_ARRAY_ACCESS)
            return walk_expression(walk, state, ast_array_access_array(node)) &&
                walk_expression(walk, state, ast_array_access_index(node));
        return true;
    }
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(node);
        if (!move_walk_note_copied_argument(walk, node)) return false;
        if (callee != NULL && callee->type == AST_MEMBER_ACCESS && !walk_expression(walk, state, ast_member_object(callee)))
            return false;
        for (size_t p = 0; p < ast_call_arg_count(node); p++) {
            walk->argument_call = node;
            walk->argument_ordinal = p;
            bool walked = walk_expression(walk, state, ast_call_argument(node, p));
            walk->argument_call = NULL;
            if (!walked) return false;
        }
        move_walk_check_assignment_loan_call(walk, state, node);
        move_walk_copy_parts_distinct(walk, state, node);
        if (walk->failed || !move_walk_reject_overlapping_moving_arguments(walk, node))
            return false;
        move_walk_apply_call_moves(walk, state, node);
        /* An Array push, set, sort or reverse, an owned String push, a slot
         * write and a HashMap, Set, List or Queue set, removal or push change
         * their receiver in place; a push or set copies its value into it. */
        size_t stored = 0;
        uint32_t kind = 0, holder = 0;
        char fields[512];
        ASTNode *receiver = ast_call_arg_count(node) > 0 ? ast_call_argument(node, 0) : NULL;
        bool store = receiver != NULL && (semantic_param_publication_checked_array_store(walk->ctx, node, &stored) ||
            semantic_param_publication_checked_collection_change(walk->ctx, node, &stored));
        if (store || (receiver != NULL && (semantic_param_publication_checked_slot_write(walk->ctx, node) ||
                semantic_param_publication_checked_array_permutation(walk->ctx, node) ||
                (ast_call_semantic_callee_builtin_kind(node, &kind) && kind == (uint32_t)BUILTIN_ARRAY_PUSH_OWNED_STRING))))
            move_walk_copy_changed(walk, state, receiver);
        if (store && stored != 0 && move_walk_access_path(receiver, &holder, fields, sizeof(fields)))
            move_walk_copy_record(walk, state, ast_call_argument(node, stored), holder, fields);
        /* A call that never returns ends this path, as a return does: what
         * it moved is never used after it. */
        SemanticPublishedValue result = semantic_param_publication_value(walk->ctx, node);
        if (result.ok && type_equals(result.type, TYPE_NEVER)) *terminated = true;
        return !walk->failed;
    }
    case AST_ASSIGNMENT: {
        bool element = false;
        MoveAssignmentBackingLoan loan = {.assignment = node};
        bool loan_started = false;
        ASTNode *target = ast_assignment_target(node);
        /* Evaluate a place exactly as the backends do: receiver first, then
         * each index from inner to outer, then the RHS. The first indexed
         * receiver lends its backing until the final store. */
        ASTNode *stack[128];
        size_t depth = 0;
        for (ASTNode *at = target; at != NULL &&
             (at->type == AST_MEMBER_ACCESS || at->type == AST_ARRAY_ACCESS);) {
            if (depth == sizeof(stack) / sizeof(stack[0])) {
                semantic_error(walk->ctx, at, "Indexed assignment target is too deeply nested");
                break;
            }
            stack[depth++] = at;
            at = at->type == AST_MEMBER_ACCESS ? ast_member_object(at) : ast_array_access_array(at);
            if (at != NULL && at->type != AST_MEMBER_ACCESS && at->type != AST_ARRAY_ACCESS &&
                at->type != AST_IDENTIFIER && !walk_expression(walk, state, at)) return false;
        }
        for (size_t i = depth; i > 0; i--) {
            ASTNode *part = stack[i - 1];
            if (part->type != AST_ARRAY_ACCESS) continue;
            ASTNode *receiver = ast_array_access_array(part);
            check_use(walk, state, receiver);
            if (!loan_started) {
                if (move_walk_access_path(receiver, &loan.binding, loan.fields, sizeof(loan.fields))) {
                    loan.receiver = receiver;
                    loan.previous = walk->assignment_loan;
                    walk->assignment_loan = &loan;
                    loan_started = true;
                } else if (move_walk_reported_once(walk, part)) {
                    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE, part,
                        "Indexed assignment requires a stable named backing path.\n"
                        "Reason:\n"
                        "- the target address must remain valid through index and value evaluation\n"
                        "- this receiver has no current ownership path the final move owner can track\n"
                        "Fix:\n"
                        "- bind the collection to a named owner before assigning through its element");
                }
            }
            element = true;
            if (!walk_expression(walk, state, ast_array_access_index(part))) {
                if (loan_started) walk->assignment_loan = loan.previous;
                return false;
            }
        }
        if (!walk_expression(walk, state, ast_assignment_value(node))) {
            if (loan_started) walk->assignment_loan = loan.previous;
            return false;
        }
        MoveAssignmentBackingLoan *prior = loan_started ? loan.previous : walk->assignment_loan;
        MoveAssignmentBackingLoan *conflict = move_walk_assignment_loan_for_path(
            walk, state, target, prior);
        if (conflict != NULL)
            move_walk_refuse_assignment_loan_change(walk, conflict, target);
        if (loan_started) walk->assignment_loan = loan.previous;
        if (walk->failed) return false;
        /* An element store writes the storage its Array shares. */
        if (element) move_walk_copy_changed(walk, state, target);
        reinitialize(walk, state, target);
        char fields[512];
        uint32_t holder = 0;
        if (move_walk_access_path(target, &holder, fields, sizeof(fields)))
            move_walk_copy_record(walk, state, ast_assignment_value(node), holder, fields);
        return !walk->failed;
    }
    case AST_LET_DECL:
        if (!walk_expression(walk, state, ast_let_initializer(node))) return false;
        /* Each run of a let binds a fresh value. */
        move_walk_copy_rebound(state, ast_node_stable_id(node));
        move_walk_copy_record(walk, state, ast_let_initializer(node), ast_node_stable_id(node), "");
        return !walk->failed;
    case AST_RETURN:
        if (!walk_expression(walk, state, ast_return_value(node))) return false;
        move_walk_copy_inout_at_exit(walk, state, node);
        *terminated = true;
        return true;
    case AST_BREAK: case AST_CONTINUE: case AST_FAIL_STMT:
        if (node->type == AST_FAIL_STMT && !walk_children(walk, state, node, terminated)) return false;
        *terminated = true;
        return true;
    case AST_IF_STMT: {
        if (!walk_expression(walk, state, ast_if_condition(node))) return false;
        ASTNode *arms[2] = {ast_if_then_branch(node), ast_if_else_branch(node)};
        return walk_branches(walk, state, arms, arms[1] != NULL ? 2 : 1, arms[1] == NULL, terminated);
    }
    case AST_WHILE_LOOP:
        return walk_loop(walk, state, ast_while_condition(node), ast_while_body(node));
    case AST_FOR_LOOP:
        if (!walk_expression(walk, state, ast_for_range_start(node)) ||
            !walk_expression(walk, state, ast_for_range_end(node)) ||
            !walk_expression(walk, state, ast_for_iterable(node))) return false;
        return walk_loop(walk, state, NULL, ast_for_body(node));
    case AST_MATCH_STMT: {
        if (!walk_expression(walk, state, ast_match_subject(node))) return false;
        size_t count = ast_match_case_count(node);
        ASTNode **arms = calloc(count + 1, sizeof(*arms));
        if (arms == NULL) { walk->failed = true; return false; }
        for (size_t c = 0; c < count; c++) arms[c] = ast_match_case_at(node, c);
        arms[count] = ast_match_default_body(node);
        bool ok = walk_branches(walk, state, arms, count + 1, false, terminated);
        free(arms);
        return ok;
    }
    case AST_MATCH_CASE:
        if (!walk_expression(walk, state, ast_match_case_guard(node))) return false;
        return walk_node(walk, state, ast_match_case_body(node), terminated);
    case AST_LAMBDA_EXPR: case AST_DEFER_STMT: case AST_ASYNC_BLOCK: case AST_PARALLEL_BLOCK:
        return walk_detached(walk, state, node);
    case AST_FUNC_DECL: case AST_CLASS_DECL: case AST_ENUM_DECL: case AST_TYPE:
        return true;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        if (!walk_children(walk, state, node, terminated)) return false;
        move_walk_copy_parts_distinct(walk, state, node);
        return !walk->failed;
    default:
        return walk_children(walk, state, node, terminated);
    }
}

bool semantic_default_aggregate_move_finalize(SemanticContext *ctx)
{
    SemanticParamPublicationView view;
    if (ctx == NULL || ctx->has_error) return ctx != NULL && !ctx->has_error;
    if (!semantic_param_publication_admitted_view(ctx, &view)) {
        semantic_error(ctx, ctx->program_root, "Default aggregate move check requires the admitted parameter view");
        return false;
    }
    ASTNode *previous_decl = ctx->current_function_decl;
    const char *previous_module = ctx->current_module_path;
    MoveWalk walk = {.ctx = ctx};
    for (size_t r = 0; r < view.routine_count && !walk.failed; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (row->decl == NULL || ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        walk.routine = row->decl;
        MoveState state = {0};
        bool terminated = false;
        (void)walk_node(&walk, &state, ast_func_body(row->decl), &terminated);
        if (!terminated) move_walk_copy_inout_at_exit(&walk, &state, row->decl);
        state_free(&state);
    }
    free(walk.reported);
    free(walk.lent);
    free(walk.copied);
    ctx->current_function_decl = previous_decl;
    ctx->current_module_path = previous_module;
    if (walk.failed) semantic_error(ctx, ctx->program_root, "Default aggregate move check allocation failed");
    return !ctx->has_error;
}
