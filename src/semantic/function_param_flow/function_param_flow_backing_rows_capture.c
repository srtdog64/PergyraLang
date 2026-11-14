/* Currency of a routine's backing rows: the routines its frames read and
 * the facts each row was built from, walked once per demand. */
#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../../parser/ast_analysis.h"
#include "../../common/string_compat.h"
#include "../typed_map_query_fact_owner.h"
#include "../../common/match_variant_policy.h"
#include "../indexed_string_borrow_owner.h"

/* Indexes one rows capture already found current. Positions survive index
 * growth; each index's typed claims own its checked-call projection. `slots`
 * finds a declaration's entry (entry + 1, 0 empty) so that a capture over a
 * large callee closure does not scan every entry for each lookup. */
typedef struct {
    ASTNode **decls;
    size_t *positions;
    size_t count, capacity;
    size_t *slots;
    size_t slot_capacity;
    bool verify;
} RowsCaptureIndexes;

/* One rows-capture target reads the routine's formal identities and modes
 * once.  Facts still prove that every formal has exactly one binding; this
 * projection only replaces the former parameter-by-binding nested scans. */
typedef struct {
    uint32_t *ids;
    ParamMode *modes;
    size_t *slots;
    unsigned char *seen;
    size_t count, capacity;
    bool prepared;
} RowsCaptureFormals;

typedef enum {
    ROWS_CAPTURE_ALL,
    ROWS_CAPTURE_FORMAL,
    ROWS_CAPTURE_DECLARED_RESULT
} RowsCaptureTargetKind;

/* A currency edge names the exact backing fact it consumed.  Keeping this
 * identity in the work queue prevents one formal's replay from requiring
 * unrelated formals (or an unrelated declared result) of every reached
 * routine.  ROWS_CAPTURE_ALL preserves the existing routine-wide audit. */
typedef struct {
    ASTNode *decl;
    RowsCaptureTargetKind kind;
    size_t formal;
} RowsCaptureTarget;

/* A rows capture reaches claims only through a sealed program-point index.
 * The index check already replayed the call identity, arity and every saved
 * argument relation for this generation. Keep the remaining lookup local to
 * that sealed view instead of re-walking the AST for every backing edge. */
static bool rows_capture_call_argument_claim(
    FunctionParamFlowSummaryStore *store,
    const FunctionParamFlowProgramPointIndex *index,
    size_t claim_ordinal, ASTNode *call, size_t argument_ordinal,
    ASTNode **callee, size_t *formal, bool *physical, bool verify)
{
    if (callee == NULL || formal == NULL) return false;
    *callee = NULL;
    *formal = SIZE_MAX;
    if (physical != NULL) *physical = false;
    if (store == NULL || index == NULL || index->claims == NULL ||
        index->claims->failed || !index->claims->checked ||
        index->claims->checked_generation != index->claims->generation ||
        index->claims->checked_locals != index->local_count ||
        index->claims->rows == NULL ||
        claim_ordinal >= index->claims->count || call == NULL ||
        call->type != AST_CALL)
        return false;

    const FunctionParamFlowIndexClaim *claim =
        &index->claims->rows[claim_ordinal];
    if (claim->node != call || claim->shape != AST_CALL ||
        !claim->semantic_promoted ||
        argument_ordinal >= claim->call_argument_count ||
        (claim->call_argument_count != 0 && claim->call_arguments == NULL))
        return false;
    const FunctionParamFlowCallArgumentClaim *argument =
        &claim->call_arguments[argument_ordinal];
    if (!argument->admitted ||
        ((argument->callee == NULL) != (argument->formal == SIZE_MAX)))
        return false;

    ASTNode *saved_callee = argument->callee;
    size_t saved_formal = argument->formal;
    bool saved_physical = argument->physical;
    if (verify) {
        ASTNode *replayed_callee = NULL;
        size_t replayed_formal = SIZE_MAX;
        bool replayed_physical = false;
        bool replayed = function_param_flow_call_argument_claim_current(
            index->claims, claim_ordinal, call, argument_ordinal,
            &replayed_callee, &replayed_formal, &replayed_physical);
        if (!replayed || replayed_callee != saved_callee ||
            replayed_formal != saved_formal ||
            replayed_physical != saved_physical) {
            function_param_flow_fail(store, call,
                "sealed call argument projection diverged from its full replay");
            return false;
        }
    }
    *callee = saved_callee;
    *formal = saved_formal;
    if (physical != NULL) *physical = saved_physical;
    return true;
}

/* Typed publication inputs are sealed for the lifetime of the flow store.
 * The program-point index therefore owns this header verdict.  Verification
 * mode deliberately replays the sealed-input check and fails closed if the
 * stored projection ever diverges. */
static bool rows_capture_relation_header_ready(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowProgramPointIndex *index, ASTNode *decl, bool verify)
{
    if (index->relation_header_checked) {
        bool ready = index->relation_header_ready;
        if (!verify) return ready;
        bool replay = semantic_param_publication_routine_relation_header_ready(
            store->ctx, decl);
        if (replay != ready) {
            function_param_flow_fail(store, decl,
                "rows capture relation header projection diverged from sealed inputs");
            return false;
        }
        return replay;
    }
    index->relation_header_ready =
        semantic_param_publication_routine_relation_header_ready(store->ctx, decl);
    index->relation_header_checked = true;
    return index->relation_header_ready;
}

/* One synchronous capture owns one validated view of every reached index.
 * A later top-level currency replay creates a fresh set and checks it again;
 * cycles and shared DAG descendants inside this capture reuse the pointer
 * that this capture already found current. */
static size_t rows_capture_pointer_hash(const void *pointer, uint64_t salt)
{
    uint64_t h = (uint64_t)(uintptr_t)pointer ^ (salt * 0x9E3779B97F4A7C15ull);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    return (size_t)h;
}

/* The (node, id) pairs of one backing frame's node rows. Each slot holds a
 * row ordinal + 1, so a probe compares against the rows themselves. */
typedef struct {
    size_t *slots;
    size_t mask;
} RowsCaptureNodeRows;

static bool rows_capture_node_rows_build(RowsCaptureNodeRows *rows,
    const FunctionParamFlowBackingFact *fact)
{
    size_t capacity = 16;
    while (capacity / 2 < fact->node_count) {
        if (capacity > SIZE_MAX / (2 * sizeof(*rows->slots))) return false;
        capacity *= 2;
    }
    rows->slots = calloc(capacity, sizeof(*rows->slots));
    if (rows->slots == NULL) return false;
    rows->mask = capacity - 1;
    for (size_t n = 0; n < fact->node_count; n++) {
        size_t slot = rows_capture_pointer_hash(fact->nodes[n].node,
            fact->nodes[n].id) & rows->mask;
        while (rows->slots[slot] != 0) slot = (slot + 1) & rows->mask;
        rows->slots[slot] = n + 1;
    }
    return true;
}

