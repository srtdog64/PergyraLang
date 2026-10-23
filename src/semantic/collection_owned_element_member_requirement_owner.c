/* Captures and validates the String-element requirement induced when an
 * Array field moved out of a default aggregate is deep-mutated. */
#include "collection_owned_element_requirement_internal.h"

#include "builtin_kind.h"
#include "collection_ownership_fact_internal.h"
#include "default_aggregate_move_rule.h"
#include "diag_codes.h"
#include "function_param_publication_internal.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

#include <string.h>

bool
semantic_collection_owned_element_requirement_record_member_push(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    ASTNode *function = ctx != NULL ? ctx->current_function_decl : NULL;
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact = receiver_collection_fact(
        receiver, ctx, &binding);
    PgyCollectionMemberPath member_path;
    size_t formal = SIZE_MAX;
    ParamMode formal_mode = PARAM_MODE_DEFAULT;
    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || ast_call_arg_count(call) != 2
        || ast_call_argument(call, 0) != receiver
        || function == NULL || function->type != AST_FUNC_DECL
        || binding == NULL || binding->decl_syntax_id == 0
        || ast_identifier_binding_syntax_id(receiver)
            != binding->decl_syntax_id
        || fact == NULL || fact->member_move_source == NULL
        || !collection_member_move_path_identity(
            fact->member_move_source, ctx, &member_path)
        || (!(collection_member_move_from_current_default_parameter(
                  fact, binding, ctx, &formal)
              || (formal_mode = PARAM_MODE_MUT_REF,
                  collection_member_move_from_current_inout_parameter_at(
                      fact, binding, ctx, &formal))))
        || formal >= ast_func_param_count(function))
        return collection_owned_requirement_failure(ctx, call,
            "missing aggregate member-push identity");
    FuncParam *parameter = ast_func_param(function, formal);
    if (parameter == NULL || parameter->mode != formal_mode
        || ast_func_param_stable_id(parameter)
            != fact->source_binding_syntax_id
        || ast_node_stable_id(call) == 0
        || ast_node_stable_id(receiver) == 0
        || fact->member_move_source_id == 0
        || ast_node_stable_id(fact->member_move_source)
            != fact->member_move_source_id
        || !collection_call_is_unshadowed_builtin(
            call, "ArrayPushOwnedString"))
        return collection_owned_requirement_failure(ctx, call,
            "unresolved aggregate member-push identity");
    for (size_t i = 0; i < store->member_push_count; i++) {
        OwnedElementMemberPush *row = &store->member_pushes[i];
        if (row->call != call && row->call_id != ast_node_stable_id(call))
            continue;
        return row->call == call && row->receiver == receiver
            && row->receiver_id == ast_node_stable_id(receiver)
            && row->receiver_binding_id == binding->decl_syntax_id
            && row->function == function
            && row->function_id == ast_node_stable_id(function)
            && row->formal_ordinal == formal
            && row->formal_id == ast_func_param_stable_id(parameter)
            && row->formal_mode == formal_mode
            && row->member_source == fact->member_move_source
            && row->member_source_id == fact->member_move_source_id
            && row->root_binding_id == fact->source_binding_syntax_id
            && row->leaf_field_id == fact->origin_syntax_id
            && row->source_birth_serial == fact->source_birth_serial
            && row->field_count == member_path.field_count
            && memcmp(row->field_ids, member_path.field_ids,
                member_path.field_count * sizeof(*row->field_ids)) == 0;
    }
    OwnedElementMemberPush *grown =
        collection_owned_requirement_rows_reserve(
            store->member_pushes, &store->member_push_capacity,
            store->member_push_count + 1, sizeof(*store->member_pushes));
    if (grown == NULL)
        return collection_owned_requirement_failure(ctx, call,
            "aggregate member-push allocation failed");
    store->member_pushes = grown;
    store->member_pushes[store->member_push_count++] =
        (OwnedElementMemberPush){
            .call = call, .receiver = receiver, .function = function,
            .member_source = fact->member_move_source,
            .call_id = ast_node_stable_id(call),
            .receiver_id = ast_node_stable_id(receiver),
            .receiver_binding_id = binding->decl_syntax_id,
            .function_id = ast_node_stable_id(function),
            .formal_id = ast_func_param_stable_id(parameter),
            .member_source_id = fact->member_move_source_id,
            .root_binding_id = fact->source_binding_syntax_id,
            .leaf_field_id = fact->origin_syntax_id,
            .source_birth_serial = fact->source_birth_serial,
            .formal_ordinal = formal,
            .formal_mode = formal_mode,
            .field_count = member_path.field_count};
    memcpy(store->member_pushes[store->member_push_count - 1].field_ids,
        member_path.field_ids,
        member_path.field_count * sizeof(*member_path.field_ids));
    return true;
}

