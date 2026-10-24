/* Call-local effects for the default aggregate move walk: copied getter
 * results, indexed-assignment backing loans, callee moves and overlapping
 * moving actuals. Control-flow state joining remains in the walk owner. */
#include "default_aggregate_move_internal.h"
#include "default_aggregate_move_rule.h"
#include "loan_copy_landing_owner.h"

#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "function_param_publication_owner.h"
#include "slot_summary.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"

static bool
call_copied(const MoveWalk *walk, const ASTNode *call)
{
    for (size_t i = 0; i < walk->copied_count; i++)
        if (walk->copied[i] == call) return true;
    return false;
}

bool
move_walk_note_copied_argument(MoveWalk *walk, const ASTNode *call)
{
    const ASTNode *argument = semantic_loan_copy_landing_copied_call(
        walk->ctx, call);
    if (argument == NULL) return true;
    if (walk->copied_count == walk->copied_capacity) {
        size_t capacity = walk->copied_capacity == 0
            ? 8 : walk->copied_capacity * 2;
        const ASTNode **grown = realloc(
            walk->copied, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->copied = grown;
        walk->copied_capacity = capacity;
    }
    walk->copied[walk->copied_count++] = argument;
    return true;
}

static bool
assignment_loan_overlaps(const MoveAssignmentBackingLoan *loan,
                         uint32_t binding, const char *fields)
{
    return loan != NULL && loan->binding == binding
        && (move_walk_path_contains(loan->fields, fields)
            || move_walk_path_contains(fields, loan->fields));
}

MoveAssignmentBackingLoan *
move_walk_assignment_loan_for_path(MoveWalk *walk,
    const MoveState *state, const ASTNode *path,
    MoveAssignmentBackingLoan *loans)
{
    char fields[512];
    uint32_t binding = 0;
    if (loans == NULL
        || !move_walk_access_path(path, &binding, fields, sizeof(fields)))
        return NULL;
    MoveCopyTraversal traversal;
    MoveCopyReach reach;
    if (!move_walk_copy_begin(walk, state, binding, fields, &traversal)) return NULL;
    while (move_walk_copy_next(&traversal, &reach)) {
        for (MoveAssignmentBackingLoan *loan = loans;
             loan != NULL; loan = loan->previous) {
            if (assignment_loan_overlaps(
                    loan, reach.binding, reach.fields)) {
                move_walk_copy_end(&traversal);
                return loan;
            }
        }
    }
    move_walk_copy_end(&traversal);
    return NULL;
}

void
move_walk_refuse_assignment_loan_change(MoveWalk *walk,
    const MoveAssignmentBackingLoan *loan, ASTNode *site)
{
    if (loan == NULL || !move_walk_reported_once(walk, site)) return;
    const ASTNode *root = loan->receiver;
    while (root != NULL
        && (root->type == AST_MEMBER_ACCESS
            || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS
            ? ast_member_object(root) : ast_array_access_array(root);
    char display[512];
    const char *name = root != NULL && root->type == AST_IDENTIFIER
        ? ast_identifier_name(root) : NULL;
    if (name != NULL
        && strlen(name) + strlen(loan->fields) < sizeof(display)) {
        strcpy(display, name);
        strcat(display, loan->fields);
        name = display;
    }
    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, site,
        "Cannot change '%s' while an indexed assignment keeps its backing address.\n"
        "Reason:\n"
        "- the assignment evaluates its target before its value\n"
        "- this operation may write, grow, release or transfer the same backing before the store\n"
        "Fix:\n"
        "- finish the backing-changing operation before starting the indexed assignment\n"
        "- or compute the index and value first, then perform the indexed store",
        name != NULL ? name : "collection backing");
}

static bool
call_argument_changes_assignment_backing(MoveWalk *walk,
    ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_call(walk->ctx, call)
        || !semantic_param_publication_checked_argument(
            walk->ctx, call, ordinal))
        return true;
    if (semantic_stdlib_collection_argument_mutates_storage(call, ordinal))
        return true;
    SemanticPublishedValue argument = semantic_param_publication_value(
        walk->ctx, ast_call_argument(call, ordinal));
    if (!argument.ok || argument.meta) return true;
    if (type_equals(argument.type, TYPE_STRING)
        || semantic_classify_ownership_type(argument.type, walk->ctx)
            == OWNERSHIP_TYPE_COPY_ONLY)
        return false;
    uint32_t kind = 0;
    if (ast_call_semantic_callee_builtin_kind(call, &kind)
        && kind == BUILTIN_CLONE)
        return false;
    ASTNode *callee = semantic_param_publication_callee(
        walk->ctx, call, ordinal);
    size_t formal = 0;
    if (callee == NULL)
        return !semantic_param_publication_argument_borrowed_for_call(
            walk->ctx, call, ordinal);
    if (!semantic_param_publication_formal_for_actual(
            walk->ctx, call, ordinal, &formal))
        return true;
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL) return true;
    unsigned summary = function_param_flow_summary_for_call(
        &(SlotFunctionLookup){walk->ctx, walk->ctx->program_root},
        call, ordinal);
    unsigned changes = SLOT_PARAM_SUMMARY_WRITE | SLOT_PARAM_SUMMARY_RELEASE
        | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE
        | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    if (param->mode == PARAM_MODE_MUT_REF || param->mode == PARAM_MODE_OWN
        || (summary & changes) != 0)
        return true;
    return type_is_constructed_named(argument.type, "Array")
        && !function_param_flow_preserves_array_storage(
            walk->ctx, callee, formal);
}

