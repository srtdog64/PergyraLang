#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "indexed_string_borrow_owner.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool function_param_flow_locals_record(FunctionParamFlowProgramPointIndex *index, ASTNode *node);

static bool function_param_flow_index_private_local(
    const FunctionParamFlowProgramPointIndex *index, uint32_t binding,
    const ASTNode **definition)
{
    if (definition != NULL) *definition = NULL;
    if (index == NULL || binding == 0) return false;
    for (size_t i = 0; i < index->local_count; i++) {
        ASTNode *local = index->local_definitions[i];
        if (index->local_ids[i] != binding || local == NULL ||
            local->type != AST_LET_DECL || ast_node_stable_id(local) != binding ||
            ast_let_is_alias(local)) continue;
        if (definition != NULL) *definition = local;
        return true;
    }
    return false;
}

static bool function_param_flow_index_inout_array_formal(
    SemanticContext *ctx, const FunctionParamFlowProgramPointIndex *index,
    uint32_t binding)
{
    if (ctx == NULL || index == NULL || index->function_decl == NULL ||
        binding == 0 || index->param_count !=
            ast_func_param_count(index->function_decl))
        return false;
    for (size_t formal = 0; formal < index->param_count; formal++) {
        FuncParam *parameter = ast_func_param(index->function_decl, formal);
        if (parameter == NULL || parameter->mode != PARAM_MODE_MUT_REF ||
            ast_func_param_stable_id(parameter) != binding ||
            parameter->type == NULL)
            continue;
        return type_is_constructed_named(
            semantic_type_resolution_lookup_metadata_type_ref(
                ctx, parameter->type), "Array");
    }
    return false;
}

static bool function_param_flow_private_output_call(
    SemanticContext *ctx, const FunctionParamFlowProgramPointIndex *index,
    ASTNode *call, bool require_current, bool allow_forwarded,
    bool *uses_local, bool *uses_forwarded)
{
    if (uses_local != NULL) *uses_local = false;
    if (uses_forwarded != NULL) *uses_forwarded = false;
    if (ctx == NULL || index == NULL || call == NULL || call->type != AST_CALL ||
        index->function_decl == NULL ||
        index->function_id != ast_node_stable_id(index->function_decl))
        return false;
    ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
    if (callee == NULL ||
        !semantic_param_publication_routine_relation_header_ready(ctx, callee))
        return false;
    bool saw_inout = false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    for (size_t formal = 0; formal < ast_func_param_count(callee); formal++) {
        FuncParam *parameter = ast_func_param(callee, formal);
        if (parameter == NULL || parameter->mode == PARAM_MODE_OWN) return false;
        if (parameter->mode != PARAM_MODE_MUT_REF) continue;
        size_t matched = SIZE_MAX;
        for (size_t actual = 0; actual < ast_call_arg_count(call); actual++) {
            size_t selected = SIZE_MAX;
            if (!semantic_param_publication_checked_argument(ctx, call, actual) ||
                !semantic_param_publication_value(ctx,
                    ast_call_argument(call, actual)).ok ||
                !semantic_param_publication_formal_for_actual(
                    ctx, call, actual, &selected))
                return false;
            if (selected != formal) continue;
            if (matched != SIZE_MAX) return false;
            matched = actual;
        }
        if (matched == SIZE_MAX) return false;
        ASTNode *value = ast_call_argument(call, matched);
        const ASTNode *root = NULL, *definition = NULL;
        uint32_t binding = semantic_param_publication_binding(ctx, value);
        if (value == NULL || value->type != AST_IDENTIFIER ||
            !slot_checked_call_argument_root(&lookup, call, matched, &root) ||
            root != value)
            return false;
        bool local = function_param_flow_index_private_local(
                index, binding, &definition) && definition != NULL &&
            definition->type == AST_LET_DECL &&
            !ast_let_is_alias(definition) &&
            ast_let_initializer(definition) != NULL &&
            ast_let_initializer(definition)->type == AST_ARRAY_LITERAL;
        if (local && require_current)
            local = semantic_param_publication_private_array_literal_value(
                    ctx, value, index->function_decl) ||
                semantic_param_publication_private_array_literal_inout_actual_current(
                    ctx, value, index->function_decl, call, callee, matched,
                    formal);
        bool forwarded = !local && allow_forwarded &&
            function_param_flow_index_inout_array_formal(ctx, index, binding);
        if (!local && !forwarded) return false;
        if (local && uses_local != NULL) *uses_local = true;
        if (forwarded && uses_forwarded != NULL) *uses_forwarded = true;
        for (size_t prior = 0; prior < formal; prior++) {
            FuncParam *prior_parameter = ast_func_param(callee, prior);
            if (prior_parameter == NULL ||
                prior_parameter->mode != PARAM_MODE_MUT_REF) continue;
            for (size_t actual = 0; actual < ast_call_arg_count(call); actual++) {
                size_t selected = SIZE_MAX;
                if (!semantic_param_publication_formal_for_actual(
                        ctx, call, actual, &selected)) return false;
                if (selected == prior &&
                    semantic_param_publication_binding(
                        ctx, ast_call_argument(call, actual)) == binding)
                    return false;
            }
        }
        saw_inout = true;
    }
    return saw_inout;
}