static bool
aggregate_actual_current(SemanticContext *ctx,
                         const OwnedElementAggregateActual *actual)
{
    size_t mapped_formal = SIZE_MAX;
    return actual != NULL && actual->call != NULL
        && actual->call->type == AST_CALL && actual->call_id != 0
        && ast_node_stable_id(actual->call) == actual->call_id
        && actual->callee != NULL && actual->callee->type == AST_FUNC_DECL
        && actual->callee_id != 0
        && ast_node_stable_id(actual->callee) == actual->callee_id
        && actual->caller != NULL
        && actual->caller->type
            == (actual->caller_is_intent ? AST_INTENT_DECL : AST_FUNC_DECL)
        && actual->caller_id != 0
        && ast_node_stable_id(actual->caller) == actual->caller_id
        && actual->argument != NULL && actual->argument_id != 0
        && ast_node_stable_id(actual->argument) == actual->argument_id
        && actual->argument_ordinal < ast_call_arg_count(actual->call)
        && ast_call_argument(actual->call, actual->argument_ordinal)
            == actual->argument
        && actual->formal != NULL
        && actual->formal_ordinal < ast_func_param_count(actual->callee)
        && ast_func_param(actual->callee, actual->formal_ordinal)
            == actual->formal
        && actual->formal_id != 0
        && ast_func_param_stable_id(actual->formal) == actual->formal_id
        && actual->formal->mode == actual->formal_mode
        && semantic_param_publication_checked_argument(
            ctx, actual->call, actual->argument_ordinal)
        && semantic_param_publication_formal_for_actual(
            ctx, actual->call, actual->argument_ordinal, &mapped_formal)
        && mapped_formal == actual->formal_ordinal
        && ast_call_semantic_callee_decl_id(actual->call)
            == actual->callee_id
        && semantic_param_publication_value(ctx, actual->argument).ok;
}

typedef struct OwnedElementAggregateRequirementVisit {
    uint32_t function_id;
    uint32_t formal_id;
    const uint32_t *field_ids;
    size_t field_count;
    bool *cycle_seen;
    const struct OwnedElementAggregateRequirementVisit *parent;
} OwnedElementAggregateRequirementVisit;

typedef struct OwnedElementAggregateValueVisit {
    uint32_t function_id;
    uint32_t value_id;
    bool local_binding;
    const uint32_t *field_ids;
    size_t field_count;
    bool *cycle_seen;
    size_t memo_index;
    const struct OwnedElementAggregateValueVisit *parent;
} OwnedElementAggregateValueVisit;

typedef struct OwnedElementAggregateCallFrame {
    ASTNode *call;
    ASTNode *callee;
    ASTNode *caller;
} OwnedElementAggregateCallFrame;

static bool
aggregate_requirement_same(
    const OwnedElementAggregateRequirementVisit *visit,
    uint32_t function_id, uint32_t formal_id,
    const uint32_t *field_ids, size_t field_count)
{
    return visit != NULL && visit->function_id == function_id
        && visit->formal_id == formal_id
        && visit->field_count == field_count
        && (field_count == 0 || memcmp(visit->field_ids, field_ids,
            field_count * sizeof(*field_ids)) == 0);
}

static bool
aggregate_requirement_memo_same(
    const OwnedElementAggregateRequirementMemo *memo,
    uint32_t function_id, uint32_t formal_id,
    const uint32_t *field_ids, size_t field_count)
{
    return memo != NULL && memo->function_id == function_id
        && memo->formal_id == formal_id
        && memo->field_count == field_count
        && (field_count == 0 || memcmp(memo->field_ids, field_ids,
            field_count * sizeof(*field_ids)) == 0);
}

static bool aggregate_requirement_validate(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t formal_id,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateRequirementVisit *parent,
    ASTNode *site, bool *proof_cycle_seen);

static bool
aggregate_value_visit_same(const OwnedElementAggregateValueVisit *visit,
                           uint32_t function_id, uint32_t value_id,
                           bool local_binding,
                           const uint32_t *field_ids, size_t field_count)
{
    return visit != NULL && visit->function_id == function_id
        && visit->value_id == value_id
        && visit->local_binding == local_binding
        && visit->field_count == field_count
        && (field_count == 0 || memcmp(visit->field_ids, field_ids,
            field_count * sizeof(*field_ids)) == 0);
}

