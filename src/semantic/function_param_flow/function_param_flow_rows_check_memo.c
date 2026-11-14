/* Reuse of a routine's passed rows check.
 *
 * A routine's rows check reads sealed typed facts, its own store state and,
 * for the calls its rows name, the callee entries and frames those rows
 * were built from. Typed facts do not change while the flow store lives. A
 * complete entry and a built frame that is neither building nor pending
 * never change again. A settled routine's frames were built in a demand
 * that completed every callee entry they name, so a check that passed on a
 * settled shape read only such callee state. What can still change is the
 * routine's own shape: a later demand adds an entry or a frame, whose rows
 * the check must then read. So a passed check holds again while its
 * routine's shape keeps the version it had, settled. The callees it reached
 * are checked when the capture reaches them, as before. The checks that
 * read state outside any shape (no-store graphs and read-only receivers)
 * run again on every reuse. */
#include "function_param_flow_summary_internal.h"

#include <stdlib.h>
#include <string.h>

static bool rows_frame_settled(const FunctionParamFlowBackingFact *frame)
{
    return frame == NULL || (frame->epoch != 0 && !frame->building && !frame->pending &&
        (frame->complete || frame->analysis_unsupported));
}

/* Refreshes the shape of index `ordinal` and gives its version, or false
 * when its storage cannot be allocated. */
static bool rows_shape_refresh(FunctionParamFlowSummaryStore *store, size_t ordinal,
    size_t *version, bool *settled)
{
    FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
    FunctionParamFlowRowsShape *shape = &index->rows_shape;
    size_t formals = index->param_count;
    if (!shape->known) {
        shape->entries = malloc((formals == 0 ? 1 : formals) * sizeof(*shape->entries));
        shape->frames = malloc((formals + 1) * sizeof(*shape->frames));
        if (shape->entries == NULL || shape->frames == NULL) {
            free(shape->entries); free(shape->frames);
            shape->entries = NULL; shape->frames = NULL;
            return false;
        }
    }
    bool same = shape->known, ready = true;
    for (size_t p = 0; p < formals; p++) {
        size_t entry = function_param_flow_find(store, index->function_id, p);
        if (entry != SIZE_MAX && store->entries[entry].state != FUNCTION_PARAM_FLOW_COMPLETE) ready = false;
        if (!shape->known || shape->entries[p] != entry) { same = false; shape->entries[p] = entry; }
    }
    for (size_t f = 0; f <= formals; f++) {
        FunctionParamFlowBackingFact *frame = f == formals ? index->declared_result :
            index->backing_frames != NULL && f < index->backing_frame_count ? index->backing_frames[f] : NULL;
        if (!rows_frame_settled(frame)) ready = false;
        if (!shape->known || shape->frames[f] != frame) { same = false; shape->frames[f] = frame; }
    }
    size_t claim_count = index->claims != NULL ? index->claims->count : 0;
    bool claims_failed = index->claims == NULL || index->claims->failed;
    if (shape->claim_count != claim_count || shape->claims_failed != claims_failed ||
        shape->settled != ready) same = false;
    shape->claim_count = claim_count; shape->claims_failed = claims_failed; shape->settled = ready;
    if (!same) shape->version++;
    shape->known = true;
    *version = shape->version;
    *settled = ready;
    return true;
}

static void rows_memo_clear(FunctionParamFlowRowsMemo *memo)
{
    free(memo->callees); free(memo->no_store_routines); free(memo->receiver_calls);
    memset(memo, 0, sizeof(*memo));
}

void function_param_flow_rows_memo_destroy(FunctionParamFlowProgramPointIndex *index)
{
    rows_memo_clear(&index->rows_memo);
    free(index->rows_shape.entries); free(index->rows_shape.frames);
    memset(&index->rows_shape, 0, sizeof(index->rows_shape));
}