/* A direct Array store into this routine's inout formal is an output, but a
 * caller that supplies an exact private local owns that output contextually.
 * This records only the operation shape; the no-store graph owns whether the
 * caller edge actually supplies that permission. */
static bool function_param_flow_contextual_array_output_current(
    SemanticContext *ctx, const FunctionParamFlowProgramPointIndex *index,
    ASTNode *call)
{
    size_t stored = SIZE_MAX;
    if (ctx == NULL || index == NULL || call == NULL ||
        !semantic_param_publication_checked_array_store(ctx, call, &stored) ||
        ast_call_arg_count(call) == 0 || stored >= ast_call_arg_count(call))
        return false;
    ASTNode *receiver = ast_call_argument(call, 0);
    uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    if (binding == 0 ||
        binding != ast_identifier_binding_syntax_id(receiver) ||
        !function_param_flow_index_inout_array_formal(ctx, index, binding))
        return false;
    for (size_t actual = 0; actual < ast_call_arg_count(call); actual++)
        if (!semantic_param_publication_checked_argument(ctx, call, actual) ||
            !semantic_param_publication_value(
                ctx, ast_call_argument(call, actual)).ok)
            return false;
    return true;
}

bool function_param_flow_current_begin(SemanticContext *ctx, bool *previous)
{
    FunctionParamFlowSummaryStore *store = ctx == NULL ? NULL : ctx->function_param_flow_summaries;
    if (previous == NULL || store == NULL || store->failed || store->solving || ctx->has_error ||
        store->ctx != ctx || store->program_root != ctx->program_root ||
        !semantic_param_publication_inputs_ready(ctx)) return false;
    *previous = store->validating_current;
    store->validating_current = true;
    return true;
}
void function_param_flow_current_end(SemanticContext *ctx, bool previous)
{
    ctx->function_param_flow_summaries->validating_current = previous;
}
bool function_param_flow_private_string_receiver_current(SemanticContext *ctx,
    ASTNode *routine, uint32_t binding)
{
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = function_param_flow_private_string_receiver_replay(ctx, routine, binding);
    function_param_flow_current_end(ctx, previous);
    return current;
}
bool function_param_flow_string_result_independent_current(SemanticContext *ctx, const ASTNode *call)
{
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = function_param_flow_string_result_independent_replay(ctx, call);
    function_param_flow_current_end(ctx, previous);
    return current;
}
bool function_param_flow_result_lane_string_owned_current(
    SemanticContext *ctx, const ASTNode *call)
{
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = function_param_flow_result_lane_string_owned_replay(ctx, call);
    function_param_flow_current_end(ctx, previous);
    return current;
}

bool function_param_flow_index_roots_current(const FunctionParamFlowProgramPointIndex *index)
{
    ASTNode *body = ast_func_body(index->function_decl);
    if (body == NULL || body != index->dependency_body || ast_node_stable_id(body) != index->dependency_body_id ||
        (body->type == AST_BLOCK ? ast_block_statement_count(body) : 1) != index->dependency_root_count) return false;
    for (size_t r = 0; r < index->dependency_root_count; r++)
        if (index->dependency_root_ids[r] == 0 ||
            ast_node_stable_id(index->dependency_roots[r]) != index->dependency_root_ids[r] ||
            index->dependency_roots[r] != (body->type == AST_BLOCK ? ast_block_statement(body, r) : body)) return false;
    return true;
}

bool function_param_flow_program_point_index_read_sealed(
    FunctionParamFlowSummaryStore *store, ASTNode *function_decl,
    FunctionParamFlowProgramPointIndex **index_out)
{
    static int verify_currency = -1;
    if (index_out != NULL) *index_out = NULL;
    if (store == NULL || function_decl == NULL || index_out == NULL)
        return false;
    uint32_t function_id = ast_node_stable_id(function_decl);
    size_t at = function_param_flow_program_point_index_find(store,
        function_id);
    if (function_id == 0 || at == SIZE_MAX ||
        at >= store->program_point_index_count)
        return false;
    FunctionParamFlowProgramPointIndex *index =
        &store->program_point_indexes[at];
    FunctionParamFlowIndexClaims *claims = index->claims;
    bool header_current = index->function_decl == function_decl &&
        index->function_id == function_id &&
        index->dependency_roots_ready && claims != NULL &&
        !claims->failed && claims->checked &&
        claims->checked_generation == claims->generation &&
        claims->checked_locals == index->local_count;
    if (!header_current)
        return false;
    bool reused = index->sealed_read_current;
    if (verify_currency < 0)
        verify_currency = getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL;
    bool verify = verify_currency != 0;
    if (reused && !verify) {
        *index_out = index;
        return true;
    }
    bool roots_current = function_param_flow_index_roots_current(index);
    if (reused && !roots_current) {
        function_param_flow_fail(store, function_decl,
            "sealed program-point index receipt diverged from root replay");
        return false;
    }
    if (!roots_current) return false;
    index->sealed_read_current = true;
    *index_out = index;
    return true;
}

static bool
function_param_flow_identifier_matches(const char *candidate, void *userdata)
{
    const char *name = userdata;
    return candidate != NULL && name != NULL && strcmp(candidate, name) == 0;
}

static bool
function_param_flow_has_identifier_ref(const ASTNode *node, const char *name)
{
    return node != NULL && name != NULL
        && ast_contains_identifier_ref(
            node, function_param_flow_identifier_matches, (void *)name);
}