static void
aggregate_value_cycle_mark(
    CollectionOwnedElementRequirementStore *store,
    const OwnedElementAggregateValueVisit *parent,
    const OwnedElementAggregateValueVisit *matched)
{
    size_t root = matched->memo_index;
    size_t root_depth = 0;
    size_t depth = 0;
    for (const OwnedElementAggregateValueVisit *visit = parent;
         visit != NULL; visit = visit->parent, depth++)
        if (visit == matched) {
            root_depth = depth;
            break;
        }
    for (const OwnedElementAggregateValueVisit *visit = parent;
         visit != NULL; visit = visit->parent) {
        size_t existing = store->aggregate_proof_memos[visit->memo_index]
            .cycle_root_plus_one;
        if (existing != 0) {
            size_t candidate = existing - 1u;
            size_t candidate_depth = 0;
            bool active = false;
            size_t at = 0;
            for (const OwnedElementAggregateValueVisit *scan = parent;
                 scan != NULL; scan = scan->parent, at++)
                if (scan->memo_index == candidate) {
                    candidate_depth = at;
                    active = true;
                    break;
                }
            if (active && candidate_depth > root_depth) {
                root = candidate;
                root_depth = candidate_depth;
            }
        }
        if (visit == matched)
            break;
    }
    for (const OwnedElementAggregateValueVisit *visit = parent;
         visit != NULL; visit = visit->parent) {
        size_t existing = store->aggregate_proof_memos[visit->memo_index]
            .cycle_root_plus_one;
        if (existing != 0 && existing - 1u != root) {
            size_t old_root = existing - 1u;
            for (size_t i = 0; i < store->aggregate_proof_memo_count; i++)
                if (store->aggregate_proof_memos[i].cycle_root_plus_one
                        == old_root + 1u)
                    store->aggregate_proof_memos[i].cycle_root_plus_one =
                        root + 1u;
        }
        store->aggregate_proof_memos[visit->memo_index]
            .cycle_root_plus_one = root + 1u;
        if (visit == matched)
            break;
    }
    store->aggregate_proof_memos[root].cycle_root_plus_one = root + 1u;
}

static FuncParam *
aggregate_formal_for_binding(ASTNode *function, uint32_t binding_id,
                             size_t *ordinal_out)
{
    if (ordinal_out != NULL) *ordinal_out = SIZE_MAX;
    if (function == NULL || function->type != AST_FUNC_DECL
        || binding_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_func_param_count(function); i++) {
        FuncParam *formal = ast_func_param(function, i);
        if (formal == NULL
            || ast_func_param_stable_id(formal) != binding_id)
            continue;
        if (ordinal_out != NULL) *ordinal_out = i;
        return formal;
    }
    return NULL;
}

static ASTNode *
aggregate_frame_actual(SemanticContext *ctx,
                       const OwnedElementAggregateCallFrame *frame,
                       size_t formal_ordinal)
{
    ASTNode *selected = NULL;
    if (frame == NULL || frame->call == NULL
        || frame->call->type != AST_CALL
        || frame->callee == NULL || frame->callee->type != AST_FUNC_DECL
        || !semantic_param_publication_checked_call(ctx, frame->call))
        return NULL;
    for (size_t i = 0; i < ast_call_arg_count(frame->call); i++) {
        size_t mapped = SIZE_MAX;
        if (!semantic_param_publication_formal_for_actual(
                ctx, frame->call, i, &mapped))
            return NULL;
        if (mapped != formal_ordinal)
            continue;
        if (selected != NULL
            || !semantic_param_publication_checked_argument(
                ctx, frame->call, i))
            return NULL;
        selected = ast_call_argument(frame->call, i);
    }
    return selected;
}

static bool aggregate_value_proves(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *function, ASTNode *value, uint32_t value_binding_id,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateCallFrame *frame,
    const OwnedElementAggregateRequirementVisit *requirements,
    const OwnedElementAggregateValueVisit *parent);

