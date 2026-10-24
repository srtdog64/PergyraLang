/* Behavioral checks of shared copy reachability and both live consumers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/ast.h"
#include "semantic/default_aggregate_move_internal.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "copy graph line %d: %s\n", __LINE__, #x); } } while (0)

static ASTNode copied_at = { .type = AST_CALL, .line = 1 };

static void clear_state(MoveState *state)
{
    for (size_t i = 0; i < state->count; i++) {
        free(state->rows[i].fields);
        free(state->rows[i].carrier_fields);
    }
    free(state->rows);
    *state = (MoveState){0};
}

static void edge(MoveWalk *walk, MoveState *state,
    uint32_t source, const char *from, uint32_t holder, const char *to)
{
    CHECK(move_walk_state_add(walk, state, source, from,
        NULL, NULL, &copied_at, holder, to, NULL));
}

/* Independent adjacency-matrix closure, not another worklist algorithm. */
static void all_four_binding_graphs(void)
{
    for (unsigned mask = 0; mask < 64; mask++) {
        MoveWalk walk = {0};
        MoveState state = {0};
        bool connected[4][4] = {{false}};
        unsigned bit = 0;
        for (unsigned i = 0; i < 4; i++) {
            connected[i][i] = true;
            for (unsigned j = i + 1; j < 4; j++, bit++) {
                if (!(mask & (1u << bit))) continue;
                connected[i][j] = connected[j][i] = true;
                /* Reverse alternate rows to exercise both directions. */
                edge(&walk, &state, (bit & 1 ? j : i) + 1, "",
                    (bit & 1 ? i : j) + 1, "");
            }
        }
        for (unsigned k = 0; k < 4; k++)
            for (unsigned i = 0; i < 4; i++)
                for (unsigned j = 0; j < 4; j++)
                    connected[i][j] |= connected[i][k] && connected[k][j];
        for (unsigned root = 0; root < 4; root++) {
            MoveCopyTraversal traversal;
            MoveCopyReach reach;
            CHECK(move_walk_copy_begin(&walk, &state, root + 1, "", &traversal));
            unsigned found = 0, visits = 0, expected = 0;
            while (move_walk_copy_next(&traversal, &reach)) {
                CHECK(reach.binding >= 1 && reach.binding <= 4);
                found |= 1u << (reach.binding - 1);
                CHECK(++visits <= state.count + 1);
                if (visits > state.count + 1) break;
            }
            for (unsigned i = 0; i < 4; i++)
                if (connected[root][i]) expected |= 1u << i;
            CHECK(found == expected && !walk.failed);
            move_walk_copy_end(&traversal);
        }
        clear_state(&state);
    }
}

static unsigned reachable(MoveWalk *walk, const MoveState *state,
    uint32_t binding, const char *fields)
{
    MoveCopyTraversal traversal;
    MoveCopyReach reach;
    unsigned found = 0;
    CHECK(move_walk_copy_begin(walk, state, binding, fields, &traversal));
    while (move_walk_copy_next(&traversal, &reach)) found |= 1u << reach.binding;
    move_walk_copy_end(&traversal);
    return found;
}

static void fields_and_reallocation(void)
{
    MoveWalk walk = {0};
    MoveState state = {0};
    edge(&walk, &state, 1, ".left", 2, ".held");
    edge(&walk, &state, 2, ".held", 3, "");
    edge(&walk, &state, 1, ".right", 4, "");
    CHECK(reachable(&walk, &state, 1, ".left") == 14);
    CHECK(reachable(&walk, &state, 1, ".left.child") == 14);
    CHECK(reachable(&walk, &state, 1, ".leftish") == 2);
    CHECK(reachable(&walk, &state, 2, ".other") == 4);
    CHECK(reachable(&walk, &state, 3, "") == 14);
    CHECK(reachable(&walk, &state, 1, ".right") == 18);
    CHECK(reachable(&walk, &state, 1, "") == 30);

    MoveCopyTraversal traversal;
    MoveCopyReach reach;
    CHECK(move_walk_copy_begin(&walk, &state, 1, ".left", &traversal));
    CHECK(move_walk_copy_next(&traversal, &reach) && reach.copy_row == SIZE_MAX);
    CHECK(move_walk_copy_next(&traversal, &reach) && reach.binding == 2 &&
        reach.copy_row == 0 && reach.from_source);
    /* Force a different row address, keeping the state's field strings live. */
    MovedPath *replacement = malloc((state.count + 4) * sizeof(*replacement));
    CHECK(replacement != NULL);
    if (replacement != NULL) {
        memcpy(replacement, state.rows, state.count * sizeof(*replacement));
        free(state.rows);
        state.rows = replacement;
        state.capacity = state.count + 4;
        edge(&walk, &state, 3, "", 5, "");
        CHECK(move_walk_copy_next(&traversal, &reach) && reach.binding == 3 && reach.copy_row == 1);
        CHECK(!move_walk_copy_next(&traversal, &reach));
    }
    move_walk_copy_end(&traversal);
    CHECK(!walk.failed);
    clear_state(&state);

    /* The starting side wins if a whole-binding read overlaps both ends. */
    edge(&walk, &state, 1, ".left", 1, ".right");
    CHECK(move_walk_copy_begin(&walk, &state, 1, "", &traversal));
    CHECK(move_walk_copy_next(&traversal, &reach));
    CHECK(move_walk_copy_next(&traversal, &reach) && reach.from_source &&
        strcmp(reach.fields, ".right") == 0);
    CHECK(!move_walk_copy_next(&traversal, &reach));
    move_walk_copy_end(&traversal);
    clear_state(&state);
}

static void actual_consumers(void)
{
    MoveWalk walk = {0};
    MoveState state = {0};
    ASTNode source = { .type = AST_IDENTIFIER };
    ast_identifier_set_binding_syntax_id(&source, 1);
    edge(&walk, &state, 1, "", 2, "");
    edge(&walk, &state, 2, "", 3, "");
    edge(&walk, &state, 4, "", 5, "");
    MoveAssignmentBackingLoan near = { .binding = 2 };
    MoveAssignmentBackingLoan far = { .binding = 3, .previous = &near };
    CHECK(move_walk_assignment_loan_for_path(&walk, &state, &source, &far) == &near);
    near.binding = 4;
    CHECK(move_walk_assignment_loan_for_path(&walk, &state, &source, &far) == &far);
    far.binding = 5;
    CHECK(move_walk_assignment_loan_for_path(&walk, &state, &source, &far) == NULL);
    move_walk_copy_changed(&walk, &state, &source);
    unsigned changed = 0;
    for (size_t i = 0; i < state.count; i++)
        if (state.rows[i].carrier == 0) changed |= 1u << state.rows[i].binding;
    CHECK(changed == 12 && !walk.failed);
    clear_state(&state);

    /* Capacity overflow fails closed and leaves a safely disposable cursor. */
    state.count = SIZE_MAX;
    MoveCopyTraversal traversal;
    CHECK(!move_walk_copy_begin(&walk, &state, 1, "", &traversal) && walk.failed);
    move_walk_copy_end(&traversal);
}

int main(void)
{
    all_four_binding_graphs();
    fields_and_reallocation();
    actual_consumers();
    printf("copy graph: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