/* Each node of one list is in the other; both lists hold every node once. */
static bool rows_memo_same_nodes(ASTNode *const *a, size_t a_count, ASTNode *const *b, size_t b_count)
{
    if (a_count != b_count) return false;
    for (size_t i = 0; i < a_count; i++) {
        size_t j = 0;
        while (j < b_count && b[j] != a[i]) j++;
        if (j == b_count) return false;
    }
    return true;
}

bool function_param_flow_rows_memo_matches(const FunctionParamFlowSummaryStore *store, size_t ordinal,
    const FunctionParamFlowRowsMemo *reached)
{
    const FunctionParamFlowRowsMemo *memo = &store->program_point_indexes[ordinal].rows_memo;
    return memo->passed &&
        rows_memo_same_nodes(memo->callees, memo->callee_count, reached->callees, reached->callee_count) &&
        rows_memo_same_nodes(memo->no_store_routines, memo->no_store_count,
            reached->no_store_routines, reached->no_store_count) &&
        rows_memo_same_nodes(memo->receiver_calls, memo->receiver_count,
            reached->receiver_calls, reached->receiver_count);
}

bool function_param_flow_rows_memo_note(ASTNode ***list, size_t *count, ASTNode *node)
{
    for (size_t i = 0; i < *count; i++) if ((*list)[i] == node) return true;
    if (*count == SIZE_MAX / sizeof(**list)) return false;
    ASTNode **grown = realloc(*list, (*count + 1) * sizeof(**list));
    if (grown == NULL) return false;
    *list = grown;
    (*list)[(*count)++] = node;
    return true;
}

bool function_param_flow_rows_memo_current(FunctionParamFlowSummaryStore *store, size_t ordinal)
{
    size_t version = 0;
    bool settled = false;
    /* A memo is kept only for a settled shape, and any change of the shape,
     * its settledness included, changes its version. */
    if (store->failed || ordinal >= store->program_point_index_count ||
        !store->program_point_indexes[ordinal].rows_memo.passed ||
        !rows_shape_refresh(store, ordinal, &version, &settled) ||
        version != store->program_point_indexes[ordinal].rows_memo.version) return false;
    /* Indexes may move while the repeated checks run; reacquire by ordinal. */
    for (size_t n = 0; n < store->program_point_indexes[ordinal].rows_memo.no_store_count; n++)
        if (!function_param_flow_no_store_graph(store,
                store->program_point_indexes[ordinal].rows_memo.no_store_routines[n])) return false;
    for (size_t c = 0; c < store->program_point_indexes[ordinal].rows_memo.receiver_count; c++)
        if (function_param_flow_receiver_readonly_current(store,
                store->program_point_indexes[ordinal].rows_memo.receiver_calls[c], false) == NULL) return false;
    return !store->failed;
}

void function_param_flow_rows_memo_record(FunctionParamFlowSummaryStore *store, size_t ordinal,
    FunctionParamFlowRowsMemo *reached, bool passed)
{
    FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
    FunctionParamFlowRowsMemo *memo = &index->rows_memo;
    rows_memo_clear(memo);
    size_t version = 0;
    bool settled = false, pending_relations = false;
    for (size_t f = 0; f <= index->backing_frame_count; f++) {
        const FunctionParamFlowBackingFact *frame = f == index->backing_frame_count ? index->declared_result :
            index->backing_frames != NULL ? index->backing_frames[f] : NULL;
        if (frame != NULL && frame->pending_relation_count != 0) pending_relations = true;
    }
    /* A solve admits unfinished callee entries, which a settled routine's
     * rows no longer name, and skips the check of pending relation targets.
     * So a check in a solve is kept only when no frame has pending relations. */
    if (!passed || store->failed || (store->solving && pending_relations) ||
        !rows_shape_refresh(store, ordinal, &version, &settled) || !settled) {
        rows_memo_clear(reached);
        return;
    }
    *memo = *reached;
    memo->passed = true;
    memo->version = version;
    store->rows_memo_records++;
    memset(reached, 0, sizeof(*reached));
}