static bool
aggregate_constructor_proves(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *function, ASTNode *value,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateCallFrame *frame,
    const OwnedElementAggregateRequirementVisit *requirements,
    const OwnedElementAggregateValueVisit *parent)
{
    PublishedNode *published = publication_result_current(ctx, value);
    const SemanticNominalResultSnapshot *mapping = published != NULL
        ? published->nominal_result : NULL;
    if (mapping == NULL
        || !semantic_nominal_result_snapshot_current(ctx, value, mapping))
        return false;
    const SemanticNominalResultField *selected = NULL;
    ASTNode *actual = NULL;
    for (size_t i = 0; i < mapping->field_count; i++) {
        const SemanticNominalResultField *candidate = &mapping->fields[i];
        if (candidate->field_id != field_ids[0])
            continue;
        if (selected != NULL || i >= ast_call_arg_count(value))
            return false;
        selected = candidate;
        actual = ast_call_argument(value, i);
    }
    if (selected == NULL || actual == NULL)
        return false;
    Type *field_type = semantic_host_resolve_type_ref(
        selected->type_node, ctx);
    if (field_count == 1) {
        bool ready = collection_owned_is_string_array(field_type)
            && selected->string_array_elements_present
            && selected->string_array_elements.owned
            && semantic_array_storage_transferred_element_use_current(
                actual, ctx, &selected->string_array_elements);
        if (!ready && semantic_loan_copy_landing_satisfied(ctx, actual))
            ready = true;
        /* A constructor must not silently turn a shallow Array<String>
         * descriptor into an owned field.  When Clone can copy the complete
         * actual, select that exact occurrence for the driver's next landed
         * analysis.  The deferred diagnostic keeps this analysis unusable;
         * only the reloaded AST with the Clone may establish `ready`. */
        if (!ready
            && semantic_loan_copy_landing_carrier_copies(ctx, actual)
            && semantic_loan_copy_landing_select_carrier(ctx, actual)) {
            semantic_loan_copy_landing_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual,
                "An aggregate String-array field requires an independent owned copy of its source storage");
            semantic_loan_copy_landing_refusal_tail_seal(ctx);
            return true;
        }
        return ready;
    }
    bool nested = semantic_default_aggregate_value(ctx, field_type)
        && !type_is_constructed_named(field_type, "Array")
        && aggregate_value_proves(ctx, store, function, actual, 0,
            field_ids + 1, field_count - 1, frame, requirements, parent);
    return nested;
}

static bool
aggregate_return_current(SemanticContext *ctx,
                         const OwnedElementAggregateReturn *row)
{
    if (row == NULL)
        return false;
    bool identity = row->statement != NULL
        && row->statement->type == AST_RETURN
        && row->statement_id != 0
        && ast_node_stable_id(row->statement) == row->statement_id
        && row->value != NULL && row->value_id != 0
        && ast_node_stable_id(row->value) == row->value_id
        && ast_return_value(row->statement) == row->value
        && row->function != NULL && row->function->type == AST_FUNC_DECL
        && row->function_id != 0
        && ast_node_stable_id(row->function) == row->function_id
        && row->value_type != NULL && row->value_type != TYPE_UNKNOWN;
    bool inputs = identity
        && semantic_param_publication_routine_inputs_current(
            ctx, row->function, true);
    bool result = inputs
        && semantic_param_publication_routine_result_current(
            ctx, row->function, row->value_type);
    SemanticPublishedValue value = identity
        ? semantic_param_publication_value(ctx, row->value)
        : (SemanticPublishedValue){0};
    bool binding = identity
        && ((row->value->type == AST_IDENTIFIER
                && (row->value_binding_id != 0 || value.meta))
            || (row->value->type != AST_IDENTIFIER
                && row->value_binding_id == 0));
    bool current = result && binding && value.ok && !value.meta
        && type_equals(value.type, row->value_type);
    return current;
}

