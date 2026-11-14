#include "function_param_flow_summary_internal.h"
#include "../../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "../typed_map_query_fact_owner.h"
#include "../type_checker/type_checker_internal.h"
#include "../type_checker/type_checker_ownership_internal.h"
#include "../array_storage_release_owner.h"
#include "../collection_ownership_fact.h"
#include "../formal_string_store_landing_owner.h"
#include "../loan_copy_landing_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool function_param_flow_receiver_independent(ReturnedBacking *state, ASTNode *call)
{
    ASTNode *decl = function_param_flow_receiver_readonly_current(state->store, call, true), *receiver = NULL;
    if (decl == NULL || !semantic_param_publication_instance_receiver(state->store->ctx, call, &receiver)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, receiver);
    size_t at = backing_node(state, call);
    if (at == SIZE_MAX || !value.ok || value.meta) return false;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    uint32_t id = ast_node_stable_id(receiver), formal = ast_func_param_stable_id(ast_func_param(decl, 0));
    if (id == 0 || formal == 0 || (row->receiver != NULL
        && (row->receiver_summarized || row->receiver != receiver || row->receiver_id != id || row->receiver_type != value.type
            || row->receiver_callee_id != ast_node_stable_id(decl) || row->receiver_formal_id != formal))) return false;
    row->receiver = receiver; row->receiver_id = id; row->receiver_type = value.type;
    row->receiver_callee_id = ast_node_stable_id(decl); row->receiver_formal_id = formal;
    return true;
}

/* The method's summary for its receiver formal, once that summary is
 * complete: its entry, or SIZE_MAX. */
size_t function_param_flow_receiver_formal_summary(ReturnedBacking *state, ASTNode *decl)
{
    FunctionParamFlowSummaryStore *store = state->store;
    if (decl == NULL) return SIZE_MAX;
    if (!store->validating_current) (void)function_param_flow_summary_demand(&state->lookup, decl, 0);
    size_t index = function_param_flow_find(store, ast_node_stable_id(decl), 0);
    if (store->failed || index == SIZE_MAX) return SIZE_MAX;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[index];
    if (entry->function_decl != decl || entry->analysis_unsupported ||
        (entry->state != FUNCTION_PARAM_FLOW_COMPLETE &&
         !(store->solving && entry->state == FUNCTION_PARAM_FLOW_EVALUATED)) ||
        entry->returned_backing_unproved || entry->returned_backing_pending ||
        !entry->output_dependencies_ready || entry->output_dependencies == NULL ||
        entry->output_count != ast_func_param_count(decl)) return SIZE_MAX;
    return index;
}

/* A receiver the method may change or keep follows the method's summary for
 * its receiver formal. The edge is recorded on the call's row so the currency
 * check revalidates it. Returns that summary's entry, or SIZE_MAX while it is
 * not complete; an unsupported summary makes the frame unsupported. */
size_t backing_receiver_summary(ReturnedBacking *state, ASTNode *call, ASTNode *receiver)
{
    ASTNode *decl = semantic_param_publication_direct_callee(state->store->ctx, call);
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, receiver);
    if (decl == NULL || !value.ok || value.meta) return SIZE_MAX;
    size_t index = function_param_flow_receiver_formal_summary(state, decl);
    if (index == SIZE_MAX) { (void)function_param_flow_receiver_unsupported(state, call, decl); return SIZE_MAX; }
    size_t at = backing_node(state, call);
    if (at == SIZE_MAX) return SIZE_MAX;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    uint32_t id = ast_node_stable_id(receiver), formal = ast_func_param_stable_id(ast_func_param(decl, 0));
    if (id == 0 || formal == 0 || (row->receiver != NULL
        && (!row->receiver_summarized || row->receiver != receiver || row->receiver_id != id ||
            row->receiver_type != value.type || row->receiver_callee_id != ast_node_stable_id(decl) ||
            row->receiver_formal_id != formal))) return SIZE_MAX;
    row->receiver = receiver; row->receiver_id = id; row->receiver_type = value.type;
    row->receiver_callee_id = ast_node_stable_id(decl); row->receiver_formal_id = formal;
    row->receiver_summarized = true;
    return index;
}

/* The frame row of the binding an identifier names. A host field is a
 * field of the routine's `self`, so its storage is the self parameter's. */
static FunctionParamFlowBackingBinding *backing_iteration_binding_row(
    ReturnedBacking *state, uint32_t binding);

FunctionParamFlowBackingBinding *backing_identifier_row(ReturnedBacking *state, ASTNode *value)
{
    FunctionParamFlowBackingBinding *row = backing_binding(state,
        semantic_param_publication_binding(state->store->ctx, value), false);
    if (row == NULL)
        row = backing_iteration_binding_row(state,
            semantic_param_publication_binding(state->store->ctx, value));
    if (row != NULL || !ast_identifier_binding_is_host_field(value)) return row;
    for (size_t p = 0; p < ast_func_param_count(state->decl); p++) {
        FuncParam *param = ast_func_param(state->decl, p);
        if (param != NULL && param->type == NULL && param->name != NULL && strcmp(param->name, "self") == 0)
            return backing_binding(state, ast_func_param_stable_id(param), false);
    }
    return NULL;
}

/* An enum value whose payloads hold no storage, such as a unit variant,
 * carries no backing, whether it is a variable or a variant constant. */
bool backing_storage_free_enum(ReturnedBacking *state, SemanticPublishedValue value)
{
    return value.ok && !value.meta && value.type != NULL && value.type->kind == TYPE_KIND_ENUM &&
        semantic_classify_ownership_type(value.type, state->store->ctx) == OWNERSHIP_TYPE_COPY_ONLY;
}

/* A bare unit variant is bound to its enum's declaration (a payload variant
 * is a constructor), and holds no storage whatever the other variants do. */
bool backing_unit_variant(ReturnedBacking *state, SemanticPublishedValue value, uint32_t binding)
{
    ASTNode *decl = value.ok && !value.meta && value.type != NULL && value.type->kind == TYPE_KIND_ENUM
        ? semantic_find_enum_decl_by_name(state->store->ctx, value.type->name) : NULL;
    return decl != NULL && decl->type == AST_ENUM_DECL && binding != 0 && ast_node_stable_id(decl) == binding;
}

static bool backing_path_merge(ReturnedBacking *state,
    FunctionParamFlowBackingBinding *binding,
    const FunctionParamFlowResultPath *path)
{
    if (state == NULL || binding == NULL ||
        !function_param_flow_result_path_valid(path) || !path->present) {
        if (state != NULL && state->fact != NULL) state->fact->complete = false;
        return false;
    }
    FunctionParamFlowResultPath before = {0};
    if (binding->origin_path != NULL) before = *binding->origin_path;
    else {
        binding->origin_path = calloc(1, sizeof(*binding->origin_path));
        if (binding->origin_path == NULL) {
            function_param_flow_fail(state->store, state->decl,
                "backing binding result path allocation failed");
            state->fact->complete = false;
            return false;
        }
    }
    if (!function_param_flow_result_path_join(binding->origin_path, path)) {
        state->fact->complete = false;
        return false;
    }
    bool changed = !function_param_flow_result_path_equal(&before,
        binding->origin_path);
    state->fact->changed |= changed &&
        binding->origin_read_epoch == state->fact->epoch;
    return true;
}

/* Control projection expands a for-loop into its header/body children, so the
 * loop node itself is not an executable step. Materialize the typed element
 * binding lazily when the first body identifier reads it. The sealed loop
 * claim and published element kind own whether this is a value or a backing
 * alias; a missing claim must not become a zero-origin default. */