static bool rows_capture_node_rows_has(const RowsCaptureNodeRows *rows,
    const FunctionParamFlowBackingFact *fact, const ASTNode *node, uint32_t id)
{
    for (size_t slot = rows_capture_pointer_hash(node, id) & rows->mask;
         rows->slots[slot] != 0; slot = (slot + 1) & rows->mask) {
        const FunctionParamFlowBackingNode *row = &fact->nodes[rows->slots[slot] - 1];
        if (row->node == node && row->id == id) return true;
    }
    return false;
}

/* Slot of `decl` in the entry table: its entry + 1, or the empty slot where
 * it would go. */
static size_t rows_capture_seen_slot(const RowsCaptureIndexes *seen,
    const ASTNode *decl)
{
    size_t mask = seen->slot_capacity - 1;
    size_t slot = rows_capture_pointer_hash(decl, 0) & mask;
    while (seen->slots[slot] != 0 && seen->decls[seen->slots[slot] - 1] != decl)
        slot = (slot + 1) & mask;
    return slot;
}

/* Keeps the entry table at most half full. False only when it could not
 * grow; the caller then keeps the entry uncached, as with the entry arrays. */
static bool rows_capture_seen_reserve(RowsCaptureIndexes *seen)
{
    if (seen->slot_capacity != 0 && (seen->count + 1) * 2 <= seen->slot_capacity)
        return true;
    size_t next = seen->slot_capacity == 0 ? 16 : seen->slot_capacity * 2;
    if (next < seen->slot_capacity || next > SIZE_MAX / sizeof(*seen->slots))
        return false;
    size_t *slots = calloc(next, sizeof(*slots));
    if (slots == NULL) return false;
    free(seen->slots);
    seen->slots = slots;
    seen->slot_capacity = next;
    for (size_t i = 0; i < seen->count; i++)
        seen->slots[rows_capture_seen_slot(seen, seen->decls[i])] = i + 1;
    return true;
}

static bool rows_capture_index(FunctionParamFlowSummaryStore *store, RowsCaptureIndexes *seen,
    ASTNode *decl, FunctionParamFlowProgramPointIndex **index)
{
    size_t reused = SIZE_MAX;
    if (seen->slot_capacity != 0) {
        size_t entry = seen->slots[rows_capture_seen_slot(seen, decl)];
        if (entry != 0) reused = seen->positions[entry - 1];
    }
    if (reused != SIZE_MAX && reused < store->program_point_index_count &&
        store->program_point_indexes[reused].function_decl == decl) {
        *index = &store->program_point_indexes[reused];
        return true;
    }
    if (!function_param_flow_program_point_index_read_sealed(
            store, decl, index) &&
        !function_param_flow_program_point_index_build(store, decl, index))
        return false;
    if (seen->count == seen->capacity) {
        size_t next = seen->capacity == 0 ? 8 : seen->capacity * 2;
        if (next < seen->capacity || next > SIZE_MAX / sizeof(*seen->positions)) {
            return true;
        }
        ASTNode **decls = realloc(seen->decls, next * sizeof(*decls));
        if (decls == NULL) return true;
        seen->decls = decls;
        size_t *positions = realloc(seen->positions, next * sizeof(*positions));
        if (positions == NULL) return true;
        seen->positions = positions;
        seen->capacity = next;
    }
    seen->decls[seen->count] = decl;
    seen->positions[seen->count++] = (size_t)(*index - store->program_point_indexes);
    /* A later entry for the same declaration replaces a stale earlier one. */
    if (rows_capture_seen_reserve(seen))
        seen->slots[rows_capture_seen_slot(seen, decl)] = seen->count;
    return true;
}

static bool rows_capture_target_equal(const RowsCaptureTarget *left,
    const RowsCaptureTarget *right)
{
    return left->decl == right->decl && left->kind == right->kind &&
        (left->kind != ROWS_CAPTURE_FORMAL || left->formal == right->formal);
}

/* The validation work queue, in first-reached order, with a set of the
 * targets already in it (`slots`: position + 1, 0 empty, at most half full)
 * so that reaching a target again costs one lookup, not a queue scan. */
typedef struct {
    RowsCaptureTarget *items;
    size_t count, capacity;
    size_t *slots;
    size_t slot_capacity;
} RowsCaptureQueue;

static size_t rows_capture_queue_slot(const RowsCaptureQueue *queue,
    const RowsCaptureTarget *target)
{
    size_t mask = queue->slot_capacity - 1;
    size_t formal = target->kind == ROWS_CAPTURE_FORMAL ? target->formal : 0;
    size_t slot = rows_capture_pointer_hash(target->decl,
        (uint64_t)target->kind * 0x10001u + (uint64_t)formal) & mask;
    while (queue->slots[slot] != 0 &&
           !rows_capture_target_equal(&queue->items[queue->slots[slot] - 1], target))
        slot = (slot + 1) & mask;
    return slot;
}

/* Grows the set so one more target keeps it at most half full. */
static bool rows_capture_queue_reserve_slots(RowsCaptureQueue *queue)
{
    if ((queue->count + 1) * 2 <= queue->slot_capacity) return true;
    size_t next = queue->slot_capacity == 0 ? 16 : queue->slot_capacity * 2;
    if (next < queue->slot_capacity || next > SIZE_MAX / sizeof(*queue->slots))
        return false;
    size_t *slots = calloc(next, sizeof(*slots));
    if (slots == NULL) return false;
    free(queue->slots);
    queue->slots = slots;
    queue->slot_capacity = next;
    for (size_t i = 0; i < queue->count; i++)
        queue->slots[rows_capture_queue_slot(queue, &queue->items[i])] = i + 1;
    return true;
}

/* Appends `target` to the validation queue unless it is already there. */
static bool rows_capture_enqueue(FunctionParamFlowSummaryStore *store,
    ASTNode *current, RowsCaptureQueue *queue, RowsCaptureTarget target)
{
    if (!rows_capture_queue_reserve_slots(queue)) {
        function_param_flow_fail(store, current,
            "backing validation queue set allocation failed");
        return false;
    }
    size_t slot = rows_capture_queue_slot(queue, &target);
    if (queue->slots[slot] != 0) return true;
    if (queue->count == queue->capacity) {
        if (queue->capacity > SIZE_MAX / 2 / sizeof(*queue->items)) {
            function_param_flow_fail(store, current,
                "backing validation queue size overflow");
            return false;
        }
        RowsCaptureTarget *grown = realloc(queue->items,
            queue->capacity * 2 * sizeof(*queue->items));
        if (grown == NULL) {
            function_param_flow_fail(store, current,
                "backing validation queue allocation failed");
            return false;
        }
        queue->items = grown;
        queue->capacity *= 2;
    }
    queue->items[queue->count++] = target;
    queue->slots[slot] = queue->count;
    return true;
}