static bool
aggregate_local_proves(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *function, uint32_t binding_id,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateCallFrame *frame,
    const OwnedElementAggregateRequirementVisit *requirements,
    const OwnedElementAggregateValueVisit *parent)
{
    for (const OwnedElementAggregateValueVisit *visit = parent;
         visit != NULL; visit = visit->parent) {
        if (aggregate_value_visit_same(visit,
                ast_node_stable_id(function), binding_id, true,
                field_ids, field_count)) {
            aggregate_value_cycle_mark(store, parent, visit);
            for (const OwnedElementAggregateValueVisit *mark = parent;
                 mark != NULL; mark = mark->parent) {
                if (mark->cycle_seen != NULL)
                    *mark->cycle_seen = true;
                if (mark == visit)
                    break;
            }
            return true;
        }
    }
    size_t memo_index = SIZE_MAX;
    OwnedElementAggregateMemoLookup memo =
        collection_owned_proof_memo_begin(ctx, store, function,
            OWNED_ELEMENT_AGGREGATE_PROOF_LOCAL,
            ast_node_stable_id(function), binding_id,
            frame != NULL ? ast_node_stable_id(frame->call) : 0,
            field_ids, field_count, &memo_index);
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_ERROR
        || memo == OWNED_ELEMENT_AGGREGATE_MEMO_REFUTED) {
        return false;
    }
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_PROVED)
        return true;
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_VISITING
        && parent != NULL && parent->cycle_seen != NULL) {
        *parent->cycle_seen = true;
        return true;
    }
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_VISITING)
        return true;
    bool cycle_seen = false;
    OwnedElementAggregateValueVisit visit = {
        ast_node_stable_id(function), binding_id, true,
        field_ids, field_count, &cycle_seen, memo_index, parent};
    FunctionParamPublicationStore *publication =
        ctx->function_param_publications;
    PublishedNode *definition = publication_node_by_id(
        publication, binding_id);
    ASTNode *declaration = definition != NULL
        ? (ASTNode *)definition->node : NULL;
    if (declaration == NULL || declaration->type != AST_LET_DECL
        || ast_node_stable_id(declaration) != binding_id
        || ast_let_initializer(declaration) == NULL) {
        return collection_owned_proof_memo_resolve(
            store, memo_index, false, false);
    }
    if (!aggregate_value_proves(ctx, store, function,
            ast_let_initializer(declaration), 0, field_ids, field_count,
            frame, requirements, &visit)) {
        return collection_owned_proof_memo_resolve(
            store, memo_index, false, false);
    }
    const OwnedElementAggregateAssignmentIndexBucket *bucket =
        collection_owned_assignment_index_find(store,
            ast_node_stable_id(function), binding_id);
    for (size_t link = bucket != NULL ? bucket->head_plus_one : 0;
         link != 0;) {
        const OwnedElementAggregateAssignmentIndexRow *indexed =
            &store->aggregate_assignment_rows[link - 1];
        PublishedNode *row = (PublishedNode *)indexed->publication;
        link = indexed->next_plus_one;
        if (row->node == NULL
            || !semantic_param_publication_assignment_replacement(
                ctx, row->node)
            || row->caller != function
            || row->caller_id != ast_node_stable_id(function)) {
            return collection_owned_proof_memo_resolve(
                store, memo_index, false, false);
        }
        if (!aggregate_value_proves(ctx, store, function,
                (ASTNode *)row->assignment_rhs, 0, field_ids, field_count,
                 frame, requirements, &visit)) {
            return collection_owned_proof_memo_resolve(
                store, memo_index, false, false);
        }
    }
    return collection_owned_proof_memo_resolve(
        store, memo_index, true, !cycle_seen);
}

static bool
aggregate_call_result_proves(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *caller, ASTNode *call,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateRequirementVisit *requirements,
    const OwnedElementAggregateValueVisit *parent)
{
    const Type *result_type = NULL;
    ASTNode *callee = semantic_param_publication_declared_result(
        ctx, call, &result_type);
    if (callee == NULL || result_type == NULL
        || !semantic_default_aggregate_value(ctx, result_type)
        || type_is_constructed_named(result_type, "Array"))
        return false;
    for (const OwnedElementAggregateValueVisit *visit = parent;
         visit != NULL; visit = visit->parent) {
        if (aggregate_value_visit_same(visit,
                ast_node_stable_id(caller), ast_node_stable_id(call), false,
                field_ids, field_count)) {
            aggregate_value_cycle_mark(store, parent, visit);
            for (const OwnedElementAggregateValueVisit *mark = parent;
                 mark != NULL; mark = mark->parent) {
                if (mark->cycle_seen != NULL)
                    *mark->cycle_seen = true;
                if (mark == visit)
                    break;
            }
            return true;
        }
    }
    size_t memo_index = SIZE_MAX;
    OwnedElementAggregateMemoLookup memo =
        collection_owned_proof_memo_begin(ctx, store, call,
            OWNED_ELEMENT_AGGREGATE_PROOF_CALL_RESULT,
            ast_node_stable_id(caller), ast_node_stable_id(call),
            0,
            field_ids, field_count, &memo_index);
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_ERROR
        || memo == OWNED_ELEMENT_AGGREGATE_MEMO_REFUTED) {
        return false;
    }
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_PROVED)
        return true;
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_VISITING
        && parent != NULL && parent->cycle_seen != NULL) {
        *parent->cycle_seen = true;
        return true;
    }
    if (memo == OWNED_ELEMENT_AGGREGATE_MEMO_VISITING)
        return true;
    bool cycle_seen = false;
    OwnedElementAggregateValueVisit visit = {
        ast_node_stable_id(caller), ast_node_stable_id(call), false,
        field_ids, field_count, &cycle_seen, memo_index, parent};
    OwnedElementAggregateCallFrame next = {
        call, callee, caller};
    bool saw_return = false;
    const OwnedElementAggregateReturnIndexBucket *bucket =
        collection_owned_return_index_find(
            store, ast_node_stable_id(callee));
    for (size_t link = bucket != NULL ? bucket->head_plus_one : 0;
         link != 0;) {
        size_t i = link - 1;
        link = store->aggregate_return_next_plus_one[i];
        const OwnedElementAggregateReturn *row =
            &store->aggregate_returns[i];
        saw_return = true;
        bool current = row->function == callee
            && aggregate_return_current(ctx, row)
            && type_equals(row->value_type, result_type);
        bool proves = current
            && aggregate_value_proves(ctx, store, callee, row->value,
                row->value_binding_id, field_ids, field_count,
                &next, requirements, &visit);
        if (!proves) {
            return collection_owned_proof_memo_resolve(
                store, memo_index, false, false);
        }
    }
    return collection_owned_proof_memo_resolve(
        store, memo_index, saw_return, saw_return && !cycle_seen);
}