static FunctionParamFlowBackingBinding *backing_iteration_binding_row(
    ReturnedBacking *state, uint32_t binding)
{
    if (state == NULL || state->fact == NULL ||
        state->fact->claims == NULL || binding == 0)
        return NULL;
    ASTNode *loop = NULL;
    for (size_t c = 0; c < state->fact->claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim =
            &state->fact->claims->rows[c];
        if (claim->id != binding || claim->shape != AST_FOR_LOOP)
            continue;
        if (loop != NULL) {
            state->fact->complete = false;
            return NULL;
        }
        loop = claim->node;
    }
    if (loop == NULL || loop->type != AST_FOR_LOOP ||
        ast_node_stable_id(loop) != binding || ast_for_variable(loop) == NULL) {
        return NULL;
    }
    bool shares = false;
    if (!semantic_param_publication_iteration_binding_shares_backing(
            state->store->ctx, loop, &shares)) {
        state->fact->complete = false;
        return NULL;
    }
    FunctionParamFlowBackingBinding *row = backing_binding(state, binding,
        true);
    if (row == NULL)
        return NULL;
    row->local = true;
    ASTNode *iterable = ast_for_iterable(loop);
    if (iterable == NULL)
        return row;
    const uint64_t *bits = backing_origin(state, iterable);
    if (bits == NULL) {
        state->fact->complete = false;
        return NULL;
    }
    if (!shares || !backing_any_origin(state, bits))
        return row;
    bool grew = backing_merge(state->fact->origin_words, row->origins, bits);
    state->fact->changed |= grew &&
        row->origin_read_epoch == state->fact->epoch;
    FunctionParamFlowResultPath element_path = {.present = true};
    return backing_path_merge(state, row, &element_path) ? row : NULL;
}

static bool backing_path_concatenate(const FunctionParamFlowResultPath *outer,
    const FunctionParamFlowResultPath *inner,
    FunctionParamFlowResultPath *result)
{
    function_param_flow_result_path_clear(result);
    if (result == NULL ||
        !function_param_flow_result_path_valid(outer) ||
        !function_param_flow_result_path_valid(inner) ||
        !outer->present || !inner->present || outer->branch_count != 0 ||
        outer->count > FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT - inner->count)
        return false;
    *result = *inner;
    if (inner->count != 0)
        memmove(&result->fields[outer->count], result->fields,
            inner->count * sizeof(*result->fields));
    if (outer->count != 0)
        memcpy(result->fields, outer->fields,
            outer->count * sizeof(*result->fields));
    result->count = outer->count + inner->count;
    return true;
}

static bool backing_value_origin_path(ReturnedBacking *state, ASTNode *value,
    FunctionParamFlowResultPath *path);

static bool backing_member_origin_path(ReturnedBacking *state, ASTNode *member,
    FunctionParamFlowResultPath *path, bool *current, bool *carries)
{
    function_param_flow_result_path_clear(path);
    if (current != NULL) *current = false;
    if (carries != NULL) *carries = false;
    if (state == NULL || member == NULL || member->type != AST_MEMBER_ACCESS ||
        path == NULL || current == NULL || carries == NULL) return false;
    ASTNode *caller = NULL, *receiver = NULL, *root = NULL;
    SemanticPublishedSelectedField field = {0};
    if (!semantic_param_publication_member_projection_field_current(
            state->store->ctx, member, &caller, &receiver, &root, &field) ||
        caller != state->decl || receiver == NULL || root == NULL ||
        root->type != AST_IDENTIFIER || field.field_id == 0)
        return true;
    uint32_t binding = semantic_param_publication_binding(
        state->store->ctx, root);
    FunctionParamFlowBackingBinding *row =
        backing_identifier_row(state, root);
    FunctionParamFlowResultPath selected = {0};
    if (binding == 0 || row == NULL || row->origin_path == NULL ||
        !row->origin_path->present ||
        !function_param_flow_place_path_current(state->store->ctx, state->decl,
            member, binding, &selected) || !selected.present)
        return true;
    *current = true;
    return function_param_flow_result_path_select(row->origin_path,
        &selected, path, carries);
}

static bool backing_value_origin_path(ReturnedBacking *state, ASTNode *value,
    FunctionParamFlowResultPath *path)
{
    function_param_flow_result_path_clear(path);
    if (state == NULL || value == NULL || path == NULL) return false;
    const uint64_t *bits = backing_origin(state, value);
    if (bits == NULL) return false;
    if (!backing_any_origin(state, bits)) return true;
    if (value == state->loan_seed) {
        path->present = true;
        return true;
    }
    if (value->type == AST_IDENTIFIER) {
        FunctionParamFlowBackingBinding *row =
            backing_identifier_row(state, value);
        if (row == NULL || row->origin_path == NULL ||
            !row->origin_path->present) return false;
        *path = *row->origin_path;
        return true;
    }
    size_t at = backing_node(state, value);
    if (at == SIZE_MAX) return false;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    if (row->origin_path_epoch != state->fact->epoch ||
        row->origin_path == NULL || !row->origin_path->present) return false;
    *path = *row->origin_path;
    return true;
}

static bool backing_node_origin_path_publish(ReturnedBacking *state, size_t at,
    const FunctionParamFlowResultPath *path)
{
    if (state == NULL || state->fact == NULL || at >= state->fact->node_count ||
        !function_param_flow_result_path_valid(path) || !path->present)
        return false;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    if (row->origin_path == NULL) {
        row->origin_path = malloc(sizeof(*row->origin_path));
        if (row->origin_path == NULL) {
            function_param_flow_fail(state->store, state->decl,
                "backing node result path allocation failed");
            state->fact->complete = false;
            return false;
        }
    }
    *row->origin_path = *path;
    row->origin_path_epoch = state->fact->epoch;
    return true;
}

static bool backing_store_restores_loan_owner(ReturnedBacking *state,
    ASTNode *target, const uint64_t *origins, ASTNode *carrier)
{
    if (state == NULL || state->loan_seed == NULL ||
        !state->loan_owner_path_current || state->loan_restore_owner_id == 0 ||
        target == NULL || origins == NULL || carrier == NULL ||
        !backing_any_origin(state, origins) || state->consumer == NULL ||
        state->consumer->type != AST_ASSIGNMENT ||
        ast_assignment_target(state->consumer) != target ||
        ast_assignment_value(state->consumer) != carrier)
        return false;
    FunctionParamFlowResultPath target_path = {0};
    if (!function_param_flow_place_path_current(state->store->ctx,
            state->decl, target, state->loan_restore_owner_id, &target_path) ||
        !target_path.present ||
        !function_param_flow_result_path_equal(
            &target_path, &state->loan_owner_path))
        return false;
    return true;
}

