/* Copies of an access path inside one routine.
 *
 * Copying a struct or collection value shares the storage it holds, though
 * each read of a value is an independent value
 * (docs/semantics/27_ownership_clean.md section 2.1). The move owner
 * records each copy of a path into a binding (a local, an assigned place,
 * the receiver of an Array push or set, or a value built from the path) as
 * a copy row, in execution order. When storage a copy row shares changes in
 * place through either side, the other side, and every copy that shares
 * the storage with them, is refused at its next use, and the next analysis
 * copies the path where it was copied. A side not used after the change
 * needs nothing. */

#include "default_aggregate_move_internal.h"
#include "default_aggregate_move_rule.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "function_param_publication_internal.h"
#include "function_param_publication_owner.h"
#include "loan_copy_landing_owner.h"
#include "slot_summary.h"
#include "type_checker_call_constructor_internal.h"
#include "type_checker_internal.h"

void move_walk_copy_changed_at(MoveWalk *walk, MoveState *state,
    const ASTNode *path, uint32_t binding, const char *fields,
    bool copy_on_reuse);

void move_walk_copy_end(MoveCopyTraversal *traversal)
{
    free(traversal->queue);
    free(traversal->seen);
    *traversal = (MoveCopyTraversal){0};
}

bool move_walk_copy_begin(MoveWalk *walk, const MoveState *state,
    uint32_t binding, const char *fields, MoveCopyTraversal *traversal)
{
    *traversal = (MoveCopyTraversal){0};
    if (state->count == SIZE_MAX || state->count + 1 > SIZE_MAX / sizeof(*traversal->queue)) {
        walk->failed = true;
        return false;
    }
    traversal->queue = malloc((state->count + 1) * sizeof(*traversal->queue));
    traversal->seen = calloc(state->count + 1, sizeof(*traversal->seen));
    if (traversal->queue == NULL || traversal->seen == NULL) {
        move_walk_copy_end(traversal);
        walk->failed = true;
        return false;
    }
    traversal->state = state;
    traversal->row_count = state->count;
    traversal->queue[0] = (MoveCopyReach){binding, fields, SIZE_MAX, false};
    traversal->count = 1;
    traversal->root_pending = true;
    return true;
}

bool move_walk_copy_next(MoveCopyTraversal *traversal, MoveCopyReach *reach)
{
    if (traversal->root_pending) {
        traversal->root_pending = false;
        *reach = traversal->queue[0];
        return true;
    }
    while (traversal->parent < traversal->count) {
        MoveCopyReach path = traversal->queue[traversal->parent];
        while (traversal->row < traversal->row_count) {
            size_t i = traversal->row++;
            /* Resolve the row again: the previous consumer may have grown
             * state->rows. Field strings themselves belong to the state. */
            const MovedPath *copy = &traversal->state->rows[i];
            if (traversal->seen[i] || copy->carrier == 0) continue;
            bool source = copy->binding == path.binding &&
                (move_walk_path_contains(copy->fields, path.fields) ||
                 move_walk_path_contains(path.fields, copy->fields));
            bool carrier = copy->carrier == path.binding &&
                (move_walk_path_contains(copy->carrier_fields, path.fields) ||
                 move_walk_path_contains(path.fields, copy->carrier_fields));
            if (!source && !carrier) continue;
            traversal->seen[i] = true;
            *reach = source ? (MoveCopyReach){copy->carrier, copy->carrier_fields, i, true}
                : (MoveCopyReach){copy->binding, copy->fields, i, false};
            traversal->queue[traversal->count++] = *reach;
            return true;
        }
        traversal->parent++;
        traversal->row = 0;
    }
    return false;
}

/* `made_at` is the expression a copy landing would clone: the value itself,
 * or the call whose result shares the value's storage. */