bool
function_param_flow_unused_formal_current(
    const FunctionParamFlowProgramPointIndex *index, size_t formal)
{
    if (index == NULL || index->function_decl == NULL ||
        index->function_decl->type != AST_FUNC_DECL ||
        index->function_id == 0 ||
        index->function_id != ast_node_stable_id(index->function_decl) ||
        formal >= index->param_count ||
        formal >= ast_func_param_count(index->function_decl) ||
        index->root_counts == NULL || index->root_counts[formal] != 0 ||
        !function_param_flow_index_roots_current(index))
        return false;
    FuncParam *parameter = ast_func_param(index->function_decl, formal);
    if (parameter == NULL || parameter->name == NULL ||
        ast_func_param_stable_id(parameter) == 0 ||
        /* Host methods may read receiver fields without spelling `self` in
         * the body.  A name-only absence is therefore not receiver absence. */
        strcmp(parameter->name, "self") == 0)
        return false;
    for (size_t root = 0; root < index->dependency_root_count; root++)
        if (function_param_flow_has_identifier_ref(
                index->dependency_roots[root], parameter->name))
            return false;
    return true;
}

static void function_param_flow_program_point_hash_disable(
    FunctionParamFlowSummaryStore *store)
{
    free(store->program_point_index_hash);
    store->program_point_index_hash = NULL;
    store->program_point_index_hash_capacity = 0;
    store->program_point_index_hash_count = 0;
}

static size_t function_param_flow_program_point_hash_slot(uint32_t function_id,
    size_t capacity)
{
    return ((size_t)function_id * (size_t)2654435761u) & (capacity - 1);
}

/* Rebuild the projection atomically. Allocation failure retires only the
 * optimization; the exact linear owner remains available. */
static bool function_param_flow_program_point_hash_rebuild(
    FunctionParamFlowSummaryStore *store)
{
    if (store == NULL || store->program_point_index_count > SIZE_MAX / 2)
        return false;
    size_t capacity = 16;
    while (capacity < store->program_point_index_count * 2) {
        if (capacity > SIZE_MAX / 2) return false;
        capacity *= 2;
    }
    size_t *slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL) return false;
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        uint32_t id = store->program_point_indexes[i].function_id;
        if (id == 0) {
            free(slots);
            return false;
        }
        size_t slot = function_param_flow_program_point_hash_slot(id,
            capacity);
        while (slots[slot] != 0) {
            size_t existing = slots[slot] - 1;
            if (existing >= store->program_point_index_count ||
                store->program_point_indexes[existing].function_id == id) {
                free(slots);
                return false;
            }
            slot = (slot + 1) & (capacity - 1);
        }
        slots[slot] = i + 1;
    }
    free(store->program_point_index_hash);
    store->program_point_index_hash = slots;
    store->program_point_index_hash_capacity = capacity;
    store->program_point_index_hash_count = store->program_point_index_count;
    return true;
}

static void function_param_flow_program_point_hash_append(
    FunctionParamFlowSummaryStore *store, size_t ordinal)
{
    if (store == NULL || ordinal >= store->program_point_index_count ||
        store->program_point_indexes[ordinal].function_id == 0) {
        function_param_flow_program_point_hash_disable(store);
        return;
    }
    if (store->program_point_index_hash == NULL ||
        store->program_point_index_hash_count + 1 !=
            store->program_point_index_count ||
        store->program_point_index_count >
            store->program_point_index_hash_capacity / 2) {
        if (!function_param_flow_program_point_hash_rebuild(store))
            function_param_flow_program_point_hash_disable(store);
        return;
    }
    uint32_t id = store->program_point_indexes[ordinal].function_id;
    size_t slot = function_param_flow_program_point_hash_slot(id,
        store->program_point_index_hash_capacity);
    while (store->program_point_index_hash[slot] != 0)
        slot = (slot + 1) &
            (store->program_point_index_hash_capacity - 1);
    store->program_point_index_hash[slot] = ordinal + 1;
    store->program_point_index_hash_count++;
}

size_t
function_param_flow_program_point_index_find(
    const FunctionParamFlowSummaryStore *store,
    uint32_t function_id)
{
    if (store == NULL || function_id == 0)
        return SIZE_MAX;
    if (store->program_point_index_hash != NULL &&
        store->program_point_index_hash_capacity != 0 &&
        store->program_point_index_hash_count ==
            store->program_point_index_count) {
        size_t slot = function_param_flow_program_point_hash_slot(function_id,
            store->program_point_index_hash_capacity);
        while (store->program_point_index_hash[slot] != 0) {
            size_t ordinal = store->program_point_index_hash[slot] - 1;
            if (ordinal >= store->program_point_index_count) break;
            if (store->program_point_indexes[ordinal].function_id == function_id)
                return ordinal;
            slot = (slot + 1) &
                (store->program_point_index_hash_capacity - 1);
        }
        if (store->program_point_index_hash[slot] == 0) return SIZE_MAX;
    }
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        if (store->program_point_indexes[i].function_id == function_id)
            return i;
    }
    return SIZE_MAX;
}

