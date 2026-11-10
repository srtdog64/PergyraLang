#include "collection_owned_local_transition_owner.h"
#include "../common/string_compat.h"
#include <stdio.h>
#include <stdlib.h>

enum { COLLECTION_UNBORN = 1, COLLECTION_OWNED = 2,
       COLLECTION_RETIRED = 4, COLLECTION_PRIOR_LIVE = 8 };

bool
pgy_collection_owned_local_fact_tracked(const PgyCollectionOwnershipFact *fact)
{
    return fact != NULL
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_CALL_RESULT
            || fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
}

static bool
collection_transition_error(char **error, const char *stage, uint32_t binding)
{
    if (error != NULL) {
        char detail[192];
        snprintf(detail, sizeof(detail),
            "Owned local collection transition witness is invalid (binding=%u stage=%s)",
            binding, stage);
        *error = pergyra_strdup(detail);
    }
    return false;
}

static size_t
collection_transition_row(const PgyCollectionTransitionView *view, uint32_t id)
{
    for (size_t row = 0; row < view->binding_count; row++)
        if (view->bindings[row].binding_syntax_id == id)
            return row;
    return SIZE_MAX;
}

static bool
collection_transition_retirement(const PgyCollectionTransitionView *view,
                                 size_t predecessor, size_t successor,
                                 uint32_t binding)
{
    for (size_t i = 0; i < view->retirement_count; i++) {
        const PgyCollectionTransitionRetirement *row =
            &view->retirements[i];
        if (row->predecessor == predecessor && row->successor == successor
            && row->binding_syntax_id == binding)
            return true;
    }
    return false;
}

static bool
collection_transition_binding_retired(
    const PgyCollectionTransitionView *view, uint32_t binding)
{
    for (size_t i = 0; i < view->retirement_count; i++)
        if (view->retirements[i].binding_syntax_id == binding)
            return true;
    return false;
}

static bool
collection_transition_binding_pending(
    const PgyCollectionTransitionView *view, uint32_t binding)
{
    for (size_t i = 0; i < view->pending_retirement_count; i++)
        if (view->pending_retirement_bindings[i] == binding)
            return true;
    return false;
}