static void copy_record_made_at(MoveWalk *walk, MoveState *state, ASTNode *value, ASTNode *made_at,
    uint32_t carrier, const char *carrier_fields)
{
    if (value == NULL || carrier == 0 || walk->failed) return;
    SemanticPublishedValue copied = semantic_param_publication_value(walk->ctx, value);
    if (!copied.ok || !semantic_default_aggregate_value(walk->ctx, copied.type)) return;
    char fields[512];
    uint32_t binding = 0;
    if (move_walk_access_path(value, &binding, fields, sizeof(fields))) {
        MovedPath outward_cause = {0};
        bool has_outward_cause = false;
        for (size_t i = 0; i < state->count; i++) {
            const MovedPath *row = &state->rows[i];
            if (row->carrier == 0 && row->outward_borrow_escape
                && row->binding == binding
                && (move_walk_path_contains(row->fields, fields)
                    || move_walk_path_contains(fields, row->fields))) {
                outward_cause = *row;
                has_outward_cause = true;
                break;
            }
        }
        if (has_outward_cause
            && !move_walk_state_add(walk, state, carrier,
                carrier_fields != NULL ? carrier_fields : "",
                outward_cause.callee, outward_cause.callee_decl,
                outward_cause.param, made_at, 0, NULL,
                outward_cause.argument, false, true))
            return;
        (void)move_walk_state_add(walk, state, binding, fields, NULL, NULL,
            NULL, made_at, carrier, carrier_fields, NULL, false, false);
        return;
    }
    if (value->type == AST_ARRAY_LITERAL || value->type == AST_TUPLE_LITERAL) {
        bool array = value->type == AST_ARRAY_LITERAL;
        for (size_t i = 0; i < (array ? ast_array_literal_count(value) : ast_tuple_literal_count(value)); i++)
            move_walk_copy_record(walk, state, array ? ast_array_literal_element(value, i) : ast_tuple_literal_element(value, i),
                carrier, carrier_fields);
        return;
    }
    /* A checked nominal constructor maps each actual to one distinct result
     * field. Preserve that field identity in the copy graph: two fields may
     * receive the same descriptor, but a later change through one field must
     * select a copy for the other field rather than treating both as the
     * aggregate root. The current snapshot owns named-argument reordering and
     * rejects stale constructor metadata. */
    PublishedNode *published = value->type == AST_CALL ? publication_result_current(walk->ctx, value) : NULL;
    if (published != NULL && published->nominal_result != NULL) {
        const SemanticNominalResultSnapshot *snapshot = published->nominal_result;
        if (carrier_fields == NULL || snapshot->field_count != ast_call_arg_count(value)) {
            walk->failed = true;
            return;
        }
        for (size_t p = 0; p < snapshot->field_count && !walk->failed; p++) {
            const char *name = snapshot->fields[p].field_name;
            size_t base = strlen(carrier_fields);
            size_t length = name != NULL ? strlen(name) : 0;
            if (name == NULL || base > SIZE_MAX - length - 2) {
                walk->failed = true;
                return;
            }
            char *field_path = malloc(base + length + 2);
            if (field_path == NULL) {
                walk->failed = true;
                return;
            }
            memcpy(field_path, carrier_fields, base);
            field_path[base] = '.';
            memcpy(field_path + base + 1, name, length + 1);
            move_walk_copy_record(walk, state, ast_call_argument(value, p), carrier, field_path);
            free(field_path);
        }
        return;
    }
    /* A constructor, an Option wrap or a Clone that copies only by value
     * returns its argument's storage. An Array sort or reverse returns the
     * storage of its receiver place, which it rewrote in place and the
     * caller keeps: that copy is made by the call, so a copy landing clones
     * the call's result and leaves the place as the call changed it. */
    bool permutation = value->type == AST_CALL &&
        semantic_param_publication_checked_array_permutation(walk->ctx, value);
    for (size_t p = 0; value->type == AST_CALL && p < ast_call_arg_count(value); p++) {
        bool depends = false, retains = false;
        ASTNode *argument = ast_call_argument(value, p);
        if (semantic_param_publication_callee(walk->ctx, value, p) == NULL &&
            semantic_param_publication_checked_result_relation(walk->ctx, value, p, &depends, &retains) && depends)
            copy_record_made_at(walk, state, argument, permutation && p == 0 ? value : argument,
                carrier, carrier_fields);
    }
}

void move_walk_copy_record(MoveWalk *walk, MoveState *state, ASTNode *value, uint32_t carrier,
    const char *carrier_fields)
{
    copy_record_made_at(walk, state, value, value, carrier, carrier_fields);
}

/* A change through a copy reaches the copied path's storage. A loan taken
 * from that path still holds the storage, and no later use of the path
 * need follow to refuse it: the loan's owner changed, so the change is
 * refused now and the next analysis copies the loan where it is made
 * (2026-10-22_copy_changed_under_a_loan_is_copied). */