void backing_store(ReturnedBacking *state, ASTNode *target, const uint64_t *origins, bool replacement,
    ASTNode *carrier)
{
    /* A stored value carrying none of this frame's origins moves no backing. */
    if (!backing_any_origin(state, origins)) return;
    uint32_t place = function_param_flow_backing_place(state, target);
    FunctionParamFlowBackingBinding *row = backing_binding(state, place, false);
    if (row == NULL) { backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier); state->fact->complete = false; return; }
    FunctionParamFlowResultPath target_path = {.present = true};
    FunctionParamFlowResultPath carrier_path = {.present = true};
    FunctionParamFlowResultPath stored_path = {0};
    FunctionParamFlowResultPath selected = {0};
    if (function_param_flow_place_path_current(state->store->ctx, state->decl,
            target, place, &selected) && selected.present)
        target_path = selected;
    if (carrier != NULL &&
        !backing_value_origin_path(state, carrier, &carrier_path)) {
        state->fact->complete = false;
        return;
    }
    if (!backing_path_concatenate(&target_path, &carrier_path,
            &stored_path)) {
        state->fact->complete = false;
        return;
    }
    if (!replacement) row->destination_read_epoch = state->fact->epoch;
    bool output = false;
    for (size_t w = 0; !replacement && w < state->fact->destination_words; w++) output |= row->destinations[w] != 0;
    for (size_t p = 0; !replacement && p < state->fact->origin_count; p++) if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0)
        backing_merge(state->fact->destination_words, &state->fact->outputs[p * state->fact->destination_words], row->destinations);
    bool restores_loan_owner = backing_store_restores_loan_owner(state,
        target, origins, carrier);
    if (output && !restores_loan_owner &&
        !backing_origins_held(state, origins, row->origins)) {
        backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier);
    }
    /* A by-value or own parameter is this call's storage, as a local is: a
     * write through it (an inout argument) keeps the backing in the frame,
     * and a later return or escape of the parameter carries it. A declared
     * result frame keeps its parameters free of origins. */
    bool owned_parameter = false;
    for (size_t p = 0; !row->local && !state->checking_declared_result &&
         p < ast_func_param_count(state->decl); p++) {
        FuncParam *param = ast_func_param(state->decl, p);
        if (param != NULL && ast_func_param_stable_id(param) == row->binding &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_OWN)) owned_parameter = true;
    }
    if (row->local || replacement || owned_parameter) {
        bool grew = backing_merge(state->fact->origin_words, row->origins, origins);
        state->fact->changed |= grew && row->origin_read_epoch == state->fact->epoch;
        if (!backing_path_merge(state, row, &stored_path)) return;
    }
    else if (!output) { backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier); state->fact->complete = false; }
}
static const uint64_t *backing_origin_impl(ReturnedBacking *state,
    ASTNode *node)
{
    if (node == NULL || state->fact->analysis_unsupported) return NULL;
    /* A copy assumed made reads its source as its Clone would, and holds
     * storage of its own. */
    for (size_t c = 0; state->copied != NULL && node != state->copy_source && c < state->copied->count; c++) {
        if (state->copied->at[c] != node) continue;
        static const uint64_t fresh = 0;
        const ASTNode *outer = state->copy_source;
        state->copy_source = node;
        const uint64_t *read = backing_origin(state, node);
        state->copy_source = outer;
        return read == NULL ? NULL : &fresh;
    }
    if (state->checking_declared_result) {
        uint64_t destinations = 0;
        backing_destination_value_at(state, node, &destinations, node->type == AST_MEMBER_ACCESS);
        if (!state->fact->complete || destinations != 0) { state->fact->complete = false; return NULL; }
    }
    if (node != state->loan_seed &&
        (node->type == AST_STRING || node->type == AST_NUMBER || node->type == AST_BOOLEAN)) {
        /* Pure leaves have no input backing; shared claims own their currency. */
        static const uint64_t zero = 0;
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, state->fact->claims, node)) {
            state->fact->complete = false; return NULL;
        }
        return &zero;
    }
    if (node != state->loan_seed &&
        semantic_param_publication_value_is_physical_scalar(
            state->store->ctx, node)) {
        /* A primitive result can read or compute from owned storage, but the
         * resulting value does not retain that storage. The consumer walk
         * still visits its operands and applies every call effect. */
        static const uint64_t zero = 0;
        if (ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx,
                state->fact->claims, node)) {
            state->fact->complete = false;
            return NULL;
        }
        return &zero;
    }
    if (node != state->loan_seed && node->type == AST_IDENTIFIER) {
        /* A leaf needs the current claim and binding, but not a second
         * per-frame expression row. Return the read's one-bit snapshot:
         * later binding growth must not mutate an earlier expression value. */
        static const uint64_t zero = 0, one = 1;
        FunctionParamFlowBackingFact *fact = state->fact;
        SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
        uint32_t binding = semantic_param_publication_binding(state->store->ctx, node);
        if (fact->origin_count != 1 || fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, fact->claims, node)) {
            fact->complete = false; return NULL;
        }
        if (semantic_param_publication_slot_token_operand(state->store->ctx, node) ||
            (value.ok && (value.meta || value.origin_ready))) return &zero;
        if (!value.ok || binding == 0) { fact->complete = false; return NULL; }
        FunctionParamFlowBackingBinding *source = backing_identifier_row(state, node);
        if (source == NULL && (backing_storage_free_enum(state, value) ||
                backing_unit_variant(state, value, binding))) return &zero;
        if (source == NULL) { fact->complete = false; return NULL; }
        source->origin_read_epoch = fact->epoch;
        uint64_t bits = source->origins[0];
        if ((bits & ~UINT64_C(1)) != 0) { fact->complete = false; return NULL; }
        return bits == 0 ? &zero : &one;
    }
    size_t at = backing_node(state, node);
    if (at == SIZE_MAX || !backing_work(state, at)) return NULL;
    FunctionParamFlowBackingFact *fact = state->fact;
    uint64_t *origins = fact->nodes[at].origins;
    if (fact->nodes[at].origin_epoch == fact->epoch) return origins;
    fact->nodes[at].origin_epoch = fact->epoch;
    FunctionParamFlowResultPath carried_path = {0};
    if (fact->nodes[at].origin_path != NULL)
        function_param_flow_result_path_clear(fact->nodes[at].origin_path);
    fact->nodes[at].origin_path_epoch = fact->epoch;
    if (node == state->loan_seed) {
        state->loan_seed_seen = true;
        backing_merge(state->fact->origin_words, origins, &(uint64_t){1});
        carried_path.present = true;
        if (!backing_node_origin_path_publish(state, at, &carried_path))
            fact->complete = false;
        return origins;
    }
    if (node->type == AST_CAST) {
        if (!semantic_param_publication_checked_numeric_cast(state->store->ctx, node)) { fact->complete = false; return NULL; }
        (void)backing_origin(state, ast_cast_operand(node));
        return origins;
    }
    const uint64_t *receiver_return = NULL;
    if (node->type == AST_CALL) {
        ASTNode *receiver = NULL;
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        /* An intent's value is its Bool verdict; the consumer walk applies
         * its effect on the arguments. */
        if (direct == NULL && semantic_param_publication_intent_callee(state->store->ctx, node) != NULL) {
            if (semantic_param_publication_value_is_physical_scalar(state->store->ctx, node)) return origins;
            fact->complete = false; return NULL;
        }
        if ((direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
             !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, node)) ||
            (direct != NULL && !semantic_param_publication_instance_receiver(state->store->ctx, node, &receiver))) {
            fact->complete = false; return NULL;
        }
        if (receiver != NULL) {
            const uint64_t *bits = backing_origin(state, receiver);
            if (bits == NULL) { fact->complete = false; return NULL; }
            if (backing_any_origin(state, bits) && !function_param_flow_receiver_independent(state, node)) {
                size_t entry = backing_receiver_summary(state, node, receiver);
                if (entry == SIZE_MAX) { fact->complete = false; return NULL; }
                unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
                if (!function_param_flow_entry_outward_effects_current(
                        state->store, entry, &outward_effects)) {
                    fact->complete = false;
                    return NULL;
                }
                if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                    receiver_return = bits;
                    /* The older projection walk did not expose receiver
                     * placement.  Keep that edge whole until its typed
                     * receiver ABI publishes an exact result path. */
                    carried_path.present = true;
                    carried_path.count = 0;
                }
            }
        }
    }
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
    if (value.ok && (value.meta || value.origin_ready)) return origins;
    if (semantic_param_publication_checked_local_owner_constructor(state->store->ctx, node)) return origins;
    if (node->type == AST_CALL && semantic_typed_map_query_result_independent(state->store->ctx, node)) return origins;
    switch (node->type) {
    case AST_MEMBER_ACCESS:
        if (!semantic_param_publication_checked_unit_enum_selection(state->store->ctx, node)) {
            const uint64_t *bits = backing_origin(state, ast_member_object(node));
            bool current = false, carries = false;
            FunctionParamFlowResultPath selected = {0};
            if (bits != NULL && backing_any_origin(state, bits) &&
                !backing_member_origin_path(state, node, &selected, &current,
                    &carries)) {
                fact->complete = false;
                break;
            }
            if (bits != NULL && (!current || carries))
                backing_merge(state->fact->origin_words, origins, bits);
            if (bits != NULL && backing_any_origin(state, bits) &&
                (!current || carries)) {
                FunctionParamFlowResultPath member_path = selected;
                if (!current) {
                    function_param_flow_result_path_clear(&member_path);
                    member_path.present = true;
                }
                if (!function_param_flow_result_path_join(&carried_path,
                        &member_path)) fact->complete = false;
            }
        }
        break;
    case AST_ARRAY_ACCESS: {
        const uint64_t *bits = backing_origin(state,
            ast_array_access_array(node));
        if (bits != NULL) backing_merge(state->fact->origin_words, origins, bits);
        if (bits != NULL && backing_any_origin(state, bits))
            carried_path.present = true;
        break;
    }
    case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: break;
    case AST_CALL:
        if (function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
        if (receiver_return != NULL) backing_merge(state->fact->origin_words, origins, receiver_return);
        ASTNode *viewed = semantic_param_publication_slice_view_receiver(state->store->ctx, node);
        if (viewed != NULL) {
            /* A Slice view is the receiver's storage. */
            const uint64_t *bits = backing_origin(state, viewed);
            if (bits == NULL) { fact->complete = false; break; }
            backing_merge(state->fact->origin_words, origins, bits);
            if (backing_any_origin(state, bits)) carried_path.present = true;
            break;
        }
        PublishedNode *published = publication_result_current(
            state->store->ctx, node);
        const SemanticNominalResultSnapshot *nominal = published != NULL
            ? published->nominal_result : NULL;
        bool nominal_current = nominal != NULL &&
            semantic_nominal_result_snapshot_current(state->store->ctx,
                node, nominal) &&
            nominal->field_count == ast_call_arg_count(node);
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            const uint64_t *bits = backing_origin(state, ast_call_argument(node, i));
            if (bits == NULL || !backing_any_origin(state, bits)) continue;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            if (callee == NULL) {
                if (function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
                bool depends = true, retains = true;
                bool relation = semantic_param_publication_checked_backing_relation(state->store->ctx, node, i, &depends, &retains);
                if (!relation || depends) {
                    backing_merge(state->fact->origin_words, origins, bits);
                    FunctionParamFlowResultPath result_argument_path = {0};
                    if (nominal_current &&
                        backing_value_origin_path(state,
                            ast_call_argument(node, i),
                            &result_argument_path) &&
                        result_argument_path.present) {
                        FunctionParamFlowResultPath selected = {0};
                        if (!function_param_flow_result_path_prepend(
                                nominal->fields[i].field_id,
                                &result_argument_path, &selected) ||
                            !function_param_flow_result_path_join(
                                &carried_path, &selected))
                            fact->complete = false;
                    } else {
                        carried_path.present = true;
                        carried_path.count = 0;
                    }
                }
                /* A current nominal snapshot is the exact retention
                 * relation: argument i becomes field i of the result.  It is
                 * not an unmodelled call merely because no generic backing
                 * relation row exists for a constructor. */
                bool unmodelled_retention = (!relation || retains) &&
                    !nominal_current &&
                    !semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) &&
                    !semantic_typed_map_query_receiver_read(state->store->ctx, node, i);
                if (unmodelled_retention) fact->complete = false;
            } else {
                size_t formal = 0;
                if (!semantic_param_publication_formal_for_actual(
                        state->store->ctx, node, i, &formal)) {
                    fact->complete = false;
                    break;
                }
                unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
                bool owned_sink =
                    semantic_param_publication_owned_sink_formal_current(
                        state->store->ctx, callee, formal);
                bool outward_ready = owned_sink ||
                    function_param_flow_callee_outward_effects_current(
                        &state->lookup, node, i, &outward_effects);
                bool relation_ready = owned_sink ||
                    function_param_flow_callee_relation_ready(
                        &state->lookup, node, i);
                if (owned_sink)
                    outward_effects = SLOT_PARAM_SUMMARY_CALL_ESCAPE;
                if (!relation_ready)
                    function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                else if (!outward_ready) {
                    fact->complete = false;
                    break;
                }
                /* A recursive edge can answer with its previous-pass mask while
                 * its backing relation is still pending.  Record that edge, but
                 * do not make the provisional mask a monotone frame fact.  The
                 * next closure pass consumes the effect once the relation is
                 * complete. */
                if (relation_ready && outward_ready &&
                    (outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                    backing_merge(state->fact->origin_words, origins, bits);
                    FunctionParamFlowResultPath selected = {0};
                    if (!function_param_flow_call_result_path_current(
                            state->store, node, i, &selected) ||
                        !selected.present) {
                        carried_path.present = true;
                        carried_path.count = 0;
                    }
                    if (selected.present &&
                        !function_param_flow_result_path_join(&carried_path,
                            &selected)) {
                        fact->complete = false;
                    }
                }
            }
        }
        break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < (node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node) : ast_tuple_literal_count(node)); i++) {
            const uint64_t *bits = backing_origin(state, node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i) : ast_tuple_literal_element(node, i));
            if (bits != NULL) backing_merge(state->fact->origin_words, origins, bits);
            if (bits != NULL && backing_any_origin(state, bits))
                carried_path.present = true;
        }
        break;
    default: fact->complete = false; break;
    }
    if (backing_any_origin(state, origins)) {
        if (!carried_path.present) {
            /* Missing placement never grants sibling distinction. */
            carried_path.present = true;
            carried_path.count = 0;
        }
        if (!backing_node_origin_path_publish(state, at, &carried_path))
            fact->complete = false;
    }
    return origins;
}