static bool
aggregate_value_proves(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *function, ASTNode *value, uint32_t value_binding_id,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateCallFrame *frame,
    const OwnedElementAggregateRequirementVisit *requirements,
    const OwnedElementAggregateValueVisit *parent)
{
    SemanticPublishedValue published;
    if (function == NULL || function->type != AST_FUNC_DECL
        || ast_node_stable_id(function) == 0 || value == NULL
        || field_ids == NULL || field_count == 0
        || field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT
        || !semantic_param_publication_routine_inputs_current(
            ctx, function, true))
        return false;
    published = semantic_param_publication_value(ctx, value);
    if (!published.ok || published.meta || published.type == NULL
        || !semantic_default_aggregate_value(ctx, published.type)
        || type_is_constructed_named(published.type, "Array")) {
        return false;
    }
    if (value->type == AST_CALL) {
        PublishedNode *call = publication_result_current(ctx, value);
        if (call != NULL && call->nominal_result != NULL)
            return aggregate_constructor_proves(ctx, store, function, value,
                field_ids, field_count, frame, requirements, parent);
        uint32_t builtin = 0;
        bool depends = true;
        bool retains = true;
        if (ast_call_arg_count(value) == 1
            && ast_call_semantic_callee_builtin_kind(value, &builtin)
            && builtin == BUILTIN_CLONE
            && semantic_param_publication_checked_result_relation(
                ctx, value, 0, &depends, &retains)
            && !depends && !retains
            && semantic_param_publication_clone_independent(
                ctx, published.type))
            return true;
        return aggregate_call_result_proves(ctx, store, function, value,
            field_ids, field_count, requirements, parent);
    }
    if (value->type == AST_MEMBER_ACCESS) {
        ASTNode *caller = NULL;
        ASTNode *root = NULL;
        SemanticPublishedSelectedField selected = {0};
        uint32_t expanded[PGY_COLLECTION_MEMBER_PATH_LIMIT];
        if (field_count == PGY_COLLECTION_MEMBER_PATH_LIMIT
            || !semantic_param_publication_selected_member_field_current(
                ctx, value, &caller, &root, &selected)
            || caller != function || root == NULL
            || selected.field_id == 0
            || selected.field_type == NULL
            || !type_equals(selected.field_type, published.type)
            || !semantic_default_aggregate_value(ctx, selected.nominal_type)
            || type_is_constructed_named(selected.nominal_type, "Array")) {
            return false;
        }
        expanded[0] = selected.field_id;
        memcpy(expanded + 1, field_ids,
            field_count * sizeof(*field_ids));
        return aggregate_value_proves(ctx, store, function, root, 0,
            expanded, field_count + 1, frame, requirements, parent);
    }
    if (value->type != AST_IDENTIFIER) {
        return false;
    }
    uint32_t published_binding_id = ast_identifier_binding_syntax_id(value);
    uint32_t binding_id = value_binding_id != 0
        ? value_binding_id : published_binding_id;
    if (binding_id == 0
        || (published_binding_id != 0
            && published_binding_id != binding_id))
        return false;
    size_t formal_ordinal = SIZE_MAX;
    FuncParam *formal = aggregate_formal_for_binding(
        function, binding_id, &formal_ordinal);
    if (formal != NULL) {
        if (formal->mode == PARAM_MODE_REF) {
            return false;
        }
        if (frame != NULL && frame->callee == function) {
            ASTNode *actual = aggregate_frame_actual(
                ctx, frame, formal_ordinal);
            return actual != NULL
                && aggregate_value_proves(ctx, store, frame->caller, actual,
                    0, field_ids, field_count, NULL,
                    requirements, parent);
        }
        return aggregate_requirement_validate(ctx, store,
            ast_node_stable_id(function), binding_id,
            field_ids, field_count, requirements, value,
            parent != NULL ? parent->cycle_seen : NULL);
    }
    return aggregate_local_proves(ctx, store, function, binding_id,
        field_ids, field_count, frame, requirements, parent);
}