void
move_walk_check_assignment_loan_call(MoveWalk *walk,
    const MoveState *state, ASTNode *call)
{
    if (walk->assignment_loan == NULL) return;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *argument = ast_call_argument(call, p);
        MoveAssignmentBackingLoan *loan = move_walk_assignment_loan_for_path(
            walk, state, argument, walk->assignment_loan);
        if (walk->failed) return;
        if (loan != NULL
            && call_argument_changes_assignment_backing(walk, call, p))
            move_walk_refuse_assignment_loan_change(walk, loan, argument);
    }
}

static ParamMode
routine_param_mode(const ASTNode *routine, uint32_t binding, bool *found)
{
    *found = false;
    for (size_t p = 0;
         routine != NULL && p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL
            && ast_func_param_stable_id(param) == binding) {
            *found = true;
            return param->mode;
        }
    }
    return PARAM_MODE_DEFAULT;
}

void
move_walk_apply_call_moves(MoveWalk *walk, MoveState *state, ASTNode *call)
{
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *arg = ast_call_argument(call, p);
        char fields[512];
        uint32_t binding = 0;
        if (!move_walk_access_path(arg, &binding, fields, sizeof(fields)))
            continue;
        ASTNode *callee = semantic_param_publication_callee(
            walk->ctx, call, p);
        size_t formal = 0;
        if (callee == NULL || !semantic_param_publication_formal_for_actual(
                walk->ctx, call, p, &formal))
            continue;
        /* An inout aggregate changes only the fields its summary says it
         * writes.  Preserve sibling fields instead of turning a field-local
         * mutation into a whole-value storage change. */
        if (ast_func_param(callee, formal)->mode == PARAM_MODE_MUT_REF
            && function_param_flow_array_preservation_failures(
                walk->ctx, callee, formal) != 0)
            move_walk_copy_inout_changed(
                walk, state, arg, callee, formal);
        SemanticPublishedValue value = semantic_param_publication_value(
            walk->ctx, arg);
        if (!value.ok || value.meta
            || !semantic_default_aggregate_value(walk->ctx, value.type)
            || !semantic_default_aggregate_formal_moves(
                walk->ctx, callee, formal))
            continue;
        FuncParam *param = ast_func_param(callee, formal);
        const char *callee_name = ast_declaration_name(callee) != NULL
            ? ast_declaration_name(callee) : "callee";
        const char *param_name = param != NULL && param->name != NULL
            ? param->name : "parameter";
        bool parameter = false;
        ParamMode mode = routine_param_mode(
            walk->routine, binding, &parameter);
        bool lends = semantic_default_aggregate_formal_lends(
            walk->ctx, callee, formal);
        if (parameter && mode == PARAM_MODE_REF && lends) continue;
        if (parameter && mode != PARAM_MODE_DEFAULT
            && mode != PARAM_MODE_OWN) {
            if (!semantic_loan_copy_landing_select_carrier(walk->ctx, arg))
                walk->failed = true;
            if (move_walk_reported_once(walk, arg))
                semantic_error_with_hints(walk->ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE, arg,
                    "Borrowed parameter storage cannot be moved into '%s'.\n"
                    "Reason:\n"
                    "- parameter '%s' of '%s' changes or returns the storage it receives\n"
                    "- this function only borrows the argument from its own caller\n"
                    "Fix:\n"
                    "- take the parameter by value instead of ref or inout\n"
                    "- or pass a fresh value to the call",
                    callee_name, param_name, callee_name);
            continue;
        }
        if (lends && call_copied(walk, call)) continue;
        if (!move_walk_state_add(walk, state, binding, fields,
                callee_name, param_name, lends ? call : NULL, 0,
                NULL, arg))
            return;
        if (!lends) move_walk_copy_changed(walk, state, arg);
    }
}