static bool
function_param_flow_program_point_index_reserve(
    FunctionParamFlowSummaryStore *store,
    size_t minimum)
{
    FunctionParamFlowProgramPointIndex *grown;
    size_t capacity;

    if (store == NULL)
        return false;
    if (minimum <= store->program_point_index_capacity)
        return true;
    capacity = store->program_point_index_capacity == 0
        ? 8 : store->program_point_index_capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(store->program_point_indexes,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    memset(grown + store->program_point_index_capacity, 0,
           (capacity - store->program_point_index_capacity) * sizeof(*grown));
    store->program_point_indexes = grown;
    store->program_point_index_capacity = capacity;
    return true;
}

static bool
function_param_flow_program_point_append(
    FunctionParamFlowProgramPointIndex *index,
    size_t param_index,
    ASTNode *root)
{
    ASTNode **grown;
    size_t capacity;

    if (index == NULL || root == NULL || param_index >= index->param_count)
        return false;
    if (index->root_counts[param_index]
            == index->root_capacities[param_index]) {
        capacity = index->root_capacities[param_index] == 0
            ? 8 : index->root_capacities[param_index];
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
        if (capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(index->roots_by_param[param_index],
                        capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        index->roots_by_param[param_index] = grown;
        index->root_capacities[param_index] = capacity;
    }
    index->roots_by_param[param_index][index->root_counts[param_index]++] = root;
    index->relevant_root_count++;
    return true;
}

bool
function_param_flow_program_point_index_build(
    FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl,
    FunctionParamFlowProgramPointIndex **index_out)
{
    FunctionParamFlowProgramPointIndex *index;
    ASTNode *body;
    uint32_t function_id;
    size_t existing;
    size_t param_count;

    if (store == NULL || function_decl == NULL || index_out == NULL)
        return false;
    function_id = ast_node_stable_id(function_decl);
    if (function_id == 0)
        return false;
    existing = function_param_flow_program_point_index_find(
        store, function_id);
    if (existing != SIZE_MAX) {
        *index_out = &store->program_point_indexes[existing];
        bool same_decl = (*index_out)->function_decl == function_decl;
        bool roots_ready = (*index_out)->dependency_roots_ready;
        bool roots_current = roots_ready &&
            function_param_flow_index_roots_current(*index_out);
        bool claims_current = roots_current &&
            function_param_flow_index_claims_current(store->ctx, *index_out);
        if ((!same_decl || !roots_ready || !roots_current || !claims_current) &&
            getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
            const char *requested = ast_declaration_name(function_decl);
            const char *indexed = ast_declaration_name((*index_out)->function_decl);
            fprintf(stderr,
                "pgy: function-param-flow-index-stale id=%u requested=%s"
                " indexed=%s same_decl=%d roots_ready=%d roots_current=%d"
                " claims_current=%d\n",
                function_id,
                requested != NULL ? requested : "<unknown>",
                indexed != NULL ? indexed : "<unknown>",
                same_decl, roots_ready, roots_current, claims_current);
        }
        return same_decl && roots_ready && roots_current && claims_current;
    }

    if (store->validating_current) return false;
    param_count = ast_func_param_count(function_decl);
    if (!function_param_flow_program_point_index_reserve(
            store, store->program_point_index_count + 1)
        || (param_count > SIZE_MAX / sizeof(ASTNode **))
        || (param_count > SIZE_MAX / sizeof(size_t))) {
        return false;
    }

    existing = store->program_point_index_count++;
    index = &store->program_point_indexes[existing];
    memset(index, 0, sizeof(*index));
    index->function_id = function_id;
    index->function_decl = function_decl;
    index->param_count = param_count;
    function_param_flow_program_point_hash_append(store, existing);
    if (param_count > 0) {
        index->roots_by_param = calloc(param_count, sizeof(ASTNode **));
        index->root_counts = calloc(param_count, sizeof(size_t));
        index->root_capacities = calloc(param_count, sizeof(size_t));
        if (index->roots_by_param == NULL
            || index->root_counts == NULL
            || index->root_capacities == NULL) {
            function_param_flow_program_point_index_destroy(index);
            store->program_point_index_count--;
            function_param_flow_program_point_hash_disable(store);
            return false;
        }
    }

    body = ast_func_body(function_decl);
    index->dependency_body = body;
    index->dependency_body_id = ast_node_stable_id(body);
    if (body == NULL) {
        index->dependency_roots_ready = true;
        *index_out = index;
        return true;
    }

    if (body->type == AST_BLOCK) {
        index->dependency_root_count = ast_block_statement_count(body);
        if (index->dependency_root_count > SIZE_MAX / sizeof(*index->dependency_roots)) return false;
        index->dependency_roots = calloc(index->dependency_root_count, sizeof(*index->dependency_roots));
        index->dependency_root_ids = calloc(index->dependency_root_count, sizeof(*index->dependency_root_ids));
        if (index->dependency_root_count != 0 && (index->dependency_roots == NULL || index->dependency_root_ids == NULL)) return false;
        for (size_t s = 0; s < ast_block_statement_count(body); s++) {
            ASTNode *statement = ast_block_statement(body, s);
            index->dependency_roots[s] = statement;
            index->dependency_root_ids[s] = ast_node_stable_id(statement);
            index->statement_count++;
            store->indexed_statement_visits++;
            for (size_t p = 0; p < param_count; p++) {
                FuncParam *param = ast_func_param(function_decl, p);
                if (param == NULL || param->name == NULL
                    || !function_param_flow_has_identifier_ref(
                        statement, param->name))
                    continue;
                if (!function_param_flow_program_point_append(
                        index, p, statement)) {
                    store->indexed_program_points -=
                        index->relevant_root_count;
                    function_param_flow_program_point_index_destroy(index);
                    store->program_point_index_count--;
                    function_param_flow_program_point_hash_disable(store);
                    return false;
                }
                store->indexed_program_points++;
            }
        }
    } else {
        index->dependency_roots = malloc(sizeof(*index->dependency_roots));
        index->dependency_root_ids = malloc(sizeof(*index->dependency_root_ids));
        if (index->dependency_roots == NULL || index->dependency_root_ids == NULL) return false;
        index->dependency_roots[0] = body;
        index->dependency_root_ids[0] = ast_node_stable_id(body);
        index->dependency_root_count = 1;
        index->statement_count = 1;
        store->indexed_statement_visits++;
        for (size_t p = 0; p < param_count; p++) {
            FuncParam *param = ast_func_param(function_decl, p);
            if (param == NULL || param->name == NULL
                || !function_param_flow_has_identifier_ref(body, param->name))
                continue;
            if (!function_param_flow_program_point_append(index, p, body)) {
                store->indexed_program_points -= index->relevant_root_count;
                function_param_flow_program_point_index_destroy(index);
                store->program_point_index_count--;
                function_param_flow_program_point_hash_disable(store);
                return false;
            }
            store->indexed_program_points++;
        }
    }

    if (!function_param_flow_index_claims_snapshot(store->ctx, index, body) ||
        !function_param_flow_locals_record(index, body)) return false;
    index->mutable_output_required = function_param_flow_mutable_output_required(store->ctx, function_decl, index, body);
    index->dependency_roots_ready = true;
    *index_out = index;
    /* Construction reads the live typed generation. Seal the resulting
     * projection once here so downstream owners can distinguish a read of
     * that sealed view from a request to replay its currency memo. */
    return function_param_flow_index_claims_current(store->ctx, index);
}

bool function_param_flow_no_store_call_append(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *call)
{
    ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
    bool uses_local = false, uses_forwarded = false;
    bool private_outputs = function_param_flow_private_output_call(
        ctx, index, call, false, true, &uses_local, &uses_forwarded);
    if (callee == NULL ||
        (!semantic_param_publication_routine_no_output_header_ready(ctx, callee) &&
         !private_outputs)) return false;
    for (size_t p = 0; p < ast_call_arg_count(call); p++)
        if (!semantic_param_publication_value(ctx, ast_call_argument(call, p)).ok) return false;
    size_t count = index->no_store_call_count;
    for (size_t e = 0; e < count; e++) if (index->no_store_calls[e] == call) return true;
    if (count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **calls = realloc(index->no_store_calls, (count + 1) * sizeof(*calls));
    if (calls == NULL) return false;
    index->no_store_calls = calls;
    calls[count] = call;
    index->no_store_call_count++;
    index->private_local_stores |= uses_local;
    index->contextual_output_required |= uses_forwarded;
    return true;
}


typedef struct {
    ASTNode *decl;
    /* This routine is reached only through a call whose every inout actual is
     * an independent local Array literal in its caller.  The permission is an
     * edge fact: it never makes the callee globally no-output. */
    bool private_outputs;
} FunctionParamFlowNoStoreWork;

/* The queue is owned by this closure, not an early parameter mask or an index
 * pointer retained across growable table allocation. Cycles close only after
 * every reached header and local store predicate has been checked. */
static bool function_param_flow_no_store_graph_replay(FunctionParamFlowSummaryStore *store,
    ASTNode *decl,
    FunctionParamFlowInputCapture capture, void *user)
{
    if (store == NULL || decl == NULL || store->failed) {
        return false;
    }
    uint32_t declaration_id = ast_node_stable_id(decl);
    /* Repeating a refusal cannot grant permission. Its only consumer is the
     * unchanged conservative backing path. Positive set reuse replays every
     * current premise; out-of-solver queries reconstruct the graph. No borrowed
     * index pointer survives closure construction. */
    if (store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id &&
                index->failed_no_store_epoch == store->demand_epoch) {
                return false;
            }
        }
    /* The admitted set is a demand-wide union.  It can answer a boolean
     * currency query, but it cannot own an exact input capture for one root:
     * replaying the union would make unrelated routines premises of that
     * root's result fact.  A capture therefore walks the root closure below
     * and records only the members and calls it actually reaches. */
    bool admitted_current = capture == NULL && store->solving &&
        store->demand_epoch != 0 && store->admitted_no_store_count != 0;
    if (admitted_current) {
        if (!function_param_flow_no_store_admitted_current(store, capture, user)) return false;
        size_t ordinal = function_param_flow_program_point_index_find(store, declaration_id);
        if (function_param_flow_no_store_member_admitted(store, ordinal) &&
            store->program_point_indexes[ordinal].function_decl == decl) return true;
    }
    FunctionParamFlowNoStoreWork *queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0].decl = decl;
    queue[0].private_outputs = false;
    size_t count = 1;
    bool cacheable = true;
    /* Solver work belongs to its current demand. A fresh synchronous currency
     * check outside that solve has its own bounded closure, not the last solve's
     * exhausted counter. Every reached member is still charged below. */
    size_t local_work = 0;
    size_t *work = store->solving ? &store->work_units : &local_work;
    bool closed = true;
    for (size_t at = 0; at < count && closed; at++) {
        ASTNode *current = queue[at].decl;
        bool private_outputs = queue[at].private_outputs;
        if (private_outputs) cacheable = false;
        if (*work >= store->work_budget) {
            closed = false;
            break;
        }
        (*work)++;
        FunctionParamFlowProgramPointIndex *index;
        bool header_current = private_outputs
            ? semantic_param_publication_routine_relation_header_ready(
                store->ctx, current)
            : semantic_param_publication_routine_no_output_header_ready(
                store->ctx, current);
        bool index_current =
            function_param_flow_program_point_index_read_sealed(
                store, current, &index) ||
            function_param_flow_program_point_index_build(
                store, current, &index);
        if (!header_current || !index_current || !index->dependency_roots_ready ||
            index->mutable_output_required ||
            (!private_outputs && index->contextual_output_required)) {
            closed = false;
            break;
        }
        if (capture != NULL &&
            !capture(store->ctx, current, index->claims, NULL, user)) {
            closed = false;
            break;
        }
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL) {
                closed = false;
                break;
            }
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_value(
                        store->ctx, ast_call_argument(call, p)).ok) {
                    closed = false;
                }
            if (!closed) break;
            if (capture != NULL &&
                !capture(store->ctx, current, NULL, call, user)) {
                closed = false;
                break;
            }
            bool regular_edge =
                semantic_param_publication_routine_no_output_header_ready(
                    store->ctx, callee);
            bool uses_local = false, uses_forwarded = false;
            bool private_edge = !regular_edge &&
                function_param_flow_private_output_call(
                    store->ctx, index, call, true, private_outputs,
                    &uses_local, &uses_forwarded);
            if (!regular_edge && !private_edge) {
                closed = false;
                break;
            }
            if (private_edge) cacheable = false;
            if (admitted_current && regular_edge) {
                size_t ordinal = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
                if (function_param_flow_no_store_member_admitted(store, ordinal) &&
                    store->program_point_indexes[ordinal].function_decl == callee) continue;
            }
            size_t found = 0;
            while (found < count && queue[found].decl != callee) found++;
            if (found != count) {
                /* A globally no-output path is stricter than the contextual
                 * private-output path and therefore wins if both reach the
                 * same routine. */
                if (regular_edge) queue[found].private_outputs = false;
                continue;
            }
            if (count == SIZE_MAX / sizeof(*queue)) { closed = false; break; }
            FunctionParamFlowNoStoreWork *grown = realloc(
                queue, (count + 1) * sizeof(*queue));
            if (grown == NULL) { closed = false; break; }
            queue = grown;
            queue[count].decl = callee;
            queue[count].private_outputs = private_edge;
            count++;
        }
    }
    /* The admitted set is keyed only by routine.  A contextual edge cannot
     * enter it because that would silently turn caller-private storage into a
     * global property of the callee. */
    if (closed && cacheable && store->solving && store->demand_epoch != 0) {
        size_t prior = store->admitted_no_store_count;
        if (count > SIZE_MAX - prior || prior + count > SIZE_MAX / sizeof(size_t)) closed = false;
        else {
            size_t *members = realloc(store->admitted_no_store_indexes, (prior + count) * sizeof(*members));
            if (members == NULL) closed = false;
            else {
                store->admitted_no_store_indexes = members;
                for (size_t m = 0; m < count; m++) {
                    size_t ordinal = function_param_flow_program_point_index_find(
                        store, ast_node_stable_id(queue[m].decl));
                    if (ordinal == SIZE_MAX ||
                        store->program_point_indexes[ordinal].function_decl !=
                            queue[m].decl) { closed = false; break; }
                    members[prior + m] = ordinal;
                }
                if (closed) {
                    store->admitted_no_store_count = prior + count;
                    for (size_t m = prior; m < prior + count; m++) {
                        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[members[m]];
                        index->admitted_no_store_epoch = store->demand_epoch;
                        index->admitted_no_store_position = m + 1;
                    }
                }
            }
        }
    }
    free(queue);
    if (!closed && store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id) {
                index->failed_no_store_epoch = store->demand_epoch;
                break;
            }
        }
    return closed;
}