const uint64_t *backing_origin(ReturnedBacking *state, ASTNode *node)
{
    return backing_origin_impl(state, node);
}

/* A returned ref loan is normally allowed to remain a loan.  An explicit
 * Clone is different: if its checked result still depends on the argument,
 * treating the result as owned would break the Clone contract.  Trace only
 * the sealed local/value relation that carries this frame's origin; do not
 * reopen the program root or infer a second call graph. */
static bool backing_value_contains_dependent_clone(ReturnedBacking *state,
    ASTNode *value, size_t depth)
{
    FunctionParamFlowIndexClaims *claims = state != NULL && state->fact != NULL
        ? state->fact->claims : NULL;
    if (state == NULL || value == NULL || claims == NULL ||
        depth > claims->count + 1)
        return false;
    switch (value->type) {
    case AST_IDENTIFIER: {
        uint32_t binding = semantic_param_publication_binding(
            state->store->ctx, value);
        ASTNode *definition = NULL;
        for (size_t i = 0; i < claims->count; i++) {
            ASTNode *candidate = claims->rows[i].node;
            if (candidate == NULL || candidate->type != AST_LET_DECL ||
                binding == 0 || ast_node_stable_id(candidate) != binding)
                continue;
            if (definition != NULL && definition != candidate)
                return false;
            definition = candidate;
        }
        return definition != NULL &&
            backing_value_contains_dependent_clone(state,
                ast_let_initializer(definition), depth + 1);
    }
    case AST_CALL: {
        uint32_t builtin = 0;
        bool clone = ast_call_semantic_callee_builtin_kind(value, &builtin) &&
            builtin == BUILTIN_CLONE;
        for (size_t p = 0; p < ast_call_arg_count(value); p++) {
            bool depends = false, retains = true;
            if (!semantic_param_publication_checked_backing_relation(
                    state->store->ctx, value, p, &depends, &retains) ||
                !depends)
                continue;
            if (clone && p == 0 && !retains)
                return true;
            if (backing_value_contains_dependent_clone(state,
                    ast_call_argument(value, p), depth + 1))
                return true;
        }
        return false;
    }
    case AST_MEMBER_ACCESS:
        return backing_value_contains_dependent_clone(state,
            ast_member_object(value), depth + 1);
    case AST_ARRAY_ACCESS:
        return backing_value_contains_dependent_clone(state,
            ast_array_access_array(value), depth + 1);
    case AST_CAST:
        return backing_value_contains_dependent_clone(state,
            ast_cast_operand(value), depth + 1);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(value); i++)
            if (backing_value_contains_dependent_clone(state,
                    ast_array_literal_element(value, i), depth + 1))
                return true;
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(value); i++)
            if (backing_value_contains_dependent_clone(state,
                    ast_tuple_literal_element(value, i), depth + 1))
                return true;
        return false;
    default:
        return false;
    }
}
/* A match binding names a part of the matched value, so it is a local that
 * carries the value's storage. A unit variant or literal pattern binds
 * nothing; any other pattern shape is not followed. */