static bool rows_capture_declaration_claim_current(
    const FunctionParamFlowIndexClaims *claims, uint32_t binding)
{
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &claims->rows[c];
        if (claim->id != binding || claim->node == NULL ||
            ast_node_stable_id(claim->node) != binding) continue;
        if (claim->shape == AST_LET_DECL && claim->node->type == AST_LET_DECL)
            return true;
        if (claim->shape == AST_FOR_LOOP && claim->node->type == AST_FOR_LOOP &&
            ast_for_variable(claim->node) != NULL)
            return true;
        if (claim->shape == AST_IDENTIFIER && claim->node->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(claim->node) == binding)
            return true;
    }
    return false;
}

static bool rows_capture_declarations_build(
    FunctionParamFlowSummaryStore *store, ASTNode *current,
    FunctionParamFlowIndexClaims *claims)
{
    if (claims != NULL && claims->declaration_index_ready &&
        claims->declaration_index_generation == claims->generation) {
        if ((claims->declaration_index_capacity == 0) !=
            (claims->declaration_index_ids == NULL) ||
            (claims->declaration_index_capacity != 0 &&
             (claims->declaration_index_capacity &
              (claims->declaration_index_capacity - 1)) != 0)) {
            function_param_flow_fail(store, current,
                "rows capture declaration projection storage is invalid");
            return false;
        }
        return true;
    }
    if (claims == NULL || claims->failed || !claims->checked ||
        claims->checked_generation != claims->generation) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection has no checked claim generation");
        return false;
    }
    size_t count = 0;
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &claims->rows[c];
        if (claim->id != 0 && claim->node != NULL &&
            ((claim->shape == AST_LET_DECL &&
              claim->node->type == AST_LET_DECL) ||
             (claim->shape == AST_FOR_LOOP &&
              claim->node->type == AST_FOR_LOOP &&
              ast_for_variable(claim->node) != NULL) ||
             (claim->shape == AST_IDENTIFIER &&
              claim->node->type == AST_IDENTIFIER &&
              claim->binding == claim->id)))
            count++;
    }
    if (count > SIZE_MAX / 2) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection size overflow");
        return false;
    }
    if (count == 0) {
        free(claims->declaration_index_ids);
        claims->declaration_index_ids = NULL;
        claims->declaration_index_capacity = 0;
        claims->declaration_index_generation = claims->generation;
        claims->declaration_index_ready = true;
        return true;
    }
    size_t capacity = 8;
    while (capacity < count * 2) {
        if (capacity > SIZE_MAX / 2) {
            function_param_flow_fail(store, current,
                "rows capture declaration projection size overflow");
            return false;
        }
        capacity *= 2;
    }
    uint32_t *ids = calloc(capacity, sizeof(*ids));
    if (ids == NULL) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection allocation failed");
        return false;
    }
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &claims->rows[c];
        if (claim->id == 0 || claim->node == NULL ||
            !((claim->shape == AST_LET_DECL &&
               claim->node->type == AST_LET_DECL) ||
              (claim->shape == AST_FOR_LOOP &&
               claim->node->type == AST_FOR_LOOP &&
               ast_for_variable(claim->node) != NULL) ||
              (claim->shape == AST_IDENTIFIER &&
               claim->node->type == AST_IDENTIFIER &&
               claim->binding == claim->id)))
            continue;
        size_t slot = ((size_t)claim->id * (size_t)2654435761u) &
            (capacity - 1);
        while (ids[slot] != 0 && ids[slot] != claim->id)
            slot = (slot + 1) & (capacity - 1);
        ids[slot] = claim->id;
    }
    free(claims->declaration_index_ids);
    claims->declaration_index_ids = ids;
    claims->declaration_index_capacity = capacity;
    claims->declaration_index_generation = claims->generation;
    claims->declaration_index_ready = true;
    return true;
}

static bool rows_capture_declaration_current(
    FunctionParamFlowSummaryStore *store, ASTNode *current,
    const FunctionParamFlowIndexClaims *claims,
    uint32_t binding, bool verify)
{
    if (claims == NULL || !claims->declaration_index_ready ||
        claims->declaration_index_generation != claims->generation) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection generation is stale");
        return false;
    }
    if ((claims->declaration_index_capacity == 0) !=
        (claims->declaration_index_ids == NULL) ||
        (claims->declaration_index_capacity != 0 &&
         (claims->declaration_index_capacity &
          (claims->declaration_index_capacity - 1)) != 0)) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection storage is invalid");
        return false;
    }
    bool indexed = false;
    if (claims->declaration_index_capacity != 0) {
        size_t slot = ((size_t)binding * (size_t)2654435761u) &
            (claims->declaration_index_capacity - 1);
        while (claims->declaration_index_ids[slot] != 0 &&
            claims->declaration_index_ids[slot] != binding)
            slot = (slot + 1) & (claims->declaration_index_capacity - 1);
        indexed = claims->declaration_index_ids[slot] == binding;
    }
    if (!verify) return indexed;
    bool current_claim = rows_capture_declaration_claim_current(claims, binding);
    if (current_claim != indexed) {
        function_param_flow_fail(store, current,
            "rows capture declaration projection diverged from typed claims");
        return false;
    }
    return current_claim;
}

static void rows_capture_formals_destroy(RowsCaptureFormals *formals)
{
    if (formals == NULL) return;
    free(formals->ids);
    free(formals->modes);
    free(formals->slots);
    free(formals->seen);
    *formals = (RowsCaptureFormals){0};
}