static void copy_changed_under_loan(MoveWalk *walk, const MoveState *state, const ASTNode *path,
    const MovedPath *copy, uint32_t binding, const char *fields)
{
    for (size_t l = 0; l < state->count; l++) {
        const MovedPath *loan = &state->rows[l];
        if (loan->carrier != 0 || loan->callee == NULL || loan->lending_call == NULL || loan->binding != binding ||
            !(move_walk_path_contains(loan->fields, fields) || move_walk_path_contains(fields, loan->fields))) continue;
        if (!semantic_loan_copy_landing_select(walk->ctx, loan->lending_call)) walk->failed = true;
        const ASTNode *root = path;
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        const char *name = root != NULL && root->type == AST_IDENTIFIER ? ast_identifier_name(root) : NULL;
        if (move_walk_reported_once(walk, path))
            semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
                PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, (ASTNode *)path,
                "'%s' changes storage that the result of '%s' on line %u still holds.\n"
                "Reason:\n"
                "- '%s' copies, on line %u, a value that call lent from\n"
                "- a copy shares the storage it holds, so the change may leave that result reading freed storage\n"
                "Fix:\n"
                "- change a fresh copy instead\n"
                "- or finish using that result before the change",
                name != NULL ? name : "value", loan->callee, loan->lending_call->line,
                name != NULL ? name : "value", copy->lending_call != NULL ? copy->lending_call->line : 0);
        return;
    }
}

void move_walk_copy_changed(MoveWalk *walk, MoveState *state,
    const ASTNode *path, bool copy_on_reuse)
{
    char fields[512];
    uint32_t binding = 0;
    if (walk->failed || !move_walk_access_path(path, &binding, fields, sizeof(fields))) return;
    move_walk_copy_changed_at(walk, state, path, binding, fields,
        copy_on_reuse);
}

/* An inout callee changes a struct argument only in the fields whose paths
 * its formal does not preserve; any other argument changes whole. */
void move_walk_copy_inout_changed(MoveWalk *walk, MoveState *state, const ASTNode *arg, ASTNode *callee,
    size_t formal)
{
    char fields[512], path[512];
    uint32_t binding = 0;
    SemanticPublishedValue value = semantic_param_publication_value(walk->ctx, arg);
    ASTNode *decl = value.ok && !value.meta && value.type != NULL && value.type->kind == TYPE_KIND_CLASS ?
        semantic_host_decl_for_type(walk->ctx, value.type) : NULL;
    if (walk->failed || decl == NULL || decl->type != AST_CLASS_DECL ||
        !move_walk_access_path(arg, &binding, fields, sizeof(fields))) {
        move_walk_copy_changed(walk, state, arg, false);
        return;
    }
    for (size_t i = 0; !walk->failed; i++) {
        const char *name = NULL;
        (void)constructor_decl_field_resolved_type_at(decl, i, &name, walk->ctx);
        if (name == NULL) return;
        if (function_param_flow_field_preservation_failures(walk->ctx, callee, formal, name) == 0) continue;
        if ((size_t)snprintf(path, sizeof(path), "%s.%s", fields, name) >= sizeof(path)) {
            move_walk_copy_changed(walk, state, arg, false);
            return;
        }
        move_walk_copy_changed_at(walk, state, arg, binding, path, false);
    }
}

void move_walk_copy_changed_at(MoveWalk *walk, MoveState *state,
    const ASTNode *path, uint32_t binding, const char *fields,
    bool copy_on_reuse)
{
    MoveCopyTraversal traversal;
    MoveCopyReach reach;
    MovedPath outward_cause = {0};
    bool has_outward_cause = false;
    /* The call-move owner records an opaque outward escape before it marks
     * the connected shallow copies changed. Preserve that exact cause on
     * every reached copy; otherwise a prior alias is mistaken for an
     * ordinary independently repairable copy and receives an implicit
     * Clone that invents cleanup authority. */
    for (size_t i = 0; i < state->count; i++) {
        const MovedPath *row = &state->rows[i];
        if (row->carrier == 0 && row->outward_borrow_escape
            && row->binding == binding
            && (move_walk_path_contains(row->fields, fields)
                || move_walk_path_contains(fields, row->fields))) {
            outward_cause = *row;
            has_outward_cause = true;
            break;
        }
    }
    if (!move_walk_copy_begin(walk, state, binding, fields, &traversal)) return;
    while (!walk->failed && move_walk_copy_next(&traversal, &reach)) {
        if (reach.copy_row == SIZE_MAX) continue;
        MovedPath copy = state->rows[reach.copy_row];
        if (!walk->landing_only && !reach.from_source)
            copy_changed_under_loan(walk, state, path, &copy, reach.binding, reach.fields);
        if (!move_walk_state_add(walk, state, reach.binding, reach.fields,
                has_outward_cause ? outward_cause.callee : NULL,
                has_outward_cause ? outward_cause.callee_decl : NULL,
                has_outward_cause ? outward_cause.param : NULL,
                copy.lending_call, 0, NULL, NULL,
                copy_on_reuse, has_outward_cause)) break;
    }
    move_walk_copy_end(&traversal);
}