static void backing_match_bindings(ReturnedBacking *state, ASTNode *pattern, ASTNode *subject, const uint64_t *bits)
{
    size_t count = 0;
    if (pattern == NULL) return;
    if (pattern->type == AST_CALL) count = ast_call_arg_count(pattern);
    else if (pattern->type == AST_IDENTIFIER) count = 1;
    else if (pattern->type != AST_MEMBER_ACCESS && pattern->type != AST_NUMBER &&
             pattern->type != AST_STRING && pattern->type != AST_BOOLEAN) { state->fact->complete = false; return; }
    for (size_t a = 0; a < count; a++) {
        ASTNode *binding = pattern->type == AST_CALL ? ast_call_argument(pattern, a) : pattern;
        uint32_t id = binding != NULL && binding->type == AST_IDENTIFIER ? ast_node_stable_id(binding) : 0;
        if (pattern->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(binding) != id) return;
        if (id == 0 || ast_identifier_binding_syntax_id(binding) != id) { state->fact->complete = false; return; }
        FunctionParamFlowBackingBinding *row = backing_binding(state, id, true);
        if (row == NULL) { state->fact->complete = false; return; }
        row->local = true;
        backing_destination_alias(state, subject, id);
        row = backing_binding(state, id, false);
        if (row != NULL && bits != NULL) {
            bool grew = backing_merge(state->fact->origin_words, row->origins, bits);
            state->fact->changed |= grew && row->origin_read_epoch == state->fact->epoch;
            if (backing_any_origin(state, bits)) {
                FunctionParamFlowResultPath path = {.present = true};
                if (!backing_path_merge(state, row, &path)) return;
            }
        }
    }
}
static void backing_consumers_impl(ReturnedBacking *state, ASTNode *node)
{
    if (node == NULL || state->fact->analysis_unsupported) return;
    bool selector = node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS;
    bool selector_reachable = true;
    bool explicit_unreachable = state->loan_seed == NULL && selector &&
        semantic_param_publication_execution(state->store->ctx, node, &selector_reachable) && !selector_reachable;
    if (state->loan_seed == NULL && (node->type == AST_BINARY || node->type == AST_UNARY ||
        selector) && !explicit_unreachable) {
        bool empty = false;
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            !function_param_flow_consumer_empty_current(state->store->ctx,
                state->fact->claims, node, &empty)) { state->fact->complete = false; return; }
        if (empty) return;
    }
    if (node != state->loan_seed &&
        (node->type == AST_STRING || node->type == AST_NUMBER || node->type == AST_BOOLEAN ||
         node->type == AST_IDENTIFIER)) {
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, state->fact->claims, node))
            state->fact->complete = false;
        else if (node->type == AST_IDENTIFIER) {
            SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
            if (!value.ok || (!value.meta && semantic_param_publication_binding(state->store->ctx, node) == 0 &&
                !semantic_param_publication_slot_token_operand(state->store->ctx, node)))
                state->fact->complete = false;
        }
        return;
    }
    size_t at = backing_node(state, node);
    if (at == SIZE_MAX || !backing_work(state, at)) return;
    FunctionParamFlowBackingFact *fact = state->fact;
    if (fact->nodes[at].visit_epoch == fact->epoch) return;
    fact->nodes[at].visit_epoch = fact->epoch;
    if (node == state->loan_seed) state->loan_seed_seen = true;
    bool reachable;
    if (semantic_param_publication_execution(state->store->ctx, node, &reachable) && !reachable) return;
    ASTNode *previous_consumer = state->consumer;
    state->consumer = node;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) backing_consumers(state, ast_block_statement(node, i));
        break;
    case AST_DEFER_STMT: backing_consumers(state, ast_defer_body(node)); break;
    case AST_FOR_LOOP: {
        backing_consumers(state, ast_for_range_start(node));
        backing_consumers(state, ast_for_range_end(node));
        uint32_t binding = ast_node_stable_id(node);
        if (binding == 0 || ast_for_variable(node) == NULL ||
            backing_iteration_binding_row(state, binding) == NULL) {
            fact->complete = false;
            break;
        }
        ASTNode *iterable = ast_for_iterable(node);
        if (iterable != NULL) {
            backing_consumers(state, iterable);
        }
        backing_consumers(state, ast_for_body(node));
        break;
    }
    case AST_WHILE_LOOP: backing_consumers(state, ast_while_condition(node)); backing_consumers(state, ast_while_body(node)); break;
    case AST_WITH_STMT: backing_consumers(state, ast_with_body(node)); break;
    case AST_BREAK: case AST_CONTINUE: break;
    case AST_LET_DESTRUCTURE: case AST_LET_DECL: {
        SemanticPublishedSlotBirth birth;
        if (semantic_param_publication_owned_slot_binding(state->store->ctx, node, &birth)) {
            FunctionParamFlowBackingBinding *slot = backing_binding(state, birth.slot_binding_id, true);
            if (slot != NULL) slot->local = true;
            if (birth.token_type != NULL && birth.token_binding_id != 0) {
                FunctionParamFlowBackingBinding *token = backing_binding(state, birth.token_binding_id, true);
                if (token != NULL) token->local = true;
            }
            ASTNode *initializer = node->type == AST_LET_DECL ? ast_let_initializer(node) : ast_let_destructure_initializer(node);
            backing_consumers(state, initializer);
            break;
        }
        if (node->type != AST_LET_DECL) { fact->complete = false; break; }
        FunctionParamFlowBackingBinding *local = backing_binding(state, ast_node_stable_id(node), true);
        if (local != NULL) local->local = true;
        if (!semantic_param_publication_channel_binding(state->store->ctx, node) &&
            !semantic_param_publication_empty_map_binding(state->store->ctx, node))
            backing_destination_alias(state, ast_let_initializer(node), ast_node_stable_id(node));
        const uint64_t *bits = backing_origin(state, ast_let_initializer(node));
        local = backing_binding(state, ast_node_stable_id(node), false);
        if (local != NULL && bits != NULL) {
            bool grew = backing_merge(fact->origin_words, local->origins, bits);
            fact->changed |= grew && local->origin_read_epoch == fact->epoch;
            if (backing_any_origin(state, bits)) {
                FunctionParamFlowResultPath path = {0};
                if (!backing_value_origin_path(state, ast_let_initializer(node),
                        &path) || !path.present ||
                    !backing_path_merge(state, local, &path))
                    fact->complete = false;
            }
        }
        backing_consumers(state, ast_let_initializer(node)); break;
    }
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        if (target != NULL && target->type == AST_IDENTIFIER) backing_destination_alias(state, ast_assignment_value(node), semantic_param_publication_binding(state->store->ctx, target));
        const uint64_t *bits = backing_origin(state, ast_assignment_value(node));
        FunctionParamFlowProgramPointIndex *index;
        bool indexed = function_param_flow_program_point_index_read_sealed(
            state->store, state->decl, &index);
        bool replacement = indexed && function_param_flow_local_replacement(state->store->ctx, index, node);
        bool carried = bits != NULL && state->loan_seed == NULL && (bits[0] & UINT64_C(1)) != 0;
        bool landing_ready = !carried ||
            semantic_formal_store_landing_select_assigned(
                state->store->ctx, state->decl, state->fact->input_id,
                target, ast_assignment_value(node));
        if (!indexed || !landing_ready) fact->complete = false;
        if (bits != NULL) backing_store(state, target, bits, replacement, ast_assignment_value(node));
        backing_consumers(state, ast_assignment_value(node));
        break;
    }
    case AST_CALL:
        /* A sparse return-loan walk starts at the produced call value. The
         * seed call's arguments are evaluated before that value exists, so
         * replaying them as downstream consumers feeds the result relation
         * back into its own source and invents an escape. Source ownership is
         * proved separately before this walk; only later calls consume the
         * loan. */
        if (node == state->loan_seed) break;
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        if (direct == NULL && function_param_flow_intent_call(state, node)) break;
        if (direct == NULL && semantic_param_publication_slice_view_receiver(state->store->ctx, node) != NULL) {
            backing_consumers(state, semantic_param_publication_slice_view_receiver(state->store->ctx, node));
            for (size_t p = 0; p < ast_call_arg_count(node); p++) backing_consumers(state, ast_call_argument(node, p));
            break;
        }
        if (direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
            !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, node)) {
            fact->complete = false; break;
        }
        /* Invoking a function value does not store or retain that value.
         * When every argument and the result are copy-only, no backing this
         * frame tracks reaches the callee or comes back from it. Any other
         * function-value call leaves the frame incomplete below. */
        if (ast_call_semantic_callee_value_binding_id(node) != 0 &&
            semantic_param_publication_value_is_physical_scalar(
                state->store->ctx, node)) {
            bool copy_only = true;
            for (size_t p = 0; p < ast_call_arg_count(node) && copy_only; p++) {
                const uint64_t *bits =
                    semantic_param_publication_checked_argument(
                        state->store->ctx, node, p)
                    ? backing_origin(state, ast_call_argument(node, p))
                    : NULL;
                copy_only = bits != NULL && !backing_any_origin(state, bits);
            }
            if (copy_only && fact->complete) {
                for (size_t p = 0; p < ast_call_arg_count(node); p++)
                    backing_consumers(state, ast_call_argument(node, p));
                break;
            }
        }
        if (direct == NULL && function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
        if (ast_call_semantic_callee_value_binding_id(node) != 0 || ast_call_callee(node) == NULL ||
            (ast_call_callee(node)->type != AST_IDENTIFIER &&
             direct == NULL)) {
            fact->complete = false; break;
        }
        if (direct != NULL) {
            ASTNode *receiver = NULL;
            if (!semantic_param_publication_instance_receiver(state->store->ctx, node, &receiver)) {
                fact->complete = false; break;
            }
            if (receiver != NULL) {
                backing_consumers(state, receiver);
                const uint64_t *bits = backing_origin(state, receiver);
                if (bits == NULL) fact->complete = false;
                else if (backing_any_origin(state, bits) && !function_param_flow_receiver_independent(state, node)) {
                    /* The receiver's storage escapes or moves into another
                     * argument as the receiver formal's summary says. */
                    size_t entry = backing_receiver_summary(state, node, receiver);
                    FuncParam *self = ast_func_param(direct, 0);
                    if (entry == SIZE_MAX || self == NULL) { fact->complete = false; break; }
                    unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
                    if (!function_param_flow_entry_outward_effects_current(
                            state->store, entry, &outward_effects)) {
                        fact->complete = false;
                        break;
                    }
                    backing_effect(state, bits, outward_effects &
                        (SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                         SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
                    if (self->mode == PARAM_MODE_OWN)
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                    if (self->mode == PARAM_MODE_MUT_REF &&
                        !backing_inout_argument_leaves_input(state))
                        backing_movement_effect(state, bits,
                            SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                    for (size_t p = 0; p < ast_call_arg_count(node) && fact->complete; p++) {
                        size_t formal = 0;
                        if (!semantic_param_publication_formal_for_actual(state->store->ctx, node, p, &formal) ||
                            formal >= state->store->entries[entry].output_count) { fact->complete = false; break; }
                        FuncParam *output = ast_func_param(direct, formal);
                        if (output == NULL) { fact->complete = false; break; }
                        if ((output->mode == PARAM_MODE_REF || output->mode == PARAM_MODE_MUT_REF) &&
                            state->store->entries[entry].output_dependencies[formal])
                            backing_store(state, ast_call_argument(node, p), bits, false, NULL);
                    }
                }
                if (!fact->complete) break;
            }
        }
        bool physical_store = state->private_stores_closed &&
            semantic_param_publication_private_physical_array_mutation(
            state->store->ctx, node, state->decl);
        if (state->private_stores_closed && (physical_store ||
            semantic_param_publication_private_array_push(state->store->ctx, node, state->decl))) {
            /* Physical mutation transfers no backing.  A selected whole-value
             * landing makes a String store independent at this exact edge, so
             * the returned local must not keep the source formal's backing. */
            const uint64_t *bits = physical_store ? NULL : backing_origin(state, ast_call_argument(node, 1));
            bool carries = bits != NULL && (bits[0] & UINT64_C(1)) != 0;
            bool landing_ready = !carries ||
                semantic_formal_store_landing_select_held(
                    state->store->ctx, node, 1);
            bool landing_copies = carries && (
                semantic_formal_store_landing_selects_value(
                    state->store->ctx, node, ast_call_argument(node, 1)) ||
                semantic_collection_owned_string_push_effect_current(
                    node, ast_call_argument(node, 0), state->store->ctx));
            if (!landing_ready) {
                fact->complete = false;
                function_param_flow_fail(state->store, node,
                    "private String store landing receipt could not be recorded");
            }
            ASTNode *landing_root = ast_call_argument(node, 1);
            while (landing_root != NULL && (landing_root->type == AST_MEMBER_ACCESS ||
                    landing_root->type == AST_ARRAY_ACCESS))
                landing_root = landing_root->type == AST_MEMBER_ACCESS
                    ? ast_member_object(landing_root)
                    : ast_array_access_array(landing_root);
            if (state->loan_seed != NULL && landing_copies &&
                landing_root != NULL && landing_root->type == AST_IDENTIFIER &&
                state->escape_site == NULL) {
                state->escape_site = node;
                state->escape_carrier = ast_call_argument(node, 1);
            }
            /* The ordinary summary publishes the post-copy backing.  A
             * sparse loan walk still needs the pre-copy relation so it can
             * locate the exact store and prove where that loan ends. */
            if (bits != NULL && (state->loan_seed != NULL || !landing_copies))
                backing_store(state, ast_call_argument(node, 0), bits, false, NULL);
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                backing_consumers(state, ast_call_argument(node, p));
            break;
        }
        size_t stored = 0;
        bool array_store = semantic_param_publication_checked_array_store(state->store->ctx, node, &stored);
        ASTNode *held_root = array_store ? ast_call_argument(node, 0) : NULL;
        while (held_root != NULL && (held_root->type == AST_MEMBER_ACCESS || held_root->type == AST_ARRAY_ACCESS))
            held_root = held_root->type == AST_MEMBER_ACCESS ? ast_member_object(held_root) : ast_array_access_array(held_root);
        uint32_t held_id = held_root == NULL ? 0 : semantic_param_publication_binding(state->store->ctx, held_root);
        FunctionParamFlowBackingBinding *holder = held_id == 0 ? NULL : backing_binding(state, held_id, false);
        bool held = holder != NULL && holder->local;
        bool mutable_output_holder = false;
        for (size_t p = 0; holder != NULL && !held && p < ast_func_param_count(state->decl); p++) {
            FuncParam *param = ast_func_param(state->decl, p);
            bool selected = param != NULL &&
                ast_func_param_stable_id(param) == holder->binding;
            held = selected && param->mode != PARAM_MODE_REF;
            mutable_output_holder = selected &&
                param->mode == PARAM_MODE_MUT_REF;
        }
        if (held) {
            /* The pushed or set value lives in the Array afterwards, so its
             * backing goes where that Array goes. The Array is a local or a
             * parameter of this call; an inout one is an output, which the
             * store reports as a call escape. */
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(state->store->ctx, node, p)) fact->complete = false;
            const uint64_t *bits = fact->complete ? backing_origin(state, ast_call_argument(node, stored)) : NULL;
            if (bits == NULL) { fact->complete = false; break; }
            /* A local or output Array owns what it stores. When the value
             * still carries this formal's backing, the exact store edge is
             * the copy boundary whether or not the Array later leaves the
             * routine (2026-10-20_formal_storage_kept_by_output_is_copied). */
            bool carries = (bits[0] & UINT64_C(1)) != 0;
            /* A value-copy plan closes only a receiver whose continued
             * lifetime this frame owns: a proved private store graph or an
             * explicit inout output. A local already exposed to an opaque
             * call remains an escape even if its later store is copyable. */
            ASTNode *stored_argument = ast_call_argument(node, stored);
            SemanticPublishedValue stored_published =
                semantic_param_publication_value(state->store->ctx,
                    stored_argument);
            bool nominal_value = stored_published.ok &&
                !stored_published.meta && stored_published.type != NULL &&
                stored_published.type->kind == TYPE_KIND_CLASS &&
                stored_published.type->nominal_flavor == TYPE_NOMINAL_STRUCT;
            bool fixed_slot_store = array_store && stored == 2;
            bool private_local_holder =
                (nominal_value || fixed_slot_store) && holder != NULL &&
                holder->local &&
                semantic_param_publication_private_array_store_receiver(
                    state->store->ctx, node, state->decl);
            bool landing_scope_closed = state->private_stores_closed ||
                private_local_holder || mutable_output_holder;
            bool materialized_copy = semantic_loan_copy_landing_satisfied(
                state->store->ctx, stored_argument);
            bool landing_ready = !carries || !landing_scope_closed ||
                materialized_copy ||
                semantic_formal_store_landing_select_held(
                    state->store->ctx, node, stored);
            bool planned_copy = landing_scope_closed &&
                semantic_formal_store_landing_selects_value(
                    state->store->ctx, node, stored_argument);
            bool landing_copies = carries && (
                materialized_copy ||
                semantic_collection_owned_string_push_effect_current(
                    node, ast_call_argument(node, 0), state->store->ctx) ||
                planned_copy);
            if (!landing_ready) {
                fact->complete = false;
                function_param_flow_fail(state->store, node,
                    "formal String store landing receipt could not be recorded");
            }
            ASTNode *stored_value = ast_call_argument(node, stored);
            ASTNode *landing_root = stored_value;
            while (landing_root != NULL && (landing_root->type == AST_MEMBER_ACCESS ||
                    landing_root->type == AST_ARRAY_ACCESS))
                landing_root = landing_root->type == AST_MEMBER_ACCESS
                    ? ast_member_object(landing_root)
                    : ast_array_access_array(landing_root);
            /* A named carrier is already materialized before this store.  In
             * a sparse return-loan walk, expose the selected store as its
             * first copy boundary so the producer call is not cloned too.
             * A direct call stays under the call-result landing policy. */
            if (state->loan_seed != NULL && landing_copies &&
                landing_root != NULL && landing_root->type == AST_IDENTIFIER &&
                state->escape_site == NULL) {
                state->escape_site = node;
                state->escape_carrier = stored_value;
            }
            /* Keep the relation in a loan proof, where the selected store is
             * the observable end of the loan.  The public routine summary
             * instead describes the independent value after that copy. */
            if (state->loan_seed != NULL || !landing_copies)
                backing_store(state, ast_call_argument(node, 0), bits, false,
                    ast_call_argument(node, stored));
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                backing_consumers(state, ast_call_argument(node, p));
            break;
        }
        size_t collection_stored = 0;
        bool collection_change =
            semantic_param_publication_checked_collection_change(
                state->store->ctx, node, &collection_stored);
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            ASTNode *arg = ast_call_argument(node, i);
            if (i == 1 && semantic_array_storage_independent_physical_hof_result(node, state->store->ctx)) {
                backing_consumers(state, arg); continue;
            }
            const uint64_t *bits = backing_origin(state, arg);
            bool local = semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) ||
                semantic_typed_map_query_receiver_read(state->store->ctx, node, i) ||
                semantic_param_publication_checked_array_pop(state->store->ctx, node);
            /* A checked HashMap/Set/List/Queue mutator changes its receiver
             * in place; it does not publish that receiver through an opaque
             * call boundary.  The mutation summary remains owned by the
             * typed operation and slot walk, while stored value backing is
             * handled independently at its exact argument. */
            local |= collection_change && i == 0;
            ASTNode *receiver = ast_call_argument(node, 0);
            uint32_t owner_id = state->loan_seed != NULL ? state->loan_owner_slot_id : state->fact->input_id;
            bool owner_store = i == 1 && owner_id != 0 &&
                semantic_param_publication_checked_slot_write(state->store->ctx, node) &&
                receiver != NULL && receiver->type == AST_IDENTIFIER &&
                semantic_param_publication_binding(state->store->ctx, receiver) == owner_id;
            bool depends = false, retains = true;
            local |= semantic_param_publication_checked_backing_relation(state->store->ctx, node, i, &depends, &retains) && !retains;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            size_t formal = 0;
            if (callee != NULL && !semantic_param_publication_formal_for_actual(
                    state->store->ctx, node, i, &formal)) { fact->complete = false; break; }
            bool self = callee == state->decl && !state->decl->is_async_decl && arg->type == AST_IDENTIFIER &&
                ast_func_param(callee, formal)->mode == PARAM_MODE_REF &&
                semantic_param_publication_binding(state->store->ctx, arg) == ast_func_param_stable_id(ast_func_param(callee, formal));
            for (size_t p = 0; self && p < ast_func_param_count(callee); p++) if (ast_func_param(callee, p)->mode == PARAM_MODE_MUT_REF) self = false;
            if (bits != NULL && backing_any_origin(state, bits) && !local && !self && !owner_store) {
                if (callee == NULL) {
                    if (!function_param_flow_backing_unsupported_call(state->store, fact, node))
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                }
                else {
                    FuncParam *param = ast_func_param(callee, formal);
                    /* A copy can stand in for an argument a by-value formal receives;
                     * inout is the caller's place, own and ref take a named variable. */
                    ASTNode *carrier = param != NULL && param->mode == PARAM_MODE_DEFAULT ? arg : NULL;
                    unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
                    bool owned_sink =
                        semantic_param_publication_owned_sink_formal_current(
                            state->store->ctx, callee, formal);
                    bool outward_ready = owned_sink ||
                        function_param_flow_callee_outward_effects_current(
                            &state->lookup, node, i, &outward_effects);
                    bool relation_ready = owned_sink ||
                        function_param_flow_callee_relation_ready(
                            &state->lookup, node, i);
                    if (owned_sink)
                        outward_effects = SLOT_PARAM_SUMMARY_CALL_ESCAPE;
                    SemanticPublishedValue actual =
                        semantic_param_publication_value(state->store->ctx,
                            arg);
                    bool aggregate_storage = actual.ok && !actual.meta &&
                        !semantic_ownership_type_holds_no_container(
                            actual.type, state->store->ctx);
                    bool preserves_storage = state->loan_seed == NULL ||
                        !relation_ready || (actual.ok && (!aggregate_storage ||
                            function_param_flow_preserves_array_storage(
                                state->store->ctx, callee, formal)));
                    unsigned escape_effect = outward_ready ? outward_effects &
                        (SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                         SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) : 0;
                    /* A loan handed to a formal whose settled storage proof
                     * fails changes the loan's owner: the move owner sees only
                     * access paths.  A recursive edge can expose a provisional
                     * preservation failure before its backing relation closes;
                     * do not make that transient answer a monotone CALL fact. */
                    if (param == NULL || param->mode == PARAM_MODE_OWN ||
                        (state->loan_seed != NULL && relation_ready &&
                         !preserves_storage))
                        escape_effect |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
                    FuncParam *input = ast_func_param(state->decl,
                        state->fact->input_ordinal);
                    bool movable_input = input != NULL &&
                        (input->mode == PARAM_MODE_DEFAULT ||
                         input->mode == PARAM_MODE_OWN);
                    /* An own sink consumes an owned/default input during the
                     * nested synchronous call; it does not retain that
                     * backing past the current routine's return. Keep the
                     * public CALL movement without publishing false outward
                     * retention. A ref/inout frame has no such authority:
                     * its own-sink handoff remains an outward escape unless
                     * copy landing first makes the local carrier independent. */
                    if (owned_sink && movable_input)
                        backing_movement_effect_carried(state, bits,
                            SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier);
                    else
                        backing_effect_carried(state, bits, escape_effect,
                            carrier);
                    if (param != NULL && param->mode == PARAM_MODE_MUT_REF &&
                        !backing_inout_argument_leaves_input(state))
                        backing_movement_effect_carried(state, bits,
                            SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier);
                    if (!relation_ready)
                        function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                    else if (!outward_ready) {
                        fact->complete = false;
                        break;
                    }
                    for (size_t p = 0; !owned_sink &&
                        p < ast_call_arg_count(node); p++) {
                        size_t output_formal = 0;
                        if (!semantic_param_publication_formal_for_actual(
                                state->store->ctx, node, p, &output_formal)) { fact->complete = false; break; }
                        FuncParam *output = ast_func_param(callee, output_formal);
                        if (output == NULL) { fact->complete = false; break; }
                        if (output->mode != PARAM_MODE_MUT_REF && output->mode != PARAM_MODE_REF) continue;
                        if (output->mode == PARAM_MODE_REF && output->type != NULL && ast_type_name(output->type) != NULL &&
                            strcmp(ast_type_name(output->type), "String") == 0) continue;
                        bool retained = false;
                        if (!function_param_flow_output_dependency_for_call(&state->lookup, node, i, p, &retained))
                            function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                        else if (retained) backing_store(state, ast_call_argument(node, p), bits, false, NULL);
                    }
                }
            }
            backing_consumers(state, arg);
        }
        break;
    case AST_RETURN: {
        if (state->checking_declared_result) {
            state->result_return_seen = true;
            if (ast_return_value(node) == NULL) fact->complete = false;
        }
        const uint64_t *bits = backing_origin(state, ast_return_value(node));
        if (bits != NULL) {
            if (backing_any_origin(state, bits)) {
                if (backing_value_contains_dependent_clone(state,
                        ast_return_value(node), 0))
                    fact->returned_dependent_clone = true;
                FunctionParamFlowResultPath returned = {0};
                if (!backing_value_origin_path(state, ast_return_value(node),
                        &returned) || !returned.present ||
                    !function_param_flow_result_path_join(
                        &fact->result_path, &returned)) {
                    fact->complete = false;
                    break;
                }
            }
            backing_effect_carried(state, bits,
                SLOT_PARAM_SUMMARY_RETURN_ESCAPE, ast_return_value(node));
        }
        backing_consumers(state, ast_return_value(node));
        break;
    }
    case AST_CHANNEL_SEND: {
        const uint64_t *bits = backing_origin(state, ast_channel_send_value(node));
        if (bits != NULL) backing_effect(state, bits, SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE);
        backing_consumers(state, ast_channel_send_value(node)); break;
    }
    case AST_IF_STMT:
        backing_consumers(state, ast_if_condition(node)); backing_consumers(state, ast_if_then_branch(node));
        backing_consumers(state, ast_if_else_branch(node)); break;
    case AST_MATCH_STMT: {
        ASTNode *subject = ast_match_subject(node);
        backing_consumers(state, subject);
        const uint64_t *bits = backing_origin(state, subject);
        if (bits == NULL) { fact->complete = false; break; }
        for (size_t c = 0; c < ast_match_case_count(node) && fact->complete; c++) {
            ASTNode *arm = ast_match_case_at(node, c);
            if (arm == NULL || arm->type != AST_MATCH_CASE) { fact->complete = false; break; }
            for (size_t p = 0; p < ast_match_case_pattern_count(arm); p++)
                backing_match_bindings(state, ast_match_case_pattern_at(arm, p), subject, bits);
            backing_consumers(state, ast_match_case_guard(arm));
            backing_consumers(state, ast_match_case_body(arm));
        }
        backing_consumers(state, ast_match_default_body(node));
        break;
    }
    case AST_BINARY: backing_consumers(state, ast_binary_left(node)); backing_consumers(state, ast_binary_right(node)); break;
    case AST_UNARY: backing_consumers(state, ast_unary_operand(node)); break;
    case AST_CAST:
        if (!semantic_param_publication_checked_numeric_cast(state->store->ctx, node)) fact->complete = false;
        backing_consumers(state, ast_cast_operand(node)); break;
    case AST_MEMBER_ACCESS:
        if (!semantic_param_publication_checked_unit_enum_selection(state->store->ctx, node))
            backing_consumers(state, ast_member_object(node));
        break;
    case AST_ARRAY_ACCESS: backing_consumers(state, ast_array_access_array(node)); backing_consumers(state, ast_array_access_index(node)); break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < (node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node) : ast_tuple_literal_count(node)); i++)
            backing_consumers(state, node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i) : ast_tuple_literal_element(node, i));
        break;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(node); i++)
            backing_consumers(state, ast_set_literal_element(node, i));
        break;
    case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: break;
    default: fact->complete = false; break;
    }
    state->consumer = previous_consumer;
}

void backing_consumers(ReturnedBacking *state, ASTNode *node)
{
    backing_consumers_impl(state, node);
}
