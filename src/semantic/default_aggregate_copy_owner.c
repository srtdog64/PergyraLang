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