bool function_param_flow_no_store_graph_capture(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, FunctionParamFlowInputCapture capture, void *user)
{
    if (store == NULL || decl == NULL || store->failed) return false;
    uint32_t declaration_id = ast_node_stable_id(decl);
    size_t ordinal = function_param_flow_program_point_index_find(
        store, declaration_id);
    bool demand_receipt = capture == NULL && store->solving &&
        store->demand_epoch != 0 && ordinal != SIZE_MAX &&
        ordinal < store->program_point_index_count &&
        store->program_point_indexes[ordinal].function_decl == decl &&
        store->program_point_indexes[ordinal].current_no_store_epoch ==
            store->demand_epoch;
    bool sealed_receipt = capture == NULL && !store->solving &&
        ordinal != SIZE_MAX && ordinal < store->program_point_index_count &&
        store->program_point_indexes[ordinal].function_decl == decl &&
        store->program_point_indexes[ordinal].no_store_current_at_seal;
    if (demand_receipt || sealed_receipt) {
        if (getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") == NULL) return true;
        if (!function_param_flow_no_store_graph_replay(
                store, decl, NULL, NULL)) {
            function_param_flow_fail(store, decl,
                "sealed no-store closure receipt diverged from full replay");
            return false;
        }
        return true;
    }

    bool closed = function_param_flow_no_store_graph_replay(
        store, decl, capture, user);
    if (!closed || capture != NULL || store->failed)
        return closed;
    ordinal = function_param_flow_program_point_index_find(
        store, declaration_id);
    if (ordinal != SIZE_MAX && ordinal < store->program_point_index_count &&
        store->program_point_indexes[ordinal].function_decl == decl) {
        if (store->solving && store->demand_epoch != 0)
            store->program_point_indexes[ordinal].current_no_store_epoch =
                store->demand_epoch;
        else if (!store->solving)
            store->program_point_indexes[ordinal].no_store_current_at_seal = true;
    }
    return true;
}

bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store,
    ASTNode *decl)
{
    return function_param_flow_no_store_graph_capture(store, decl, NULL, NULL);
}

