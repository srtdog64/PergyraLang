/* Raw private-view protocol tests, not source ownership admission evidence. */
#include "compiler/collection_owned_local_transition_owner.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks, failures;
static void expect(PgyCollectionTransitionView *view, bool expected, const char *name)
{
    char *error = NULL;
    bool actual = pgy_collection_owned_local_transitions_validate(view, &error);
    checks++;
    if (actual != expected || (!actual && error == NULL)) {
        failures++;
        printf("FAIL %s: actual=%d error=%s\n", name, actual, error != NULL ? error : "none");
    }
    free(error);
}

int main(void)
{
    PgyCollectionOwnershipFact facts[] = {{
        .function_syntax_id = 1, .binding_syntax_id = 10,
        .origin_syntax_id = 11, .element_ownership = PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT,
        .disposition = PGY_COLLECTION_DISPOSITION_LIVE,
        .origin = PGY_COLLECTION_ORIGIN_MAP_KEYS
    }};
    PgyCollectionTransitionOperation ops[] = {
        {PGY_COLLECTION_TRANSITION_BIRTH, 10, 0, 10},
        {PGY_COLLECTION_TRANSITION_DROP, 10, 0, 30},
        {PGY_COLLECTION_TRANSITION_DROP, 10, 0, 31}
    };
    PgyCollectionTransitionBlock block = {true, true, 0, 0, 0, 1};
    PgyCollectionTransitionView view = {1, facts, 1, &block, 1, NULL, 0, ops, 1, 0};
    expect(&view, true, "live birth");
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    expect(&view, false, "live-to-retired flip");
    block.operation_count = view.operation_count = 2;
    expect(&view, true, "birth deepdrop");
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_LIVE;
    expect(&view, false, "retired-to-live flip");
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    block.operation_count = view.operation_count = 3;
    expect(&view, false, "duplicate actual drop");
    block.operation_count = view.operation_count = 1;
    expect(&view, false, "deleted drop retains retired");
    block.operation_count = view.operation_count = 0;
    expect(&view, false, "missing definition");
    block.operation_count = view.operation_count = 2;
    ops[1] = ops[0];
    expect(&view, false, "duplicate static definition");
    ops[1] = (PgyCollectionTransitionOperation){PGY_COLLECTION_TRANSITION_DROP, 10, 0, 30};

    /* The one static fresh allocation is executed repeatedly by the CFG.
     * No-drop LIVE is allowed, but its old live generations cannot be erased
     * by dropping only the final generation. */
    size_t preds[] = {0, 1, 0, 1};
    PgyCollectionTransitionBlock loop[] = {
        {true, false, 0, 0, 0, 0},
        {true, false, 0, 2, 0, 1},
        {true, true, 2, 2, 1, 0}
    };
    view = (PgyCollectionTransitionView){1, facts, 1, loop, 3, preds, 4, ops, 1, 0};
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_LIVE;
    expect(&view, true, "fresh no-drop live generations");
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    expect(&view, false, "loop deleted drop");
    loop[1].operation_count = 2;
    loop[2].operation_start = view.operation_count = 2;
    expect(&view, true, "balanced loop generations");
    loop[1].operation_count = 1;
    loop[2].operation_start = 1;
    loop[2].operation_count = 1;
    /* This exit can only follow a birth, so rejection proves the sticky
     * earlier-live bit, not an unrelated unborn drop path. */
    preds[2] = 1;
    loop[2].predecessor_count = 1;
    view.predecessor_count = 3;
    expect(&view, false, "final drop cannot retire prior live generations");

    size_t branch_preds[] = {0, 0, 1, 2};
    PgyCollectionTransitionBlock branch[] = {
        {true, false, 0, 0, 0, 0},
        {true, false, 0, 1, 0, 2},
        {true, false, 1, 1, 2, 0},
        {true, true, 2, 2, 2, 0}
    };
    view = (PgyCollectionTransitionView){1, facts, 1, branch, 4, branch_preds, 4, ops, 2, 0};
    expect(&view, true, "branch-local unborn or retired");
    branch[1].operation_count = 1;
    branch[2].operation_start = branch[3].operation_start = view.operation_count = 1;
    facts[0].disposition = PGY_COLLECTION_DISPOSITION_LIVE;
    expect(&view, true, "branch-local unborn or live");
    branch[0].operation_count = 1;
    branch[1].operation_start = 1;
    branch[1].operation_count = 1;
    branch[2].operation_start = branch[3].operation_start = view.operation_count = 2;
    expect(&view, false, "born live and retired paths cannot join");

    printf("OWNED LOCAL TRANSITION %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