static bool rows_capture_formals_build(FunctionParamFlowSummaryStore *store,
    ASTNode *current, size_t parameter_count, RowsCaptureFormals *formals)
{
    if (formals->prepared) return true;
    if (parameter_count == 0) {
        formals->prepared = true;
        return true;
    }
    if (parameter_count > SIZE_MAX / 2 ||
        parameter_count > SIZE_MAX / sizeof(*formals->ids) ||
        parameter_count > SIZE_MAX / sizeof(*formals->modes) ||
        parameter_count > SIZE_MAX / sizeof(*formals->seen)) {
        function_param_flow_fail(store, current,
            "rows capture formal projection size overflow");
        return false;
    }
    size_t capacity = 8;
    while (capacity < parameter_count * 2) {
        if (capacity > SIZE_MAX / 2) {
            function_param_flow_fail(store, current,
                "rows capture formal projection size overflow");
            return false;
        }
        capacity *= 2;
    }
    formals->ids = malloc(parameter_count * sizeof(*formals->ids));
    formals->modes = malloc(parameter_count * sizeof(*formals->modes));
    formals->seen = malloc(parameter_count * sizeof(*formals->seen));
    formals->slots = calloc(capacity, sizeof(*formals->slots));
    if (formals->ids == NULL || formals->modes == NULL ||
        formals->seen == NULL || formals->slots == NULL) {
        rows_capture_formals_destroy(formals);
        function_param_flow_fail(store, current,
            "rows capture formal projection allocation failed");
        return false;
    }
    formals->count = parameter_count;
    formals->capacity = capacity;
    for (size_t p = 0; p < parameter_count; p++) {
        FuncParam *param = ast_func_param(current, p);
        if (param == NULL) {
            rows_capture_formals_destroy(formals);
            return false;
        }
        uint32_t id = ast_func_param_stable_id(param);
        if (id == 0) {
            rows_capture_formals_destroy(formals);
            return false;
        }
        size_t slot = ((size_t)id * (size_t)2654435761u) & (capacity - 1);
        while (formals->slots[slot] != 0 &&
               formals->ids[formals->slots[slot] - 1] != id)
            slot = (slot + 1) & (capacity - 1);
        if (formals->slots[slot] != 0) {
            rows_capture_formals_destroy(formals);
            return false;
        }
        formals->ids[p] = id;
        formals->modes[p] = param->mode;
        formals->slots[slot] = p + 1;
    }
    formals->prepared = true;
    return true;
}

static size_t rows_capture_formal_find(const RowsCaptureFormals *formals,
    uint32_t binding)
{
    if (!formals->prepared || formals->capacity == 0 || binding == 0)
        return SIZE_MAX;
    size_t slot = ((size_t)binding * (size_t)2654435761u) &
        (formals->capacity - 1);
    while (formals->slots[slot] != 0) {
        size_t formal = formals->slots[slot] - 1;
        if (formal < formals->count && formals->ids[formal] == binding)
            return formal;
        slot = (slot + 1) & (formals->capacity - 1);
    }
    return SIZE_MAX;
}

/* The summary of a method's receiver formal, as the frame that recorded a
 * summarized receiver edge read it, when no backing frame produced it. */
static bool rows_capture_closed_receiver_summary_ready(
    FunctionParamFlowSummaryStore *store, ASTNode *callee)
{
    size_t at = function_param_flow_find(store, ast_node_stable_id(callee), 0);
    if (at == SIZE_MAX || at >= store->count) return false;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[at];
    return entry->function_decl == callee && !entry->analysis_unsupported &&
        (entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR ||
         entry->output_producer ==
            FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN) &&
        !entry->returned_backing_unproved && !entry->returned_backing_pending &&
        entry->output_dependencies_ready && entry->output_dependencies != NULL &&
        entry->output_count == ast_func_param_count(callee) &&
        (entry->state == FUNCTION_PARAM_FLOW_COMPLETE ||
         (store->solving &&
          (entry->state == FUNCTION_PARAM_FLOW_EVALUATED ||
           (entry->state == FUNCTION_PARAM_FLOW_UNSEEN &&
            at >= store->active_start))));
}