bool function_param_flow_backing_required(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, bool *required)
{
    if (!semantic_param_publication_return_backing_required(store->ctx, decl, ordinal, required)) return false;
    if (*required) return true;
    bool anchored;
    if (!semantic_param_publication_parameter_is_anchored_handle(store->ctx, decl, ordinal, &anchored)) return false;
    if (!anchored) return true;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index)) return false;
    FunctionParamFlowIndexClaims *claims = index->claims;
    if (claims == NULL) return false;
    uint32_t formal = ast_func_param_stable_id(ast_func_param(decl, ordinal));
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[c];
        if (!row->semantic_promoted || row->shape != AST_CALL || row->physical_value ||
            (row->execution_present && !row->reachable) || !row->backing_relation || !row->backing_depends ||
            !row->result_relation || row->result_depends) continue;
        ASTNode *source = ast_call_argument(row->node, 0);
        for (size_t step = 0; source != NULL && step <= claims->count; step++) {
            if (source->type != AST_IDENTIFIER) break;
            uint32_t binding = semantic_param_publication_binding(store->ctx, source);
            if (binding == 0) return false;
            if (binding == formal) { *required = true; return true; }
            ASTNode *alias = NULL;
            for (size_t a = 0; a < claims->count; a++) {
                const FunctionParamFlowIndexClaim *candidate = &claims->rows[a];
                if (candidate->id == binding && candidate->shape == AST_LET_DECL && candidate->local_alias)
                    alias = ast_let_initializer(candidate->node);
            }
            source = alias;
            if (source != NULL && step == claims->count) return false;
        }
    }
    return true;
}