bool
pgy_collection_owned_local_transitions_validate(
    const PgyCollectionTransitionView *view, char **error_message)
{
    unsigned char *outputs = NULL;
    unsigned char *state = NULL;
    size_t *definitions = NULL;
    bool valid = false;
    const char *failure = "shape";
    uint32_t failure_binding = 0;
    if (view == NULL || view->function_syntax_id == 0
        || (view->binding_count != 0 && view->bindings == NULL)
        || (view->operation_count != 0 && view->operations == NULL)
        || (view->retirement_count != 0 && view->retirements == NULL)
        || (view->pending_retirement_count != 0
            && view->pending_retirement_bindings == NULL)
        || (view->predecessor_count != 0 && view->predecessors == NULL))
        goto done;
    if (view->binding_count == 0) {
        if (view->operation_count != 0 || view->retirement_count != 0
            || view->pending_retirement_count != 0)
            goto done;
        return true;
    }
    if (view->block_count == 0 || view->blocks == NULL
        || view->entry_block >= view->block_count
        || !view->blocks[view->entry_block].reachable
        || view->binding_count > SIZE_MAX / view->block_count
        || view->binding_count * view->block_count > (SIZE_MAX - 1) / 4)
        goto done;
    definitions = calloc(view->binding_count, sizeof(*definitions));
    outputs = calloc(view->block_count * view->binding_count, 1);
    state = calloc(view->binding_count, 1);
    if (definitions == NULL || outputs == NULL || state == NULL) {
        failure = "allocation";
        goto done;
    }
    for (size_t row = 0; row < view->binding_count; row++) {
        const PgyCollectionOwnershipFact *fact = &view->bindings[row];
        failure_binding = fact->binding_syntax_id;
        if (!pgy_collection_owned_local_fact_tracked(fact)
            || fact->function_syntax_id != view->function_syntax_id
            || fact->binding_syntax_id == 0 || fact->origin_syntax_id == 0
            || (fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
                && fact->disposition != PGY_COLLECTION_DISPOSITION_RETIRED))
            goto done;
        if ((fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
                && (fact->element_ownership != PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                    || fact->source_binding_syntax_id != 0))
            || (fact->origin == PGY_COLLECTION_ORIGIN_CLONE
                && (fact->element_ownership != PGY_STRING_ARRAY_OWNED_ELEMENTS
                    || fact->source_binding_syntax_id != 0))
            || (fact->origin == PGY_COLLECTION_ORIGIN_CALL_RESULT
                && (fact->element_ownership != PGY_STRING_ARRAY_OWNED_ELEMENTS
                    || fact->source_binding_syntax_id != 0))
            || (fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
                && (fact->element_ownership != PGY_STRING_ARRAY_OWNED_ELEMENTS
                    || fact->source_binding_syntax_id != 0)))
            goto done;
        for (size_t prior = 0; prior < row; prior++)
            if (view->bindings[prior].binding_syntax_id == fact->binding_syntax_id)
                goto done;
    }
    failure = "pending-retirement-identity";
    for (size_t i = 0; i < view->pending_retirement_count; i++) {
        uint32_t binding = view->pending_retirement_bindings[i];
        size_t row = collection_transition_row(view, binding);
        failure_binding = binding;
        if (row == SIZE_MAX
            || view->bindings[row].disposition
                != PGY_COLLECTION_DISPOSITION_RETIRED
            || collection_transition_binding_retired(view, binding))
            goto done;
        for (size_t prior = 0; prior < i; prior++)
            if (view->pending_retirement_bindings[prior] == binding)
                goto done;
    }
    size_t expected_operation = 0;
    size_t expected_predecessor = 0;
    for (size_t block = 0; block < view->block_count; block++) {
        const PgyCollectionTransitionBlock *part = &view->blocks[block];
        if (part->operation_start != expected_operation
            || part->operation_count > view->operation_count - expected_operation
            || part->predecessor_start != expected_predecessor
            || part->predecessor_count > view->predecessor_count - expected_predecessor)
            goto done;
        expected_operation += part->operation_count;
        expected_predecessor += part->predecessor_count;
        for (size_t p = 0; p < part->predecessor_count; p++)
            if (view->predecessors[part->predecessor_start + p] >= view->block_count)
                goto done;
        for (size_t i = 0; i < part->operation_count; i++) {
            const PgyCollectionTransitionOperation *op =
                &view->operations[part->operation_start + i];
            size_t row = collection_transition_row(view, op->binding_syntax_id);
            failure_binding = op->binding_syntax_id;
            failure = "operation-identity";
            if (row == SIZE_MAX || op->syntax_id == 0) goto done;
            for (size_t prior = 0; prior < part->operation_start + i; prior++)
                if (view->operations[prior].syntax_id == op->syntax_id)
                    goto done;
            const PgyCollectionOwnershipFact *fact = &view->bindings[row];
            if (op->kind == PGY_COLLECTION_TRANSITION_BIRTH) {
                if (fact->origin == PGY_COLLECTION_ORIGIN_BINDING
                    || op->source_binding_syntax_id != 0
                    || op->syntax_id != fact->binding_syntax_id)
                    goto done;
                definitions[row]++;
            } else if (op->kind == PGY_COLLECTION_TRANSITION_MOVE) {
                size_t source = collection_transition_row(view, op->source_binding_syntax_id);
                if (fact->origin != PGY_COLLECTION_ORIGIN_BINDING
                    || op->syntax_id != fact->binding_syntax_id
                    || op->source_binding_syntax_id != fact->source_binding_syntax_id
                    || source == SIZE_MAX || source == row
                    || view->bindings[source].element_ownership != fact->element_ownership
                    || view->bindings[source].disposition != PGY_COLLECTION_DISPOSITION_RETIRED
                    || view->bindings[source].origin == PGY_COLLECTION_ORIGIN_BINDING)
                    goto done;
                definitions[row]++;
            } else if ((op->kind != PGY_COLLECTION_TRANSITION_DROP
                        && op->kind !=
                            PGY_COLLECTION_TRANSITION_OWNED_STRING_PUSH
                        && op->kind !=
                            PGY_COLLECTION_TRANSITION_OWNED_STRING_SET
                        && op->kind !=
                            PGY_COLLECTION_TRANSITION_OWNED_STRING_POP)
                || op->source_binding_syntax_id != 0) goto done;
        }
    }
    if (expected_operation != view->operation_count
        || expected_predecessor != view->predecessor_count) goto done;
    failure = "retirement-identity";
    for (size_t i = 0; i < view->retirement_count; i++) {
        const PgyCollectionTransitionRetirement *retirement =
            &view->retirements[i];
        failure_binding = retirement->binding_syntax_id;
        if (collection_transition_row(view,
                retirement->binding_syntax_id) == SIZE_MAX
            || retirement->predecessor >= view->block_count
            || (retirement->successor != SIZE_MAX
                && retirement->successor >= view->block_count)
            || !view->blocks[retirement->predecessor].reachable)
            goto done;
        if (retirement->successor == SIZE_MAX) {
            if (!view->blocks[retirement->predecessor].terminal)
                goto done;
        } else {
            const PgyCollectionTransitionBlock *successor =
                &view->blocks[retirement->successor];
            bool edge = false;
            for (size_t p = 0; p < successor->predecessor_count; p++)
                edge |= view->predecessors[successor->predecessor_start + p]
                    == retirement->predecessor;
            if (!edge)
                goto done;
        }
        for (size_t prior = 0; prior < i; prior++)
            if (view->retirements[prior].predecessor
                    == retirement->predecessor
                && view->retirements[prior].successor
                    == retirement->successor
                && view->retirements[prior].binding_syntax_id
                    == retirement->binding_syntax_id)
                goto done;
    }
    failure = "missing-or-duplicate-definition";
    for (size_t row = 0; row < view->binding_count; row++)
        if (definitions[row] != 1) {
            failure_binding = view->bindings[row].binding_syntax_id;
            goto done;
        }
    size_t limit = view->block_count * view->binding_count * 4 + 1;
    bool changed = true;
    for (size_t iteration = 0; changed && iteration < limit; iteration++) {
        changed = false;
        for (size_t block = 0; block < view->block_count; block++) {
            const PgyCollectionTransitionBlock *part = &view->blocks[block];
            if (!part->reachable) continue;
            for (size_t row = 0; row < view->binding_count; row++) {
                state[row] = block == view->entry_block ? COLLECTION_UNBORN : 0;
                for (size_t p = 0; p < part->predecessor_count; p++) {
                    size_t pred = view->predecessors[part->predecessor_start + p];
                    if (view->blocks[pred].reachable) {
                        unsigned char incoming =
                            outputs[pred * view->binding_count + row];
                        if (incoming != 0 &&
                            collection_transition_retirement(view, pred,
                                block,
                                view->bindings[row].binding_syntax_id)) {
                            failure = "invalid-edge-retirement";
                            failure_binding =
                                view->bindings[row].binding_syntax_id;
                            if ((incoming & ~COLLECTION_PRIOR_LIVE)
                                    != COLLECTION_OWNED)
                                goto done;
                            incoming = COLLECTION_RETIRED |
                                (incoming & COLLECTION_PRIOR_LIVE);
                        }
                        state[row] |= incoming;
                    }
                }
            }
            if (state[0] == 0) continue;
            for (size_t i = 0; i < part->operation_count; i++) {
                const PgyCollectionTransitionOperation *op = &view->operations[part->operation_start + i];
                size_t row = collection_transition_row(view, op->binding_syntax_id);
                failure_binding = op->binding_syntax_id;
                failure = "invalid-state-transition";
                if (op->kind == PGY_COLLECTION_TRANSITION_BIRTH) {
                    /* Only an admitted fresh canonical allocation restarts a
                     * generation. A previous live allocation is not cleaned
                     * by this birth or by dropping the new allocation. */
                    unsigned char prior = state[row] & COLLECTION_PRIOR_LIVE;
                    if ((state[row] & COLLECTION_OWNED) != 0)
                        prior = COLLECTION_PRIOR_LIVE;
                    state[row] = COLLECTION_OWNED | prior;
                } else if (op->kind == PGY_COLLECTION_TRANSITION_MOVE) {
                    size_t source = collection_transition_row(view, op->source_binding_syntax_id);
                    if ((state[row] & ~(COLLECTION_UNBORN | COLLECTION_RETIRED)) != 0
                        || state[source] != COLLECTION_OWNED)
                        goto done;
                    state[source] = COLLECTION_RETIRED;
                    state[row] = COLLECTION_OWNED;
                } else if (op->kind ==
                               PGY_COLLECTION_TRANSITION_OWNED_STRING_PUSH
                    || op->kind ==
                               PGY_COLLECTION_TRANSITION_OWNED_STRING_SET
                    || op->kind ==
                               PGY_COLLECTION_TRANSITION_OWNED_STRING_POP) {
                    if ((state[row] & ~COLLECTION_PRIOR_LIVE)
                            != COLLECTION_OWNED)
                        goto done;
                } else {
                    if ((state[row] & ~COLLECTION_PRIOR_LIVE) != COLLECTION_OWNED)
                        goto done;
                    state[row] = COLLECTION_RETIRED | (state[row] & COLLECTION_PRIOR_LIVE);
                }
            }
            for (size_t row = 0; row < view->binding_count; row++) {
                if (!collection_transition_retirement(view, block,
                        SIZE_MAX,
                        view->bindings[row].binding_syntax_id))
                    continue;
                failure = "invalid-terminal-retirement";
                failure_binding = view->bindings[row].binding_syntax_id;
                if ((state[row] & ~COLLECTION_PRIOR_LIVE)
                        != COLLECTION_OWNED)
                    goto done;
                state[row] = COLLECTION_RETIRED |
                    (state[row] & COLLECTION_PRIOR_LIVE);
            }
            for (size_t row = 0; row < view->binding_count; row++) {
                unsigned char *out = &outputs[block * view->binding_count + row];
                if (*out != state[row]) { *out = state[row]; changed = true; }
            }
        }
    }
    failure = "non-convergent-cfg";
    if (changed) goto done;
    failure = "terminal-disposition";
    for (size_t block = 0; block < view->block_count; block++) {
        if (!view->blocks[block].reachable) continue;
        for (size_t row = 0; row < view->binding_count; row++) {
            unsigned char witnessed = outputs[block * view->binding_count + row];
            unsigned char expected =
                (view->bindings[row].disposition ==
                        PGY_COLLECTION_DISPOSITION_RETIRED
                    || collection_transition_binding_retired(view,
                        view->bindings[row].binding_syntax_id))
                ? COLLECTION_RETIRED : COLLECTION_OWNED;
            bool deferred = collection_transition_binding_pending(
                view, view->bindings[row].binding_syntax_id);
            failure_binding = view->bindings[row].binding_syntax_id;
            if (witnessed == 0 || (view->blocks[block].terminal
                    && (!deferred
                        && ((witnessed & ~(COLLECTION_UNBORN | expected |
                                COLLECTION_PRIOR_LIVE)) != 0
                            || (expected == COLLECTION_RETIRED
                                && (witnessed & COLLECTION_PRIOR_LIVE)
                                    != 0)))))
                goto done;
        }
    }
    valid = true;
done:
    free(definitions);
    free(outputs);
    free(state);
    return valid || collection_transition_error(error_message, failure, failure_binding);
}