static bool
aggregate_requirement_validate(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t formal_id,
    const uint32_t *field_ids, size_t field_count,
    const OwnedElementAggregateRequirementVisit *parent,
    ASTNode *site, bool *proof_cycle_seen)
{
    size_t memo_index = SIZE_MAX;
    if (function_id == 0 || formal_id == 0 || field_ids == NULL
        || field_count == 0
        || field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT)
        return collection_owned_requirement_failure(ctx, site,
            "default aggregate member-push requirement identity is incomplete");
    for (const OwnedElementAggregateRequirementVisit *visit = parent;
         visit != NULL; visit = visit->parent)
        if (aggregate_requirement_same(
                visit, function_id, formal_id, field_ids, field_count)) {
            for (const OwnedElementAggregateRequirementVisit *mark = parent;
                 mark != NULL; mark = mark->parent) {
                if (mark->cycle_seen != NULL)
                    *mark->cycle_seen = true;
                if (mark == visit)
                    break;
            }
            if (proof_cycle_seen != NULL)
                *proof_cycle_seen = true;
            return true;
        }
    for (size_t i = 0; i < store->aggregate_requirement_memo_count; i++) {
        OwnedElementAggregateRequirementMemo *memo =
            &store->aggregate_requirement_memos[i];
        if (!aggregate_requirement_memo_same(
                memo, function_id, formal_id, field_ids, field_count))
            continue;
        if (memo->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED)
            return true;
        if (memo->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED)
            return false;
        if (memo->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_UNCACHED) {
            memo->state = OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING;
            memo_index = i;
            break;
        }
        return collection_owned_requirement_failure(ctx, site,
            "aggregate requirement cycle escaped its active visit chain");
    }
    if (memo_index == SIZE_MAX
        && store->aggregate_requirement_memo_count == SIZE_MAX)
        return collection_owned_requirement_failure(ctx, site,
            "default aggregate requirement memo bound overflow");
    if (memo_index == SIZE_MAX) {
        OwnedElementAggregateRequirementMemo *grown =
            collection_owned_requirement_rows_reserve(
                store->aggregate_requirement_memos,
                &store->aggregate_requirement_memo_capacity,
                store->aggregate_requirement_memo_count + 1,
                sizeof(*store->aggregate_requirement_memos));
        if (grown == NULL)
            return collection_owned_requirement_failure(ctx, site,
                "default aggregate requirement memo allocation failed");
        store->aggregate_requirement_memos = grown;
        memo_index = store->aggregate_requirement_memo_count++;
        store->aggregate_requirement_memos[memo_index] =
            (OwnedElementAggregateRequirementMemo){
                .function_id = function_id,
                .formal_id = formal_id,
                .field_count = field_count,
                .state = OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING};
        memcpy(store->aggregate_requirement_memos[memo_index].field_ids,
            field_ids, field_count * sizeof(*field_ids));
    }
    bool cycle_seen = false;
    OwnedElementAggregateRequirementVisit visit = {
        function_id, formal_id, field_ids, field_count,
        &cycle_seen, parent};
    const OwnedElementAggregateActualIndexBucket *bucket =
        collection_owned_actual_index_find(store, function_id, formal_id);
    for (size_t link = bucket != NULL ? bucket->head_plus_one : 0;
         link != 0;) {
        size_t i = link - 1;
        link = store->aggregate_actual_next_plus_one[i];
        const OwnedElementAggregateActual *actual =
            &store->aggregate_actuals[i];
        if (actual->callee_id != function_id
            || actual->formal_id != formal_id
            || !aggregate_actual_current(ctx, actual)) {
            store->aggregate_requirement_memos[memo_index].state =
                OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
            return collection_owned_requirement_failure(ctx, actual->call,
                "default aggregate call changed before owned-field validation");
        }
        if (actual->caller_is_intent) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual->argument,
                "Intent aggregate argument %zu cannot satisfy an owned String-array field requirement without a routine-owned source proof",
                actual->argument_ordinal + 1);
            store->aggregate_requirement_memos[memo_index].state =
                OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
            return false;
        }
        if (!aggregate_value_proves(ctx, store, actual->caller,
                actual->argument, 0, field_ids, field_count,
                NULL, &visit, NULL)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual->argument,
                "Moving aggregate parameter %zu requires every definition and return at its String-array field path to preserve owned elements",
                actual->formal_ordinal + 1);
            store->aggregate_requirement_memos[memo_index].state =
                OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
            return false;
        }
    }
    store->aggregate_requirement_memos[memo_index].state =
        cycle_seen
            ? OWNED_ELEMENT_AGGREGATE_REQUIREMENT_UNCACHED
            : OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED;
    return true;
}