static bool function_param_flow_backing_rows_capture_target(
    FunctionParamFlowSummaryStore *store, ASTNode *decl,
    RowsCaptureTargetKind root_kind, size_t root_formal,
    FunctionParamFlowInputCapture capture, void *user)
{
    if (store == NULL || decl == NULL) return false;
    RowsCaptureQueue queue = {.capacity = 8};
    queue.items = malloc(queue.capacity * sizeof(*queue.items));
    if (queue.items == NULL) {
        function_param_flow_fail(store, decl,
            "backing validation queue allocation failed");
        return false;
    }
    RowsCaptureIndexes seen = {
        .verify = getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL,
    };
    bool valid = rows_capture_enqueue(store, decl, &queue,
        (RowsCaptureTarget){.decl = decl, .kind = root_kind,
            .formal = root_formal});
    for (size_t at = 0; at < queue.count && valid; at++) {
        RowsCaptureTarget target = queue.items[at];
        ASTNode *current = target.decl;
        bool all_rows = target.kind == ROWS_CAPTURE_ALL;
        FunctionParamFlowProgramPointIndex *index;
        if (!rows_capture_index(store, &seen, current, &index) ||
            !function_param_flow_index_roots_current(index) ||
            !rows_capture_relation_header_ready(store, index, current,
                seen.verify)) {
            valid = false;
            break;
        }
        bool declared_result_stores_current = true;
        bool declared_result_index_current = true;
        if (index->declared_result != NULL &&
            (all_rows || target.kind == ROWS_CAPTURE_DECLARED_RESULT)) {
            declared_result_stores_current =
                function_param_flow_no_store_graph_capture(
                    store, current, capture, user);
            if (declared_result_stores_current)
                declared_result_index_current =
                    function_param_flow_program_point_index_read_sealed(
                        store, current, &index);
        }
        if (!declared_result_stores_current ||
            !declared_result_index_current) {
            valid = false;
            break;
        }
        FunctionParamFlowBackingFact **frames = index->backing_frames;
        size_t frame_count = index->backing_frame_count;
        size_t parameter_count = index->param_count;
        FunctionParamFlowIndexClaims *claims = index->claims;
        FunctionParamFlowBackingFact *result = index->declared_result;
        if (capture != NULL && !capture(store->ctx, current, claims, NULL, user)) { valid = false; break; }
        if (frames == NULL && result == NULL) {
            if (frame_count != 0 || !all_rows) {
                valid = false;
            }
            continue;
        }
        if (frame_count != parameter_count || (parameter_count != 0 && frames == NULL) ||
            parameter_count > SIZE_MAX - 63 ||
            claims == NULL) {
            valid = false;
            break;
        }
        /* A passed check whose shapes are unchanged answers again. With
         * PGY_VERIFY_FLOW_CURRENCY_MEMO it is also checked in full, and a
         * divergence fails the store. */
        size_t ordinal = (size_t)(index - store->program_point_indexes);
        bool memo_reused = all_rows && capture == NULL &&
            function_param_flow_rows_memo_current(store, ordinal);
        if (memo_reused) store->rows_memo_reuses++;
        if (memo_reused && !seen.verify) {
            const FunctionParamFlowRowsMemo *memo = &store->program_point_indexes[ordinal].rows_memo;
            for (size_t c = 0; c < memo->callee_count && valid; c++)
                valid = rows_capture_enqueue(store, current, &queue,
                    (RowsCaptureTarget){.decl = memo->callees[c],
                        .kind = ROWS_CAPTURE_ALL, .formal = SIZE_MAX});
            continue;
        }
        RowsCaptureFormals formals = {0};
        FunctionParamFlowRowsMemo memo_notes = {0};
        bool notes_kept = all_rows && capture == NULL;
        size_t input_begin = 0;
        size_t input_end = frame_count + (result != NULL);
        if (target.kind == ROWS_CAPTURE_FORMAL) {
            if (target.formal >= frame_count || frames == NULL ||
                frames[target.formal] == NULL) {
                valid = false;
                rows_capture_formals_destroy(&formals);
                break;
            }
            input_begin = target.formal;
            input_end = target.formal + 1;
        } else if (target.kind == ROWS_CAPTURE_DECLARED_RESULT) {
            if (result == NULL) {
                valid = false;
                rows_capture_formals_destroy(&formals);
                break;
            }
            input_begin = frame_count;
            input_end = frame_count + 1;
        }
        for (size_t input = input_begin; input < input_end && valid; input++) {
            bool result_frame = input == frame_count;
            FunctionParamFlowBackingFact *fact = result_frame ? result : frames[input];
            /* Only an established declared result (built, complete, neither
             * building nor pending) is a premise of a summary:
             * declared_result_frame_current answers any other with NULL, and
             * its callers use the callee's summaries. One an enclosing check
             * is still building (a demand opened inside that build, as
             * between mutually recursive routines), one pending on such a
             * build, or one not built yet may become established later, so a
             * pass that skips it keeps no memo. One built incomplete and not
             * pending is never rebuilt. */
            if (fact == NULL) continue;
            if (result_frame && (fact->building || fact->pending || fact->epoch == 0)) { notes_kept = false; continue; }
            if (result_frame && !fact->complete) continue;
            if (!result_frame && !store->solving && !fact->analysis_unsupported &&
                (fact->epoch == 0 || !fact->complete || fact->building || fact->pending)) {
                bool required = false;
                if (!semantic_param_publication_return_backing_required(store->ctx, current, input, &required) ||
                    required) { valid = false; break; }
            }
            size_t destination_count = result_frame ? 1 : parameter_count;
            size_t destination_words = (destination_count + 63) / 64;
            uint64_t final_mask = destination_count % 64 == 0 ? UINT64_MAX :
                (UINT64_C(1) << (destination_count % 64)) - 1;
            uint32_t input_id = result_frame ? 0 : ast_func_param_stable_id(ast_func_param(current, input));
            if ((result_frame ? (fact->input_id != 0 || fact->input_ordinal != SIZE_MAX ||
                 fact->epoch == 0 || !fact->complete || fact->building || fact->pending ||
                 fact->analysis_unsupported || !fact->private_stores_closed ||
                 fact->pending_relation_count != 0 || !semantic_param_publication_routine_result_current(
                     store->ctx, current, fact->declared_result_type)) :
                (input_id == 0 || fact->input_ordinal != input || fact->input_id != input_id ||
                 fact->declared_result_type != NULL)) ||
                fact->origin_count != 1 || fact->origin_words != 1 ||
                fact->destination_count != destination_count || fact->destination_words != destination_words ||
                fact->claims != claims ||
                !function_param_flow_backing_effect_facets_current(fact) ||
                fact->outputs == NULL ||
                (fact->node_count != 0 && fact->nodes == NULL) ||
                (fact->binding_count != 0 && fact->bindings == NULL) ||
                (fact->pending_relation_count != 0 && fact->pending_relations == NULL) ||
                (fact->sealed_pending_relation_count != 0 && fact->sealed_pending_relation_count != fact->pending_relation_count) ||
                (!fact->pending && !fact->building && fact->pending_relation_count != fact->sealed_pending_relation_count) ||
                !function_param_flow_result_path_valid(&fact->result_path) ||
                (!result_frame && fact->complete && !fact->pending && !fact->building &&
                 (!fact->result_path_ready ||
                  (((fact->outward_effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) &&
                   !fact->result_path.present))) ||
                (fact->outputs[destination_words - 1] & ~final_mask) != 0) {
                valid = false;
                break;
            }
            if (result_frame && (fact->effects[0] != 0 ||
                fact->outward_effects[0] != 0 || fact->outputs[0] != 0)) {
                valid = false;
                break;
            }
            if (fact->analysis_unsupported) {
                /* A row holds the call, or holds the intent call it is a step of. */
                bool owned = false;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = fact->nodes[n].node == fact->unsupported_relation.call &&
                        fact->nodes[n].id == fact->unsupported_relation.call_id;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = function_param_flow_intent_step_call(store->ctx, fact->nodes[n].node,
                        fact->unsupported_relation.call);
                if (!owned || fact->complete || fact->pending || fact->building ||
                    !function_param_flow_unsupported_relation_current(store, &fact->unsupported_relation)) { valid = false; break; }
            }
            /* Each relation's call must be one of the frame's node rows. A
             * frame with many of both probes a hash of its rows; the answer
             * is the one the linear scan gives. */
            RowsCaptureNodeRows node_rows = {0};
            bool hashed_rows = fact->pending_relation_count > 4 &&
                fact->node_count > 32;
            if (hashed_rows && !rows_capture_node_rows_build(&node_rows, fact)) {
                function_param_flow_fail(store, current,
                    "backing relation node index allocation failed");
                valid = false;
                break;
            }
            for (size_t e = 0; e < fact->pending_relation_count && valid; e++) {
                FunctionParamFlowPendingRelation *edge = &fact->pending_relations[e];
                ASTNode *callee = NULL;
                size_t formal = SIZE_MAX;
                bool backing_target = false;
                bool owned = hashed_rows && rows_capture_node_rows_has(
                    &node_rows, fact, edge->call, edge->call_id);
                for (size_t n = 0; !hashed_rows && n < fact->node_count && !owned; n++)
                    owned = fact->nodes[n].node == edge->call &&
                        fact->nodes[n].id == edge->call_id;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = function_param_flow_intent_step_call(
                        store->ctx, fact->nodes[n].node, edge->call);
                valid = owned && edge->call != NULL && edge->call->type == AST_CALL && edge->call_id != 0 &&
                    edge->call_id == ast_node_stable_id(edge->call) && edge->callee_id != 0 &&
                    rows_capture_call_argument_claim(
                        store, index, edge->claim_ordinal, edge->call, edge->ordinal,
                        &callee, &formal, NULL, seen.verify) &&
                    callee != NULL && edge->callee_id == ast_node_stable_id(callee);
                if (valid) {
                    size_t target = function_param_flow_find(store,
                        edge->callee_id, formal);
                    if (target == SIZE_MAX || target >= store->count) {
                        valid = false;
                        break;
                    }
                    FunctionParamFlowSummaryEntry *entry = &store->entries[target];
                    backing_target = entry->output_producer ==
                        FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME;
                    if (store->solving) {
                        bool active = target >= store->active_start;
                        bool provisional = active &&
                            (entry->state == FUNCTION_PARAM_FLOW_COMPUTING ||
                             entry->state == FUNCTION_PARAM_FLOW_UNSEEN);
                        /* The pending edge is exact, but its target has not
                         * reached this pass's fixed point yet.  Keep the
                         * source frame pending; the settlement pass below
                         * replays this same edge after every target is
                         * EVALUATED.  Treating it as stale here prevents the
                         * fixed point from ever producing its own evidence. */
                        if (provisional) continue;
                        valid = entry->function_decl == callee &&
                            (entry->state == FUNCTION_PARAM_FLOW_EVALUATED ||
                             entry->state == FUNCTION_PARAM_FLOW_COMPLETE) &&
                            !entry->analysis_unsupported;
                        if (valid && !backing_target)
                            valid = (entry->output_producer ==
                                        FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR ||
                                     entry->output_producer ==
                                        FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN) &&
                                !entry->returned_backing_pending &&
                                !entry->returned_backing_unproved &&
                                entry->output_dependencies_ready;
                    }
                    if (!valid) break;
                    if (store->solving && !backing_target) {
                        /* A closed target contributes no backing rows.  Its
                         * evaluated summary above is the exact dependency;
                         * demanding a nonexistent formal frame would turn a
                         * read-only helper inside a recursive SCC into a
                         * false stale relation. */
                        continue;
                    }
                    if (!store->solving) {
                        bool required;
                        FunctionParamFlowProgramPointIndex *callee_index = NULL;
                        valid = entry->function_decl == callee && entry->state == FUNCTION_PARAM_FLOW_COMPLETE &&
                            !entry->analysis_unsupported && !entry->returned_backing_pending &&
                            !entry->returned_backing_unproved &&
                            function_param_flow_backing_required(store, callee, formal, &required);
                        if (valid && required &&
                            entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME)
                            valid = entry->output_producer ==
                                    FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR &&
                                rows_capture_index(store, &seen, callee,
                                    &callee_index) &&
                                function_param_flow_unused_formal_current(
                                    callee_index, formal);
                        if (valid && entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
                            if (!entry->output_dependencies_ready || entry->output_dependencies == NULL ||
                                entry->output_count != ast_func_param_count(callee) ||
                                (callee_index == NULL && !rows_capture_index(store,
                                    &seen, callee, &callee_index)) ||
                                callee_index->backing_frames == NULL || formal >= callee_index->backing_frame_count) { valid = false; break; }
                            FunctionParamFlowBackingFact *callee_fact = callee_index->backing_frames[formal];
                            valid = callee_fact != NULL && !callee_fact->analysis_unsupported &&
                                callee_fact->epoch != 0 && callee_fact->complete &&
                                !callee_fact->pending && !callee_fact->building && callee_fact->input_ordinal == formal &&
                                callee_fact->input_id == ast_func_param_stable_id(ast_func_param(callee, formal));
                        }
                    }
                }
                if (valid && !all_rows && backing_target)
                    valid = rows_capture_enqueue(store, current, &queue,
                        (RowsCaptureTarget){.decl = callee,
                            .kind = ROWS_CAPTURE_FORMAL,
                            .formal = formal});
            }
            free(node_rows.slots);
            if (!rows_capture_formals_build(store, current, parameter_count,
                    &formals)) {
                valid = false;
                break;
            }
            if (formals.count != 0)
                memset(formals.seen, 0, formals.count * sizeof(*formals.seen));
            for (size_t b = 0; b < fact->binding_count && valid; b++) {
                FunctionParamFlowBackingBinding *binding = &fact->bindings[b];
                if (binding->binding == 0 || binding->origins == NULL || binding->destinations == NULL ||
                    binding->origin_read_epoch > fact->epoch || binding->destination_read_epoch > fact->epoch ||
                    (binding->origins[0] & ~UINT64_C(1)) != 0 ||
                    (binding->origin_path != NULL &&
                     !function_param_flow_result_path_valid(
                         binding->origin_path)) ||
                    (((binding->origins[0] & UINT64_C(1)) != 0) !=
                     (binding->origin_path != NULL && binding->origin_path->present)) ||
                    (binding->destinations[destination_words - 1] & ~final_mask) != 0) {
                    valid = false;
                    break;
                }
                size_t formal = rows_capture_formal_find(&formals,
                    binding->binding);
                if (formal != SIZE_MAX) {
                    ParamMode mode = formals.modes[formal];
                    if (binding->local || formals.seen[formal] ||
                        (result_frame ?
                            (binding->origins[0] != 0 ||
                             binding->destinations[0] != 0) :
                            ((formal == input &&
                              binding->origins[0] != UINT64_C(1)) ||
                             /* Only a parameter this call owns can be rebound
                              * to hold the input. */
                             (formal != input &&
                              mode != PARAM_MODE_DEFAULT &&
                              mode != PARAM_MODE_OWN &&
                              binding->origins[0] != 0) ||
                             ((mode == PARAM_MODE_REF ||
                               mode == PARAM_MODE_MUT_REF) &&
                              (binding->destinations[formal / 64] &
                               (UINT64_C(1) << (formal % 64))) == 0)))) {
                        valid = false;
                        break;
                    }
                    formals.seen[formal] = 1;
                    continue;
                }
                if (!binding->local) {
                    valid = false;
                    break;
                }
                if (!rows_capture_declarations_build(store, current, claims) ||
                    !rows_capture_declaration_current(store, current, claims,
                        binding->binding, seen.verify))
                    valid = false;
            }
            for (size_t p = 0; p < formals.count && valid; p++)
                if (!formals.seen[p]) valid = false;
            for (size_t n = 0; n < fact->node_count && valid; n++) {
                FunctionParamFlowBackingNode *row = &fact->nodes[n];
                const FunctionParamFlowIndexClaim *claim =
                    row->claim_ordinal < claims->count
                    ? &claims->rows[row->claim_ordinal] : NULL;
                if (row->node == NULL || row->origins == NULL ||
                    (row->origins[0] & ~UINT64_C(1)) != 0 ||
                    row->id == 0 || row->shape != row->node->type || claim == NULL ||
                    claim->node != row->node || claim->id != row->id ||
                    claim->shape != row->shape || !claim->semantic_promoted) {
                    valid = false;
                    break;
                }
                if (row->node->type == AST_IDENTIFIER) {
                    if (!claim->value_ready ||
                        (!claim->value_meta && claim->binding == 0)) {
                        valid = false;
                    }
                }
                if ((row->result_callee_id == 0) != (row->result_type == NULL)) { valid = false; break; }
                if (row->result_callee_id != 0 && row->node->type != AST_CALL) { valid = false; break; }
                if (row->receiver == NULL ? (row->receiver_id != 0 || row->receiver_type != NULL || row->receiver_callee_id != 0 || row->receiver_formal_id != 0 || row->receiver_summarized) : row->node->type != AST_CALL) { valid = false; break; }
                if (row->result_callee_id == 0 ?
                    (row->result_argument_count != 0 || row->result_argument_kinds != NULL) :
                    (row->result_argument_count != claim->call_argument_count ||
                     (row->result_argument_count != 0 && row->result_argument_kinds == NULL))) { valid = false; break; }
                if (row->node->type != AST_CALL) continue;
                if (claim->call_argument_count != 0 && claim->call_arguments == NULL) {
                    valid = false; break;
                }
                if (capture != NULL && !capture(store->ctx, current, NULL, row->node, user)) { valid = false; break; }
                ASTNode *edge_call = row->node;
                uint32_t edge_callee_id = row->result_callee_id;
                const Type *edge_type = row->result_type;
                unsigned char *edge_modes = row->result_argument_kinds;
                size_t edge_arity = claim->call_argument_count;
                bool result_edge = row->result_callee_id != 0;
                bool receiver_edge = row->receiver != NULL;
                /* An intent call depends on the actions its steps call. */
                size_t call_edges = edge_arity + result_edge + receiver_edge;
                /* For an exact frame, ordinary argument edges without a
                 * result relation are already owned by pending_relations
                 * above. They produce no target and no result-kind check
                 * here; only a routine-wide audit needs to enumerate them. */
                size_t edge_begin = !all_rows && !result_edge
                    ? edge_arity : 0;
                size_t intent_edges = claim->call_intent_action_count;
                if (intent_edges != 0 &&
                    (claim->call_intent_action_callees == NULL ||
                     claim->call_intent_action_callee_ids == NULL)) {
                    valid = false; break;
                }
                for (size_t p = edge_begin;
                     p < call_edges + intent_edges && valid; p++) {
                    ASTNode *callee = NULL;
                    bool backing_formal_edge = false;
                    RowsCaptureTarget edge_target = {.decl = NULL,
                        .kind = ROWS_CAPTURE_ALL, .formal = SIZE_MAX};
                    if (p >= call_edges) {
                        size_t action = p - call_edges;
                        callee = claim->call_intent_action_callees[action];
                        valid = callee != NULL &&
                            claim->call_intent_action_callee_ids[action] != 0 &&
                            claim->call_intent_action_callee_ids[action] ==
                                ast_node_stable_id(callee);
                        if (valid)
                            edge_target = (RowsCaptureTarget){.decl = callee,
                                .kind = ROWS_CAPTURE_ALL,
                                .formal = SIZE_MAX};
                    } else if (receiver_edge && p == edge_arity + result_edge) {
                        /* A summarized receiver is revalidated through its
                         * callee's rows, as an argument edge is. */
                        callee = row->receiver_summarized
                            ? claim->call_direct_callee
                            : function_param_flow_receiver_readonly_current(store, edge_call, false);
                        valid = callee != NULL && ast_func_param_count(callee) != 0 &&
                            ast_node_stable_id(callee) == row->receiver_callee_id &&
                            claim->call_receiver_ready && claim->call_receiver == row->receiver &&
                            claim->call_receiver_id == row->receiver_id &&
                            claim->call_receiver_type == row->receiver_type &&
                            ast_func_param_stable_id(ast_func_param(callee, 0)) == row->receiver_formal_id;
                        if (valid && !row->receiver_summarized)
                            notes_kept = notes_kept && function_param_flow_rows_memo_note(
                                &memo_notes.receiver_calls, &memo_notes.receiver_count, edge_call);
                        if (valid && row->receiver_summarized) {
                            /* The edge was recorded from the callee's complete
                             * summary for its receiver formal. A summary with
                             * no backing frame (a closed selector or scalar
                             * return) is itself the dependency; demanding a
                             * formal frame it never had would refuse every
                             * exact check of a routine that calls a read-only
                             * method on a value it returns. */
                            FunctionParamFlowProgramPointIndex *callee_index;
                            bool formal_frame = all_rows ||
                                !rows_capture_index(store, &seen, callee,
                                    &callee_index) ||
                                (callee_index->backing_frames != NULL &&
                                 callee_index->backing_frame_count != 0 &&
                                 callee_index->backing_frames[0] != NULL);
                            if (formal_frame)
                                edge_target = (RowsCaptureTarget){.decl = callee,
                                    .kind = ROWS_CAPTURE_FORMAL, .formal = 0};
                            else
                                valid = rows_capture_closed_receiver_summary_ready(
                                    store, callee);
                        }
                    } else if (result_edge && p == edge_arity) {
                        const Type *type = claim->call_declared_result_type;
                        callee = claim->call_declared_result;
                        FunctionParamFlowProgramPointIndex *callee_index;
                        bool result_edge_current = callee != NULL &&
                            ast_node_stable_id(callee) == edge_callee_id &&
                            type == edge_type &&
                            edge_arity + receiver_edge ==
                                ast_func_param_count(callee) &&
                            rows_capture_index(store, &seen, callee,
                                &callee_index) &&
                            callee_index->declared_result != NULL &&
                            callee_index->declared_result->declared_result_type
                                == type;
                        if (!result_edge_current) {
                            valid = false;
                            break;
                        }
                        edge_target = (RowsCaptureTarget){.decl = callee,
                            .kind = ROWS_CAPTURE_DECLARED_RESULT,
                            .formal = SIZE_MAX};
                    } else {
                        size_t formal = SIZE_MAX;
                        bool physical = false;
                        valid = rows_capture_call_argument_claim(
                            store, index, row->claim_ordinal, edge_call, p,
                            &callee, &formal, &physical, seen.verify);
                        if (valid && result_edge) {
                            unsigned char kind = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED;
                            if (physical)
                                kind = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL;
                            else if (callee != NULL && ast_node_stable_id(callee) == edge_callee_id) {
                                size_t s = function_param_flow_find(store,
                                    edge_callee_id, formal);
                                if (s != SIZE_MAX && s < store->count) {
                                    const FunctionParamFlowSummaryEntry *entry = &store->entries[s];
                                    bool entry_ready = entry->function_decl == callee &&
                                        !entry->analysis_unsupported &&
                                        !entry->returned_backing_unproved &&
                                        !entry->returned_backing_pending &&
                                        entry->output_dependencies_ready &&
                                        entry->output_dependencies != NULL &&
                                        entry->output_count == ast_func_param_count(callee) &&
                                        (entry->state == FUNCTION_PARAM_FLOW_COMPLETE ||
                                         (store->solving &&
                                          (entry->state == FUNCTION_PARAM_FLOW_EVALUATED ||
                                           (entry->state == FUNCTION_PARAM_FLOW_UNSEEN &&
                                            s >= store->active_start))));
                                    if (entry_ready &&
                                        entry->output_producer ==
                                            FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR) {
                                        FunctionParamFlowProgramPointIndex *callee_index;
                                        bool outputs_clear = true;
                                        for (size_t q = 0; q < entry->output_count; q++)
                                            if (entry->output_dependencies[q])
                                                outputs_clear = false;
                                        if (outputs_clear &&
                                            rows_capture_index(store, &seen, callee,
                                                &callee_index) &&
                                            function_param_flow_unused_formal_current(
                                                callee_index, formal))
                                            kind = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT;
                                    }
                                    /* A reused frame is checked before this pass demands
                                     * every callee it names, so an entry of the closure the
                                     * pass has not reached yet answers by its last
                                     * evaluation. A change there marks the closure changed,
                                     * and the seal runs only after a pass with none. */
                                    if (kind == FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED &&
                                        entry_ready &&
                                        entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME &&
                                        entry->output_dependencies_ready) {
                                        FunctionParamFlowProgramPointIndex *callee_index;
                                        bool source_ready = rows_capture_index(store,
                                                &seen, callee, &callee_index) &&
                                            callee_index->backing_frames != NULL &&
                                            formal < callee_index->backing_frame_count;
                                        FunctionParamFlowBackingFact *source =
                                            source_ready
                                            ? callee_index->backing_frames[formal]
                                            : NULL;
                                        source_ready = source_ready && source != NULL &&
                                            source->epoch != 0 && source->complete &&
                                            !source->building && !source->pending &&
                                            !source->analysis_unsupported &&
                                            source->claims == callee_index->claims &&
                                            source->input_ordinal == formal &&
                                            source->input_id == ast_func_param_stable_id(
                                                ast_func_param(callee, formal)) &&
                                            entry->output_count <= SIZE_MAX - 63 &&
                                            source->destination_count == entry->output_count &&
                                            source->destination_words ==
                                                (entry->output_count + 63) / 64 &&
                                            function_param_flow_backing_effect_projection_current(
                                                source, entry->mask) &&
                                            source->outputs != NULL;
                                        bool output_current = source_ready;
                                        for (size_t q = 0;
                                             q < entry->output_count && output_current;
                                             q++)
                                            if ((source->outputs[q / 64] &
                                                    (UINT64_C(1) << (q % 64))) != 0 &&
                                                !entry->output_dependencies[q])
                                                output_current = false;
                                        bool stores_current = output_current &&
                                            (!source->private_stores_closed ||
                                             function_param_flow_no_store_graph_capture(
                                                 store, callee, capture, user));
                                        if (stores_current) {
                                            if (source->private_stores_closed)
                                                notes_kept = notes_kept &&
                                                    function_param_flow_rows_memo_note(
                                                        &memo_notes.no_store_routines,
                                                        &memo_notes.no_store_count,
                                                        callee);
                                            kind = (source->outward_effects[0] &
                                                    SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0
                                                ? FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT
                                                : FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT;
                                            backing_formal_edge = true;
                                        }
                                    }
                                }
                            }
                            valid = kind !=
                                    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED &&
                                kind == edge_modes[p];
                        }
                        if (valid && backing_formal_edge)
                            edge_target = (RowsCaptureTarget){.decl = callee,
                                .kind = ROWS_CAPTURE_FORMAL,
                                .formal = formal};
                    }
                    if (!valid || callee == NULL) continue;
                    notes_kept = notes_kept && function_param_flow_rows_memo_note(
                        &memo_notes.callees, &memo_notes.callee_count, callee);
                    if (all_rows)
                        edge_target = (RowsCaptureTarget){.decl = callee,
                            .kind = ROWS_CAPTURE_ALL, .formal = SIZE_MAX};
                    if (edge_target.decl != NULL)
                        valid = rows_capture_enqueue(store, current, &queue,
                            edge_target);
                }
            }
        }
        if (memo_reused && (!valid ||
                (notes_kept && !function_param_flow_rows_memo_matches(store, ordinal, &memo_notes))))
            function_param_flow_fail(store, current, "rows check memo diverged from its full check");
        rows_capture_formals_destroy(&formals);
        if (all_rows && capture == NULL)
            function_param_flow_rows_memo_record(store, ordinal, &memo_notes,
                valid && notes_kept);
        else {
            free(memo_notes.callees);
            free(memo_notes.no_store_routines);
            free(memo_notes.receiver_calls);
        }
    }
    free(queue.items);
    free(queue.slots);
    free(seen.decls);
    free(seen.positions);
    free(seen.slots);
    return valid;
}

bool function_param_flow_backing_rows_capture(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, FunctionParamFlowInputCapture capture, void *user)
{
    return function_param_flow_backing_rows_capture_target(store, decl,
        ROWS_CAPTURE_ALL, SIZE_MAX, capture, user);
}

bool function_param_flow_declared_result_rows_capture(
    FunctionParamFlowSummaryStore *store, ASTNode *decl,
    FunctionParamFlowInputCapture capture, void *user)
{
    return function_param_flow_backing_rows_capture_target(store, decl,
        ROWS_CAPTURE_DECLARED_RESULT, SIZE_MAX, capture, user);
}

bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    return function_param_flow_backing_rows_capture(store, decl, NULL, NULL);
}

bool function_param_flow_backing_frame_current(
    FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t formal)
{
    return function_param_flow_backing_rows_capture_target(store, decl,
        ROWS_CAPTURE_FORMAL, formal, NULL, NULL);
}

bool function_param_flow_declared_result_rows_current(
    FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    return function_param_flow_declared_result_rows_capture(
        store, decl, NULL, NULL);
}