bool function_param_flow_local_replacement(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *assignment)
{
    if (!semantic_param_publication_assignment_replacement(ctx, assignment)) return false;
    ASTNode *node = ast_assignment_target(assignment);
    if (!function_param_flow_index_claim_record(ctx, index, node)) return false;
    uint32_t binding = semantic_param_publication_binding(ctx, node);
    if (binding == 0 || node->type != AST_IDENTIFIER) return false;
    for (size_t i = 0; i < index->local_count; i++)
        if (binding == index->local_ids[i] && !ast_let_is_alias(index->local_definitions[i]) &&
            binding == ast_node_stable_id(index->local_definitions[i])) return true;
    ASTNode *decl = index->function_decl;
    if (decl == NULL || decl->type != AST_FUNC_DECL || index->function_id == 0 ||
        index->function_id != ast_node_stable_id(decl) ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true)) return false;
    for (size_t p = 0; p < ast_func_param_count(decl); p++) {
        FuncParam *param = ast_func_param(decl, p);
        if (param == NULL || ast_func_param_stable_id(param) != binding) continue;
        bool anchored;
        /* An own parameter belongs to this call as a by-value one does;
         * reassigning either rebinds local storage, never the caller's. */
        if ((param->mode != PARAM_MODE_DEFAULT && param->mode != PARAM_MODE_OWN) || param->type == NULL ||
            !semantic_param_publication_parameter_is_anchored_handle(ctx, decl, p, &anchored) || anchored)
            return false;
        const Type *type = semantic_type_resolution_lookup_metadata_type_ref(ctx, param->type);
        const char *name = ast_assignment_semantic_binding_type_name(assignment);
        return type != NULL && type != TYPE_UNKNOWN && type->name != NULL && name != NULL &&
            !type_is_slot_handle(type) && !type_is_resource_handle(type) && strcmp(type->name, name) == 0;
    }
    return false;
}
static bool function_param_flow_local_record(FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (ast_let_is_alias(node)) return true;
    size_t count = index->local_count;
    if (ast_node_stable_id(node) == 0 || count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **definitions = realloc(index->local_definitions, (count + 1) * sizeof(*definitions));
    if (definitions == NULL) return false;
    index->local_definitions = definitions;
    uint32_t *ids = realloc(index->local_ids, (count + 1) * sizeof(*ids));
    if (ids == NULL) return false;
    index->local_ids = ids;
    definitions[count] = node;
    ids[count] = ast_node_stable_id(node);
    index->local_count++;
    return true;
}
/* Every local the routine declares, wherever it stands: assigning one rebinds
 * it. The mutable-output walk stops at its first answer, so it records none. */
static bool function_param_flow_locals_record(FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (node == NULL) return true;
    if (node->type == AST_LET_DECL && !function_param_flow_local_record(index, node)) return false;
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    for (size_t i = 0; supported && count != SIZE_MAX && i < count; i++)
        if (!function_param_flow_locals_record(index, function_param_flow_claim_child(node, i))) return false;
    return true;
}
bool function_param_flow_mutable_output_required(SemanticContext *ctx, ASTNode *decl, FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (node == NULL) return false;
    if (!function_param_flow_index_claim_record(ctx, index, node)) return true;
    bool reachable;
    if (semantic_param_publication_execution(ctx, node, &reachable) && !reachable) return false;
    switch (node->type) {
    case AST_LET_DECL:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_let_initializer(node));
    case AST_RETURN: return function_param_flow_mutable_output_required(ctx, decl, index, ast_return_value(node));
    case AST_CALL: {
        ASTNode *receiver = NULL;
        ASTNode *direct = semantic_param_publication_direct_callee(ctx, node);
        if ((direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
             !semantic_param_publication_bodyless_selected_call_current(ctx, node)) ||
            (direct != NULL && !semantic_param_publication_instance_receiver(ctx, node, &receiver))) {
            return true;
        }
        if (receiver != NULL && function_param_flow_mutable_output_required(ctx, decl, index, receiver)) {
            return true;
        }
        if (semantic_param_publication_checked_local_owner_constructor(ctx, node)) {
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(ctx, node, p) ||
                    function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
            return false;
        }
        if (function_param_flow_contextual_array_output_current(
                ctx, index, node)) {
            index->contextual_output_required = true;
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (function_param_flow_mutable_output_required(
                        ctx, decl, index, ast_call_argument(node, p)))
                    return true;
            return false;
        }
        if (semantic_param_publication_private_array_push(ctx, node, decl) ||
            semantic_param_publication_private_physical_array_mutation(ctx, node, decl)) {
            index->private_local_stores = true;
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(ctx, node, p) ||
                    function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
            return false;
        }
        if (!semantic_param_publication_value_call_target(ctx, node) &&
            !function_param_flow_no_store_call_append(ctx, index, node)) {
            return true;
        }
        if (ast_call_arg_count(node) == 0)
            return !function_param_flow_no_store_call_append(ctx, index, node);
        for (size_t p = 0; p < ast_call_arg_count(node); p++) {
            if (!semantic_param_publication_checked_argument(ctx, node, p)) {
                return true;
            }
            bool depends = false, retains = true;
            bool relation = semantic_param_publication_checked_result_relation(ctx, node, p, &depends, &retains);
            /* This graph proves that the callee retains no caller storage; it
             * does not prove result independence. A dependent result that the
             * callee does not retain is therefore still a no-store edge. The
             * result-loan owner separately decides whether that dependent
             * result may cross its caller boundary. */
            if (!semantic_param_publication_value_is_physical_scalar(ctx, ast_call_argument(node, p)) &&
                !semantic_indexed_string_independent_result(ast_call_argument(node, p), ctx) &&
                !(relation && !retains) && !semantic_param_publication_argument_borrowed_for_call(ctx, node, p) &&
                !semantic_typed_map_query_receiver_read(ctx, node, p) &&
                !function_param_flow_no_store_call_append(ctx, index, node)) {
                return true;
            }
            if (function_param_flow_mutable_output_required(
                    ctx, decl, index, ast_call_argument(node, p))) {
                return true;
            }
        }
        return false;
    }
    case AST_ASSIGNMENT:
        return !function_param_flow_local_replacement(ctx, index, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_assignment_value(node));
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (function_param_flow_mutable_output_required(ctx, decl, index, ast_block_statement(node, i))) return true;
        return false;
    case AST_IF_STMT:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_if_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_then_branch(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_else_branch(node));
    case AST_FOR_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_start(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_end(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_iterable(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_body(node));
    case AST_WHILE_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_while_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_while_body(node));
    case AST_WITH_STMT: return function_param_flow_mutable_output_required(ctx, decl, index, ast_with_body(node));
    case AST_IDENTIFIER: case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: case AST_BREAK: case AST_CONTINUE: return false;
    case AST_BINARY:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_left(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_right(node));
    case AST_UNARY: return function_param_flow_mutable_output_required(ctx, decl, index, ast_unary_operand(node));
    case AST_CAST:
        return !semantic_param_publication_checked_numeric_cast(ctx, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_cast_operand(node));
    case AST_MEMBER_ACCESS:
        return !semantic_param_publication_checked_unit_enum_selection(ctx, node) &&
            function_param_flow_mutable_output_required(ctx, decl, index, ast_member_object(node));
    case AST_ARRAY_ACCESS:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_array(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_index(node));
    case AST_ARRAY_LITERAL: {
        /* A current literal introduces no store edge of its own. Every element
         * evaluation still belongs to this graph; this is no backing grant. */
        SemanticPublishedValue value = semantic_param_publication_value(ctx, node);
        if (!value.ok || value.meta) return true;
        for (size_t p = 0; p < ast_array_literal_count(node); p++) {
            ASTNode *element = ast_array_literal_element(node, p);
            if (element == NULL || function_param_flow_mutable_output_required(ctx, decl, index, element)) return true;
        }
        return false;
    }
    default: return true;
    }
}