bool
collection_owned_validate_member_pushes(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store)
{
    if (store->member_push_count != 0
        && !collection_owned_element_graph_index_build(ctx, store))
        return false;
    for (size_t i = 0; i < store->member_push_count; i++) {
        OwnedElementMemberPush row = store->member_pushes[i];
        const PgyCollectionOwnershipFact *fact =
            semantic_collection_ownership_fact_find(
                ctx, row.function_id, row.receiver_binding_id);
        FuncParam *parameter = row.function != NULL
            && row.function->type == AST_FUNC_DECL
            && row.formal_ordinal < ast_func_param_count(row.function)
            ? ast_func_param(row.function, row.formal_ordinal) : NULL;
        if (row.call == NULL || row.call->type != AST_CALL
            || row.call_id == 0
            || ast_node_stable_id(row.call) != row.call_id)
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push call identity changed before finalization");
        if (row.receiver == NULL || row.receiver->type != AST_IDENTIFIER
            || row.receiver_id == 0
            || ast_node_stable_id(row.receiver) != row.receiver_id
            || ast_call_arg_count(row.call) != 2
            || ast_call_argument(row.call, 0) != row.receiver
            || ast_identifier_binding_syntax_id(row.receiver)
                != row.receiver_binding_id)
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push receiver identity changed before finalization");
        if (row.function == NULL || row.function->type != AST_FUNC_DECL
            || row.function_id == 0
            || ast_node_stable_id(row.function) != row.function_id
            || parameter == NULL || parameter->mode != row.formal_mode
            || (row.formal_mode != PARAM_MODE_DEFAULT
                && row.formal_mode != PARAM_MODE_MUT_REF)
            || ast_func_param_stable_id(parameter) != row.formal_id)
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push formal identity changed before finalization");
        if (fact == NULL
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != (row.formal_mode == PARAM_MODE_DEFAULT
                ? PGY_COLLECTION_DISPOSITION_LIVE
                : PGY_COLLECTION_DISPOSITION_RETIRED)
            || fact->binding_syntax_id != row.receiver_binding_id
            || fact->source_binding_syntax_id != row.root_binding_id
            || fact->origin_syntax_id != row.leaf_field_id
            || fact->source_birth_serial != row.source_birth_serial
            || fact->member_move_source != row.member_source
            || fact->member_move_source_id != row.member_source_id)
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push collection fact changed before finalization");
        if (row.member_source == NULL
            || ast_node_stable_id(row.member_source) != row.member_source_id)
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push source path changed before finalization");
        if (!semantic_param_publication_checked_call(ctx, row.call))
            return collection_owned_requirement_failure(ctx, row.call,
                "aggregate member-push call publication is not current");
        if (row.formal_mode == PARAM_MODE_DEFAULT
            && !semantic_default_aggregate_formal_moves(
                ctx, row.function, row.formal_ordinal)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                row.call,
                "ArrayPushOwnedString on a moved aggregate field requires every call to move parameter syntax %u of callable syntax %u",
                row.formal_id, row.function_id);
            return false;
        }
        if (row.field_count == 0
            || row.field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT
            || !aggregate_requirement_validate(ctx, store,
                row.function_id, row.formal_id,
                row.field_ids, row.field_count, NULL, row.call, NULL))
            return false;
    }
    return true;
}
