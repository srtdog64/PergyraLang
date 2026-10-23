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

#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "function_param_publication_internal.h"
#include "function_param_publication_owner.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_internal.h"

void move_walk_copy_record(MoveWalk *walk, MoveState *state, ASTNode *value, uint32_t carrier,
    const char *carrier_fields)
{
    if (value == NULL || carrier == 0 || walk->failed) return;
    SemanticPublishedValue copied = semantic_param_publication_value(walk->ctx, value);
    if (!copied.ok || !semantic_default_aggregate_value(walk->ctx, copied.type)) return;
    char fields[512];
    uint32_t binding = 0;
    if (move_walk_access_path(value, &binding, fields, sizeof(fields))) {
        (void)move_walk_state_add(walk, state, binding, fields, NULL, NULL, value, carrier, carrier_fields, NULL);
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
     * returns its argument's storage. */
    for (size_t p = 0; value->type == AST_CALL && p < ast_call_arg_count(value); p++) {
        bool depends = false, retains = false;
        if (semantic_param_publication_callee(walk->ctx, value, p) == NULL &&
            semantic_param_publication_checked_result_relation(walk->ctx, value, p, &depends, &retains) && depends)
            move_walk_copy_record(walk, state, ast_call_argument(value, p), carrier, carrier_fields);
    }
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

void move_walk_copy_changed(MoveWalk *walk, MoveState *state, const ASTNode *path)
{
    char fields[512];
    uint32_t binding = 0;
    if (walk->failed || !move_walk_access_path(path, &binding, fields, sizeof(fields))) return;
    typedef struct { uint32_t binding; const char *fields; } Changed;
    size_t rows = state->count;
    Changed *work = malloc((rows + 1) * sizeof(*work));
    bool *done = calloc(rows + 1, sizeof(*done));
    if (work == NULL || done == NULL) { free(work); free(done); walk->failed = true; return; }
    size_t count = 0;
    work[count++] = (Changed){binding, fields};
    for (size_t w = 0; w < count && !walk->failed; w++)
        for (size_t i = 0; i < rows; i++) {
            if (done[i] || state->rows[i].carrier == 0) continue;
            MovedPath copy = state->rows[i];
            bool source = copy.binding == work[w].binding &&
                (move_walk_path_contains(copy.fields, work[w].fields) || move_walk_path_contains(work[w].fields, copy.fields));
            /* The carrier side shares the storage only at the carrier's path. */
            if (!source && (copy.carrier != work[w].binding ||
                    !(move_walk_path_contains(copy.carrier_fields, work[w].fields) ||
                      move_walk_path_contains(work[w].fields, copy.carrier_fields)))) continue;
            done[i] = true;
            Changed other = source ? (Changed){copy.carrier, copy.carrier_fields} : (Changed){copy.binding, copy.fields};
            if (!source) copy_changed_under_loan(walk, state, path, &copy, other.binding, other.fields);
            if (!move_walk_state_add(walk, state, other.binding, other.fields, NULL, NULL, copy.lending_call, 0, NULL, NULL))
                break;
            work[count++] = other;
        }
    free(work);
    free(done);
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
            if (!semantic_loan_copy_landing_select(walk->ctx, row->lending_call)) walk->failed = true;
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

void move_walk_copy_rebound(MoveState *state, uint32_t binding)
{
    size_t kept = 0;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *row = &state->rows[i];
        if (row->binding == binding || row->carrier == binding) { free(row->fields); free(row->carrier_fields); continue; }
        state->rows[kept++] = *row;
    }
    state->count = kept;
}
