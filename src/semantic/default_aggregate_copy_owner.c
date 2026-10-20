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

#include "function_param_publication_owner.h"
#include "type_checker_internal.h"

void move_walk_copy_record(MoveWalk *walk, MoveState *state, ASTNode *value, uint32_t carrier)
{
    if (value == NULL || carrier == 0 || walk->failed) return;
    SemanticPublishedValue copied = semantic_param_publication_value(walk->ctx, value);
    if (!copied.ok || !semantic_default_aggregate_value(walk->ctx, copied.type)) return;
    char fields[512];
    uint32_t binding = 0;
    if (move_walk_access_path(value, &binding, fields, sizeof(fields))) {
        (void)move_walk_state_add(walk, state, binding, fields, NULL, NULL, value, carrier);
        return;
    }
    if (value->type == AST_ARRAY_LITERAL || value->type == AST_TUPLE_LITERAL) {
        bool array = value->type == AST_ARRAY_LITERAL;
        for (size_t i = 0; i < (array ? ast_array_literal_count(value) : ast_tuple_literal_count(value)); i++)
            move_walk_copy_record(walk, state, array ? ast_array_literal_element(value, i) : ast_tuple_literal_element(value, i), carrier);
        return;
    }
    /* A constructor, an Option wrap or a Clone that copies only by value
     * returns its argument's storage. */
    for (size_t p = 0; value->type == AST_CALL && p < ast_call_arg_count(value); p++) {
        bool depends = false, retains = false;
        if (semantic_param_publication_callee(walk->ctx, value, p) == NULL &&
            semantic_param_publication_checked_result_relation(walk->ctx, value, p, &depends, &retains) && depends)
            move_walk_copy_record(walk, state, ast_call_argument(value, p), carrier);
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
            if (!source && copy.carrier != work[w].binding) continue;
            done[i] = true;
            Changed other = source ? (Changed){copy.carrier, ""} : (Changed){copy.binding, copy.fields};
            if (!move_walk_state_add(walk, state, other.binding, other.fields, NULL, NULL, copy.lending_call, 0)) break;
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
        if (row->binding == binding || row->carrier == binding) { free(row->fields); continue; }
        state->rows[kept++] = *row;
    }
    state->count = kept;
}