bool
move_walk_reject_overlapping_moving_arguments(MoveWalk *walk,
                                              ASTNode *call)
{
    for (size_t moving = 0; moving < ast_call_arg_count(call); moving++) {
        ASTNode *moving_argument = ast_call_argument(call, moving);
        SemanticPublishedValue moving_value =
            semantic_param_publication_value(walk->ctx, moving_argument);
        uint32_t moving_binding = 0;
        char moving_fields[512];
        if (!moving_value.ok || moving_value.meta
            || !semantic_default_aggregate_value(walk->ctx, moving_value.type)
            || !move_walk_access_path(moving_argument, &moving_binding,
                moving_fields, sizeof(moving_fields)))
            continue;
        ASTNode *callee = semantic_param_publication_callee(
            walk->ctx, call, moving);
        size_t formal = 0;
        if (callee == NULL || !semantic_param_publication_formal_for_actual(
                walk->ctx, call, moving, &formal)
            || !semantic_default_aggregate_formal_moves(
                walk->ctx, callee, formal))
            continue;
        for (size_t other = 0;
             other < ast_call_arg_count(call); other++) {
            if (other == moving) continue;
            ASTNode *other_argument = ast_call_argument(call, other);
            SemanticPublishedValue other_value =
                semantic_param_publication_value(walk->ctx, other_argument);
            uint32_t other_binding = 0;
            char other_fields[512];
            if (!other_value.ok || other_value.meta
                || !semantic_default_aggregate_value(
                    walk->ctx, other_value.type)
                || !move_walk_access_path(other_argument, &other_binding,
                    other_fields, sizeof(other_fields))
                || moving_binding != other_binding
                || (!move_walk_path_contains(moving_fields, other_fields)
                    && !move_walk_path_contains(
                        other_fields, moving_fields)))
                continue;
            if (move_walk_reported_once(walk, other_argument)) {
                semantic_error_with_hints(walk->ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, other_argument,
                    "A call cannot pass overlapping aggregate storage to moving parameter %zu and argument %zu.\n"
                    "Reason:\n"
                    "- the moving parameter may grow, release, return or retain that storage\n"
                    "- the overlapping argument would then observe the same storage through another path\n"
                    "Fix:\n"
                    "- pass distinct aggregate owners\n"
                    "- or materialize one independent Clone before the call",
                    moving + 1, other + 1);
            }
            return false;
        }
    }
    return true;
}