/* An inout formal is the caller's own place, which the caller reads after
 * the call returns. Where a copy of one of its paths changed since, and the
 * path was not assigned since, that place may hold storage the change
 * freed: the exit is refused, and the next analysis copies the path where
 * it was copied when Clone copies it whole. A by-value formal changed
 * through a copy moves the caller's argument instead (its summary), and an
 * own one is not read again. */
void move_walk_copy_inout_at_exit(MoveWalk *walk, const MoveState *state, ASTNode *exit)
{
    for (size_t p = 0; p < ast_func_param_count(walk->routine); p++) {
        const FuncParam *param = ast_func_param(walk->routine, p);
        if (param == NULL || param->mode != PARAM_MODE_MUT_REF) continue;
        for (size_t i = 0; i < state->count; i++) {
            const MovedPath *row = &state->rows[i];
            /* An inout formal is never moved or lent: either is refused
             * where it is made. Its only other rows are copies changed. */
            if (row->binding != ast_func_param_stable_id(param) || row->carrier != 0 || row->lending_call == NULL)
                continue;
            if (!semantic_loan_copy_landing_select_carrier(
                    walk->ctx, row->lending_call))
                walk->failed = true;
            if (!move_walk_reported_once(walk, exit)) continue;
            semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
                PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, exit,
                "'%s%s' shares its storage with the copy made on line %u, which changed it, when this function returns.\n"
                "Reason:\n"
                "- inout parameter '%s' is the caller's own place, which the caller reads after the call\n"
                "- the change may have freed the storage that place still holds\n"
                "Fix:\n"
                "- assign the changed copy back to '%s%s' before returning\n"
                "- or change a fresh value instead",
                param->name, row->fields, row->lending_call->line, param->name, param->name, row->fields);
        }
    }
}

enum { PART_SOURCES = 32 };
typedef struct { size_t count; bool overflow; uint32_t binding[PART_SOURCES]; char *fields[PART_SOURCES]; } PartStorage;

static ASTNode *value_part(ASTNode *value, size_t i)
{
    return value->type == AST_ARRAY_LITERAL ? ast_array_literal_element(value, i)
        : value->type == AST_TUPLE_LITERAL ? ast_tuple_literal_element(value, i) : ast_call_argument(value, i);
}

static bool part_storage_add(MoveWalk *walk, PartStorage *storage, uint32_t binding, const char *fields)
{
    for (size_t i = 0; i < storage->count; i++)
        if (storage->binding[i] == binding && strcmp(storage->fields[i], fields) == 0) return false;
    if (storage->count == PART_SOURCES) { storage->overflow = true; return false; }
    char *copy = malloc(strlen(fields) + 1);
    if (copy == NULL) { walk->failed = true; return false; }
    strcpy(copy, fields);
    storage->binding[storage->count] = binding;
    storage->fields[storage->count++] = copy;
    return true;
}

/* The storage a part of a value names: its own path and, through each copy
 * row whose holder it names, the path it was copied from, extended by the
 * part's path below what was copied. Empty when the part names no storage. */
static void part_storage_collect(MoveWalk *walk, const MoveState *state, ASTNode *part, PartStorage *storage)
{
    char path[512];
    uint32_t binding = 0;
    /* This analysis is already unusable when a landing is selected.  For the
     * next landed analysis, that exact part is an independent Clone and owns
     * no storage in common with another part of the constructed value. */
    if (semantic_loan_copy_landing_selected(walk->ctx, part)) return;
    SemanticPublishedValue value = semantic_param_publication_value(walk->ctx, part);
    if (!value.ok || value.meta || !semantic_default_aggregate_value(walk->ctx, value.type) ||
        !move_walk_access_path(part, &binding, path, sizeof(path))) return;
    (void)part_storage_add(walk, storage, binding, path);
    for (size_t w = 0; w < storage->count && !walk->failed && !storage->overflow; w++)
        for (size_t i = 0; i < state->count; i++) {
            const MovedPath *row = &state->rows[i];
            const char *held = row->carrier_fields, *named = storage->fields[w];
            if (row->carrier == 0 || row->carrier != storage->binding[w] || held == NULL) continue;
            if (move_walk_path_contains(held, named)) {
                if ((size_t)snprintf(path, sizeof(path), "%s%s", row->fields, named + strlen(held)) >= sizeof(path)) {
                    storage->overflow = true;
                    return;
                }
                (void)part_storage_add(walk, storage, row->binding, path);
            } else if (move_walk_path_contains(named, held)) {
                (void)part_storage_add(walk, storage, row->binding, row->fields);
            }
        }
}

static bool part_storage_overlaps(const PartStorage *a, const PartStorage *b)
{
    if (a->count == 0 || b->count == 0) return false;
    if (a->overflow || b->overflow) return true;
    for (size_t i = 0; i < a->count; i++)
        for (size_t j = 0; j < b->count; j++)
            if (a->binding[i] == b->binding[j] && (move_walk_path_contains(a->fields[i], b->fields[j]) ||
                    move_walk_path_contains(b->fields[j], a->fields[i]))) return true;
    return false;
}

/* Two parts of one value, a nominal constructor's or an Array or tuple
 * literal's, that name the same storage, directly or through a local copy,
 * would leave the value's parts sharing storage that no holder of the value
 * sees. Each part that shares storage with a later one is refused, and
 * copied where the value takes it when Clone copies it whole
 * (2026-10-23_one_storage_in_two_parts_of_a_value_is_copied). */
void move_walk_copy_parts_distinct(MoveWalk *walk, const MoveState *state, ASTNode *value)
{
    PublishedNode *published = value->type == AST_CALL ? publication_result_current(walk->ctx, value) : NULL;
    size_t count = value->type == AST_ARRAY_LITERAL ? ast_array_literal_count(value)
        : value->type == AST_TUPLE_LITERAL ? ast_tuple_literal_count(value)
        : published != NULL && published->nominal_result != NULL ? ast_call_arg_count(value) : 0;
    if (count < 2 || walk->failed) return;
    PartStorage *parts = calloc(count, sizeof(*parts));
    if (parts == NULL) { walk->failed = true; return; }
    for (size_t i = 0; i < count && !walk->failed; i++) part_storage_collect(walk, state, value_part(value, i), &parts[i]);
    for (size_t p = 0; p + 1 < count && !walk->failed; p++)
        for (size_t q = p + 1; q < count; q++) {
            if (!part_storage_overlaps(&parts[p], &parts[q])) continue;
            if (!semantic_loan_copy_landing_select_carrier(walk->ctx, value_part(value, p))) walk->failed = true;
            if (move_walk_reported_once(walk, value_part(value, q)))
                semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
                    PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, value_part(value, q),
                    "Two parts of the value made on line %u share one storage.\n"
                    "Reason:\n"
                    "- each part of a struct or collection value owns the storage it holds\n"
                    "- a holder of the value that changes one part would change or free the other\n"
                    "Fix:\n"
                    "- pass a copy, as Clone(x), to one of them\n"
                    "- or build each part from a fresh value",
                    value->line);
            break;
        }
    for (size_t i = 0; i < count; i++)
        for (size_t k = 0; k < parts[i].count; k++) free(parts[i].fields[k]);
    free(parts);
}

void move_walk_copy_rebound(MoveState *state, uint32_t binding)
{
    size_t kept = 0;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *row = &state->rows[i];
        if (row->binding == binding || row->carrier == binding) {
            if (!state->arena_paths) {
                free(row->fields);
                free(row->carrier_fields);
            }
            continue;
        }
        state->rows[kept++] = *row;
    }
    state->count = kept;
}
