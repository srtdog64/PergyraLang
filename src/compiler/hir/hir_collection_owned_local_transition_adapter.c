#include "hir_collection_owned_local_transition_adapter.h"
#include "hir_cfg.h"
#include "hir_lower_cfg.h"
#include "../collection_owned_local_transition_owner.h"
#include "../../common/string_compat.h"
#include "../../semantic/builtin_kind.h"
#include "../../semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct HIRSourceExpressionVisit {
    const ASTNode *node;
    const struct HIRSourceExpressionVisit *previous;
} HIRSourceExpressionVisit;
typedef struct {
    const HIRRoutine *routine;
    PgyCollectionTransitionOperation *rows;
    size_t count, capacity;
    const PgyCollectionEffectSnapshot *snapshot;
    bool *effect_seen;
    size_t caller, effect_count;
    bool source_only;
    const HIRSourceExpressionVisit *source_active;
    bool track_execution;
    bool *source_occurrences;
    size_t source_occurrence_count;
    const ASTNode *execution_root;
    bool *write_seen;
    size_t write_count;
    const ASTNode *failure_node;
    const char *failure_reason;
    uint32_t failure_binding;
    uint32_t failure_receipt_binding;
    uint32_t failure_receipt_source;
    uint32_t failure_receipt_kind;
    uint32_t *pending_retirements;
    size_t pending_retirement_count, pending_retirement_capacity;
} HIRCollectionOperations;

static const PgyCollectionOwnershipFact *
hir_owned_local_fact(const HIRRoutine *routine, uint32_t binding)
{
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact = &routine->collection_ownership_facts[i];
        if (fact->binding_syntax_id == binding && pgy_collection_owned_local_fact_tracked(fact))
            return fact;
    }
    return NULL;
}

static bool
hir_owned_operation_append(HIRCollectionOperations *operations,
                           PgyCollectionTransitionOperation operation)
{
    if (operations->count == operations->capacity) {
        size_t capacity = operations->capacity == 0 ? 8 : operations->capacity * 2;
        if (capacity < operations->capacity || capacity > SIZE_MAX / sizeof(operation))
            return false;
        void *grown = realloc(operations->rows, capacity * sizeof(operation));
        if (grown == NULL) return false;
        operations->rows = grown;
        operations->capacity = capacity;
    }
    operations->rows[operations->count++] = operation;
    return true;
}

static bool
hir_pending_retirement_append(HIRCollectionOperations *operations,
                              uint32_t binding, bool allow_existing)
{
    if (binding == 0)
        return false;
    for (size_t i = 0; i < operations->pending_retirement_count; i++) {
        if (operations->pending_retirements[i] != binding)
            continue;
        return allow_existing;
    }
    if (operations->pending_retirement_count
            == operations->pending_retirement_capacity) {
        size_t capacity = operations->pending_retirement_capacity == 0
            ? 4 : operations->pending_retirement_capacity * 2;
        if (capacity < operations->pending_retirement_capacity
            || capacity > SIZE_MAX / sizeof(*operations->pending_retirements))
            return false;
        void *grown = realloc(operations->pending_retirements,
            capacity * sizeof(*operations->pending_retirements));
        if (grown == NULL)
            return false;
        operations->pending_retirements = grown;
        operations->pending_retirement_capacity = capacity;
    }
    operations->pending_retirements[
        operations->pending_retirement_count++] = binding;
    return true;
}

static bool
hir_source_failure(HIRCollectionOperations *operations,
                   const ASTNode *node,
                   const char *reason)
{
    operations->failure_node = node;
    operations->failure_reason = reason;
    return false;
}

static bool
hir_owned_canonical_call(const ASTNode *call, const char *name,
                         size_t argument_count)
{
    ASTNode *callee = ast_call_callee(call);
    uint32_t builtin = 0;
    bool clone_ready = strcmp(name, "Clone") != 0
        || (ast_call_semantic_callee_builtin_kind(call, &builtin) && builtin == BUILTIN_CLONE);
    return call != NULL && call->type == AST_CALL && callee != NULL
        && callee->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(callee) == 0
        && ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && !ast_call_semantic_callee_declared_callable(call)
        && ast_call_arg_count(call) == argument_count
        && clone_ready
        && strcmp(ast_identifier_name(callee), name) == 0;
}

/* A defer statement registers its body; it does not execute the release in
 * the containing HIR block. Keep only the exact retired binding as a pending
 * MIR obligation. MIR later proves which return edges execute it. */
static bool
hir_owned_deferred_retirements(HIRCollectionOperations *operations,
                               ASTNode *defer_stmt)
{
    ASTNode *body = ast_defer_body(defer_stmt);
    if (body == NULL || body->type != AST_BLOCK)
        return hir_source_failure(operations, defer_stmt,
            "defer body shape");
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *part = ast_block_statement(body, i);
        if (part == NULL || part->type != AST_CALL)
            continue;
        ASTNode *receiver = ast_call_arg_count(part) > 0
            ? ast_call_argument(part, 0) : NULL;
        uint32_t receiver_binding = receiver != NULL
                && receiver->type == AST_IDENTIFIER
            ? ast_identifier_binding_syntax_id(receiver) : 0;
        uint32_t kind = 0, receipt_binding = 0, source = 0;
        bool receipt = ast_call_semantic_collection_effect(
            part, &kind, &receipt_binding, &source);
        bool release_call = hir_owned_canonical_call(
            part, "ArrayDropOwnedStrings", 1);
        const PgyCollectionOwnershipFact *receiver_fact =
            hir_owned_local_fact(operations->routine, receiver_binding);
        const PgyCollectionOwnershipFact *receipt_fact = receipt
            ? hir_owned_local_fact(operations->routine, receipt_binding)
            : NULL;
        if (!release_call && receipt_fact == NULL)
            continue;
        if (receiver_fact == NULL && receipt_fact == NULL)
            continue;
        operations->failure_binding = receiver_binding;
        operations->failure_receipt_binding = receipt_binding;
        operations->failure_receipt_source = source;
        operations->failure_receipt_kind = kind;
        if (receiver_fact == NULL || receipt_fact != receiver_fact
            || !release_call
            || !receipt || kind != PGY_COLLECTION_EFFECT_DROP
            || receipt_binding != receiver_binding || source != 0
            || receiver_fact->disposition
                != PGY_COLLECTION_DISPOSITION_RETIRED)
            return hir_source_failure(operations, part,
                "deferred release receipt");
        if (!hir_pending_retirement_append(
                operations, receiver_binding, false))
            return hir_source_failure(operations, part,
                "duplicate deferred release");
    }
    return true;
}

/* Visit evaluated expressions in an already-owned CFG statement, not a
 * program root. Function/lambda bodies are separate routine authority. */
static bool hir_owned_expression(HIRCollectionOperations *operations, ASTNode *node);
static bool hir_owned_source_definition(HIRCollectionOperations *operations,
    ASTNode *statement, ASTNode *place, ASTNode *value, bool initialization)
{
    if (!operations->source_only) return true;
    size_t ordinal = SIZE_MAX;
    uint32_t binding = initialization ? ast_node_stable_id(statement) :
        place != NULL && place->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(place) : 0;
    if (!semantic_collection_effect_snapshot_source_definition(operations->snapshot,
            operations->routine->source_syntax_id, binding, initialization,
            statement, place, value, &ordinal))
        return hir_source_failure(operations, statement, "source definition");
    if (!operations->track_execution || ordinal == SIZE_MAX) return true;
    if (ordinal >= operations->write_count || operations->write_seen == NULL ||
        operations->write_seen[ordinal]) return false;
    operations->write_seen[ordinal] = true;
    return true;
}
static bool
hir_owned_expression_body(HIRCollectionOperations *operations, ASTNode *node)
{
    if (node == NULL) return true;
    if (operations->source_only && !semantic_collection_effect_snapshot_source_value(
            operations->snapshot, operations->routine->source_syntax_id, node))
        return hir_source_failure(operations, node, "source value");
    if (operations->source_only && operations->track_execution &&
        operations->source_occurrence_count != 0 && node->type == AST_CALL) {
        size_t ordinal = SIZE_MAX;
        if (!semantic_collection_effect_snapshot_source_occurrence(operations->snapshot,
                operations->routine->source_syntax_id, node, operations->execution_root, &ordinal))
            return hir_source_failure(operations, node, "source occurrence");
        if (ordinal != SIZE_MAX) {
            if (ordinal >= operations->source_occurrence_count ||
                operations->source_occurrences == NULL || operations->source_occurrences[ordinal]) return false;
            operations->source_occurrences[ordinal] = true;
        }
    }
    if (!operations->source_only && node->type == AST_DEFER_STMT)
        return hir_owned_deferred_retirements(operations, node);
    if (operations->source_only && node->type == AST_BLOCK) {
        for (size_t s = 0; s < ast_block_statement_count(node); s++)
            if (!hir_owned_expression(operations, ast_block_statement(node, s))) return false;
        return true;
    }
    if (operations->source_only && node->type == AST_DEFER_STMT) {
        ASTNode *body = ast_defer_body(node);
        if (!operations->track_execution) return hir_owned_expression(operations, body);
        if (body == NULL || body->type != AST_BLOCK) return false;
        const ASTNode *previous_root = operations->execution_root;
        for (size_t p = 0; p < ast_block_statement_count(body); p++) {
            ASTNode *part = ast_block_statement(body, p);
            operations->execution_root = part;
            bool valid = part != NULL && hir_owned_expression(operations, part);
            operations->execution_root = previous_root;
            if (!valid) return false;
        }
        return true;
    }
    if (operations->source_only && node->type == AST_IF_STMT)
        return hir_owned_expression(operations, ast_if_condition(node)) &&
            hir_owned_expression(operations, ast_if_then_branch(node)) &&
            hir_owned_expression(operations, ast_if_else_branch(node));
    if (operations->source_only && node->type == AST_WHILE_LOOP)
        return hir_owned_expression(operations, ast_while_condition(node)) &&
            hir_owned_expression(operations, ast_while_body(node));
    if (operations->source_only && node->type == AST_FOR_LOOP)
        return hir_owned_expression(operations, ast_for_range_start(node)) &&
            hir_owned_expression(operations, ast_for_range_end(node)) &&
            hir_owned_expression(operations, ast_for_iterable(node)) &&
            hir_owned_expression(operations, ast_for_body(node));
    if (operations->source_only && node->type == AST_MATCH_STMT) {
        if (!hir_owned_expression(operations, ast_match_subject(node))) return false;
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            if (!hir_owned_expression(operations, ast_match_case_at(node, i))) return false;
        return hir_owned_expression(operations, ast_match_default_body(node));
    }
    if (operations->source_only && node->type == AST_MATCH_CASE) {
        for (size_t i = 0; i < ast_match_case_pattern_count(node); i++)
            if (!hir_owned_expression(operations, ast_match_case_pattern_at(node, i))) return false;
        return hir_owned_expression(operations, ast_match_case_guard(node)) &&
            hir_owned_expression(operations, ast_match_case_body(node));
    }
    if (operations->source_only && node->type == AST_SELECT_STMT) {
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            if (!hir_owned_expression(operations, ast_select_case(node, i))) return false;
        return hir_owned_expression(operations, ast_select_default_case(node));
    }
    if (operations->source_only && node->type == AST_WITH_STMT)
        return hir_owned_expression(operations, ast_with_body(node));
    if (operations->source_only && node->type == AST_UNSAFE_BLOCK)
        return hir_owned_expression(operations, ast_unsafe_block_body(node));
    if (operations->source_only && node->type == AST_TRANSACTION_BLOCK)
        return hir_owned_expression(operations, ast_transaction_block_body(node));
    if (operations->source_only && node->type == AST_ASYNC_BLOCK) {
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++)
            if (!hir_owned_expression(operations, ast_async_block_statement(node, i))) return false;
        return true;
    }
    if (operations->source_only && node->type == AST_PARALLEL_BLOCK) {
        if (!hir_owned_expression(operations, ast_parallel_join_collection(node)) ||
            !hir_owned_expression(operations, ast_parallel_join_range_end(node))) return false;
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            if (!hir_owned_expression(operations, ast_parallel_task(node, i))) return false;
        return true;
    }
    if (operations->source_only && node->type == AST_INTENT_STEP) {
        if (!hir_owned_expression(operations, ast_intent_step_where_type(node)) ||
            !hir_owned_expression(operations, ast_intent_step_using_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_intent_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_pre_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_guard_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_post_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_invariant_expr(node)) ||
            !hir_owned_expression(operations, ast_intent_step_expect_expr(node)))
            return false;
        ASTNode **required = ast_intent_step_required_abilities(node, NULL);
        for (size_t i = 0; i < ast_intent_step_required_ability_count(node); i++)
            if (!hir_owned_expression(operations, required[i])) return false;
        ASTNode **outcomes = ast_intent_step_on_exprs(node, NULL);
        for (size_t i = 0; i < ast_intent_step_on_expr_count(node); i++)
            if (!hir_owned_expression(operations, outcomes[i])) return false;
        ASTNode **compensations = ast_intent_step_compensate_exprs(node, NULL);
        for (size_t i = 0; i < ast_intent_step_compensate_expr_count(node); i++)
            if (!hir_owned_expression(operations, compensations[i])) return false;
        return true;
    }
    if (operations->source_only && node->type == AST_LET_DESTRUCTURE)
        return hir_owned_expression(operations, ast_let_destructure_initializer(node));
    if (operations->source_only && node->type == AST_GIVE_STMT)
        return hir_owned_expression(operations, ast_give_value(node));
    if (operations->source_only && node->type == AST_FAIL_STMT)
        return hir_owned_expression(operations, ast_fail_stmt_reason(node));
    if (operations->source_only && node->type == AST_BIND_STMT)
        return hir_owned_expression(operations, ast_bind_statement_subject(node));
    if (operations->source_only && node->type == AST_SPAWN_EXPR) {
        if (!hir_owned_expression(operations, ast_spawn_function(node))) return false;
        for (size_t i = 0; i < ast_spawn_arg_count(node); i++)
            if (!hir_owned_expression(operations, ast_spawn_argument(node, i))) return false;
        return true;
    }
    if (operations->source_only && (node->type == AST_EVENT_SUBSCRIBE ||
            node->type == AST_EVENT_UNSUBSCRIBE))
        return hir_owned_expression(operations, ast_event_op_event(node)) &&
            hir_owned_expression(operations, ast_event_op_handler(node));
    if (operations->source_only && node->type == AST_EVENT_INVOKE) {
        if (!hir_owned_expression(operations, ast_event_invoke_event(node))) return false;
        for (size_t i = 0; i < ast_event_invoke_arg_count(node); i++)
            if (!hir_owned_expression(operations, ast_event_invoke_argument(node, i))) return false;
        return true;
    }
    if (node->type == AST_LET_DECL) {
        ASTNode *value = ast_let_initializer(node);
        if (!hir_owned_source_definition(operations, node, node, value, true)) return false;
        if (!hir_owned_expression(operations, value)) return false;
        if (operations->snapshot != NULL) return true;
        const PgyCollectionOwnershipFact *fact = hir_owned_local_fact(
            operations->routine, ast_node_stable_id(node));
        if (fact == NULL) {
            /* A direct identifier move cannot silently lose its destination
             * row; the source row already fixes the admitted Array<String>. */
            return value == NULL || value->type != AST_IDENTIFIER
                || hir_owned_local_fact(operations->routine,
                    ast_identifier_binding_syntax_id(value)) == NULL;
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
            if (value == NULL || value->type != AST_IDENTIFIER
                || ast_identifier_binding_syntax_id(value) != fact->source_binding_syntax_id)
                return false;
            return hir_owned_operation_append(operations, (PgyCollectionTransitionOperation){
                PGY_COLLECTION_TRANSITION_MOVE, fact->binding_syntax_id,
                fact->source_binding_syntax_id, ast_node_stable_id(node)});
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL) {
            if (value == NULL || value->type != AST_ARRAY_LITERAL
                || ast_array_literal_count(value) == 0
                || ast_node_stable_id(value) != fact->origin_syntax_id)
                return false;
            return hir_owned_operation_append(operations,
                (PgyCollectionTransitionOperation){
                    PGY_COLLECTION_TRANSITION_BIRTH,
                    fact->binding_syntax_id, 0,
                    ast_node_stable_id(node)});
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_CALL_RESULT) {
            if (value == NULL || value->type != AST_CALL)
                return hir_source_failure(operations, value,
                    "call-result initializer is not a call");
            if (ast_node_stable_id(value) == 0
                || !ast_call_semantic_callee_declared_callable(value))
                return hir_source_failure(operations, value,
                    "call-result initializer has no declared callee");
            if (ast_call_semantic_callee_decl_id(value)
                    != fact->origin_syntax_id
                || ast_call_semantic_callee_value_binding_id(value) != 0)
                return hir_source_failure(operations, value,
                    "call-result callee identity differs from ownership fact");
            return hir_owned_operation_append(operations,
                (PgyCollectionTransitionOperation){
                    PGY_COLLECTION_TRANSITION_BIRTH,
                    fact->binding_syntax_id, 0,
                    ast_node_stable_id(node)});
        }
        const char *type = ast_call_semantic_value_type_name(value);
        const char *producer = fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS ? "MapKeys" : "Clone";
        if (!hir_owned_canonical_call(value, producer, 1) || type == NULL
            || strcmp(type, "Array<String>") != 0 || ast_node_stable_id(value) != fact->origin_syntax_id)
            return false;
        return hir_owned_operation_append(operations, (PgyCollectionTransitionOperation){
            PGY_COLLECTION_TRANSITION_BIRTH, fact->binding_syntax_id, 0, ast_node_stable_id(node)});
    }
    if (node->type == AST_CALL) {
        if (!hir_owned_expression(operations, ast_call_callee(node))) return false;
        size_t count = 0;
        ASTNode **arguments = ast_call_arguments(node, &count);
        for (size_t i = 0; i < count; i++) {
            if (operations->source_only && !semantic_collection_effect_snapshot_source_use(
                    operations->snapshot, operations->routine->source_syntax_id, node, i, arguments[i]))
                return hir_source_failure(operations, arguments[i], "source call actual");
            if (!hir_owned_expression(operations, arguments[i])) return false;
        }
        if (operations->source_only) return true;
        if (operations->snapshot != NULL) {
            size_t effect = SIZE_MAX;
            if (!semantic_collection_effect_snapshot_call(operations->snapshot,
                    operations->caller, node, &effect)) {
                operations->failure_node = node;
                operations->failure_reason = "call membership";
                return false;
            }
            if (effect == SIZE_MAX) return true;
            if (effect >= operations->effect_count) {
                operations->failure_node = node;
                operations->failure_reason = "effect range";
                return false;
            }
            if (operations->effect_seen[effect]) {
                operations->failure_node = node;
                operations->failure_reason = "duplicate effect";
                return false;
            }
            operations->effect_seen[effect] = true;
            return true;
        }
        ASTNode *receiver = ast_call_argument(node, 0);
        uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
            ? ast_identifier_binding_syntax_id(receiver) : 0;
        const PgyCollectionOwnershipFact *fact = hir_owned_local_fact(operations->routine, binding);
        uint32_t kind = 0, receipt_binding = 0, source = 0;
        bool receipt = ast_call_semantic_collection_effect(node, &kind, &receipt_binding, &source);
        operations->failure_binding = binding;
        operations->failure_receipt_binding = receipt_binding;
        operations->failure_receipt_source = source;
        operations->failure_receipt_kind = kind;
        bool drop = hir_owned_canonical_call(
            node, "ArrayDropOwnedStrings", 1);
        bool owned_push_copy = hir_owned_canonical_call(node,
            "ArrayPushOwnedString", 2);
        bool owned_push_transfer = hir_owned_canonical_call(node,
            "ArrayPush", 2) && receipt
            && kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
        bool owned_set = hir_owned_canonical_call(node,
            "ArraySetOwnedString", 3);
        bool owned_pop = hir_owned_canonical_call(node, "ArrayPop", 1)
            && kind == PGY_COLLECTION_EFFECT_OWNED_STRING_POP;
        const PgyCollectionOwnershipFact *receipt_fact = receipt
            ? hir_owned_local_fact(operations->routine, receipt_binding) : NULL;
        if ((fact != NULL
                && (drop || owned_push_copy || owned_push_transfer
                    || owned_set || owned_pop || receipt))
            || receipt_fact != NULL) {
            if (fact == NULL)
                return hir_source_failure(operations, node,
                    "collection receipt has no receiver fact");
            if (!receipt || receipt_binding != binding)
                return hir_source_failure(operations, node,
                    "collection receipt receiver identity differs");
            if (owned_push_copy || owned_push_transfer) {
                ASTNode *value = count == 2 ? arguments[1] : NULL;
                bool transfer_source = value != NULL
                    && value->type == AST_CALL && source != 0
                    && (ast_call_semantic_callee_decl_id(value) == source
                        || ast_node_stable_id(value) == source);
                if (drop || owned_set || owned_pop || count != 2
                    || kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                    || (owned_push_copy && source != 0)
                    || (owned_push_transfer && !transfer_source))
                    return hir_source_failure(operations, node,
                        "owned String push receipt shape differs");
                return hir_owned_operation_append(operations,
                    (PgyCollectionTransitionOperation){
                        PGY_COLLECTION_TRANSITION_OWNED_STRING_PUSH,
                        binding, 0, ast_node_stable_id(node)});
            }
            if (source != 0)
                return hir_source_failure(operations, node,
                    "non-push collection receipt carries a source");
            if (owned_set) {
                if (drop || owned_pop || count != 3
                    || kind != PGY_COLLECTION_EFFECT_OWNED_STRING_SET)
                    return hir_source_failure(operations, node,
                        "owned String set receipt shape differs");
                return hir_owned_operation_append(operations,
                    (PgyCollectionTransitionOperation){
                        PGY_COLLECTION_TRANSITION_OWNED_STRING_SET,
                        binding, 0, ast_node_stable_id(node)});
            }
            if (owned_pop) {
                if (drop || count != 1)
                    return hir_source_failure(operations, node,
                        "owned String pop receipt shape differs");
                return hir_owned_operation_append(operations,
                    (PgyCollectionTransitionOperation){
                        PGY_COLLECTION_TRANSITION_OWNED_STRING_POP,
                        binding, 0, ast_node_stable_id(node)});
            }
            if (!drop || count != 1
                || kind != PGY_COLLECTION_EFFECT_DROP)
                return hir_source_failure(operations, node,
                    "owned collection drop receipt shape differs");
            return hir_owned_operation_append(operations, (PgyCollectionTransitionOperation){
                PGY_COLLECTION_TRANSITION_DROP, binding, 0, ast_node_stable_id(node)});
        }
    } else if (node->type == AST_BINARY) {
        if (!hir_owned_expression(operations, ast_binary_left(node))) return false;
        size_t before = operations->count;
        if (!hir_owned_expression(operations, ast_binary_right(node))) return false;
        Token op = ast_binary_operator(node);
        /* A short-circuit RHS lacks a separate CFG execution witness here. */
        return (op.type != TOKEN_AND && op.type != TOKEN_OR) || before == operations->count;
    } else if (node->type == AST_UNARY) {
        return hir_owned_expression(operations, ast_unary_operand(node));
    } else if (node->type == AST_MEMBER_ACCESS) {
        return hir_owned_expression(operations, ast_member_object(node));
    } else if (node->type == AST_ARRAY_ACCESS) {
        return hir_owned_expression(operations, ast_array_access_array(node))
            && hir_owned_expression(operations, ast_array_access_index(node));
    } else if (node->type == AST_ARRAY_LITERAL || node->type == AST_TUPLE_LITERAL
        || node->type == AST_SET_LITERAL || node->type == AST_MAP_LITERAL) {
        size_t count = node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node)
            : node->type == AST_TUPLE_LITERAL ? ast_tuple_literal_count(node)
            : node->type == AST_SET_LITERAL ? ast_set_literal_count(node) : ast_map_literal_count(node);
        for (size_t i = 0; i < count; i++) {
            ASTNode *value = node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i)
                : node->type == AST_TUPLE_LITERAL ? ast_tuple_literal_element(node, i)
                : node->type == AST_SET_LITERAL ? ast_set_literal_element(node, i) : ast_map_literal_value(node, i);
            if (node->type == AST_MAP_LITERAL
                && !hir_owned_expression(operations, ast_map_literal_key(node, i))) return false;
            if (!hir_owned_expression(operations, value)) return false;
        }
        return true;
    } else if (node->type == AST_CAST || node->type == AST_TYPE_TEST) {
        return hir_owned_expression(operations, node->type == AST_CAST
            ? ast_cast_operand(node) : ast_type_test_operand(node));
    } else if (node->type == AST_AWAIT_EXPR) {
        return hir_owned_expression(operations, ast_await_expression(node));
    } else if (node->type == AST_CHANNEL_SEND) {
        return hir_owned_expression(operations, ast_channel_send_channel(node))
            && hir_owned_expression(operations, ast_channel_send_value(node));
    } else if (node->type == AST_CHANNEL_RECV) {
        return hir_owned_expression(operations, ast_channel_recv_channel(node));
    } else if (node->type == AST_RETURN) {
        if (operations->source_only && !semantic_collection_effect_snapshot_source_return(
                operations->snapshot, operations->routine->source_syntax_id,
                node, ast_return_value(node)))
            return hir_source_failure(operations, node, "source return");
        return hir_owned_expression(operations, ast_return_value(node));
    } else if (node->type == AST_ASSIGNMENT) {
        ASTNode *place = ast_assignment_target(node);
        if (!hir_owned_source_definition(operations, node, place, ast_assignment_value(node), false)) return false;
        return hir_owned_expression(operations, ast_assignment_target(node))
            && hir_owned_expression(operations, ast_assignment_value(node));
    }
    if (operations->source_only) {
        switch (node->type) {
        case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN:
        case AST_TYPE: case AST_BREAK: case AST_CONTINUE: case AST_LAMBDA_EXPR: return true;
        default: return hir_source_failure(operations, node,
            "unsupported source expression");
        }
    }
    return true;
}

static bool hir_owned_expression(HIRCollectionOperations *operations, ASTNode *node)
{
    if (!operations->source_only || node == NULL) return hir_owned_expression_body(operations, node);
    for (const HIRSourceExpressionVisit *p = operations->source_active; p != NULL; p = p->previous)
        if (p->node == node)
            return hir_source_failure(operations, node, "source expression cycle");
    HIRSourceExpressionVisit visit = {node, operations->source_active};
    operations->source_active = &visit;
    bool current = hir_owned_expression_body(operations, node);
    operations->source_active = visit.previous;
    return current;
}

static bool hir_collection_source_expression_current_detail(
    const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, ASTNode *root,
    ASTNode **failure_node, const char **failure_reason)
{
    if (failure_node != NULL) *failure_node = NULL;
    if (failure_reason != NULL) *failure_reason = NULL;
    if (snapshot == NULL || routine == NULL || routine->source_syntax_id == 0 || root == NULL ||
        !semantic_collection_effect_snapshot_source_root(snapshot, routine->source_syntax_id, root)) {
        if (failure_node != NULL) *failure_node = root;
        if (failure_reason != NULL) *failure_reason = "source root";
        return false;
    }
    HIRCollectionOperations operations = {.routine = routine, .snapshot = snapshot, .source_only = true};
    bool current = hir_owned_expression(&operations, root);
    if (!current) {
        if (failure_node != NULL) *failure_node = (ASTNode *)(operations.failure_node != NULL
            ? operations.failure_node : root);
        if (failure_reason != NULL) *failure_reason = operations.failure_reason != NULL
            ? operations.failure_reason : "source expression";
    }
    return current;
}

bool hir_collection_source_expression_current(const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, ASTNode *root)
{
    return hir_collection_source_expression_current_detail(
        snapshot, routine, root, NULL, NULL);
}

bool hir_collection_source_execution_current(const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, ASTNode *root, const ASTNode *execution_root,
    bool *occurrences, size_t count, bool *writes, size_t write_count)
{
    if (snapshot == NULL || routine == NULL || routine->source_syntax_id == 0 || root == NULL ||
        (count != 0 && occurrences == NULL) || (write_count != 0 && writes == NULL) ||
        !semantic_collection_effect_snapshot_source_root(snapshot, routine->source_syntax_id, root)) return false;
    HIRCollectionOperations operations = {.routine = routine, .snapshot = snapshot, .source_only = true,
        .track_execution = true, .source_occurrences = occurrences, .source_occurrence_count = count,
        .execution_root = execution_root, .write_seen = writes, .write_count = write_count};
    return hir_owned_expression(&operations, root);
}

/* Retained control-container ASTs are source inventory. Their body blocks and
 * terminators, not recursive replay of that inventory, own actual evaluation. */
static bool hir_collection_block_execution_current(const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, const HIRBasicBlock *block, bool *occurrences,
    const PgyCollectionEffectView *view, bool *writes)
{
    if (!block->is_reachable) return true;
    for (size_t s = 0; s < block->statement_count; s++) {
        ASTNode *root = block->statements[s];
        if (root->type == AST_FOR_LOOP)
            root = ast_for_iterable(root) != NULL ? ast_for_iterable(root) : ast_for_range_start(root);
        else if (root->type != AST_RETURN && hir_stmt_ast_is_cfg_owned_control(root)) continue;
        if (root != NULL && !hir_collection_source_execution_current(snapshot, routine, root, root,
                occurrences, view->independent_string_result_count, writes, view->string_write_count)) return false;
    }
    ASTNode *condition = block->terminator_condition;
    if (condition != NULL && condition->type == AST_FOR_LOOP)
        condition = ast_for_iterable(condition) == NULL ? ast_for_range_end(condition) : NULL;
    else if (condition != NULL && condition->type == AST_MATCH_CASE)
        condition = ast_match_case_guard(condition);
    if (condition != NULL && !hir_collection_source_execution_current(snapshot, routine, condition, condition,
            occurrences, view->independent_string_result_count, writes, view->string_write_count)) return false;
    ASTNode *value = block->terminator_kind == HIR_BLOCK_RETURN ? NULL : block->terminator_value;
    return value == NULL || hir_collection_source_execution_current(snapshot, routine, value, value,
        occurrences, view->independent_string_result_count, writes, view->string_write_count);
}

/* Replay cached source claims once, then join expected calls to the actual
 * CFG. Source membership alone cannot witness an erased or duplicated call. */
bool hir_collection_effect_snapshot_current(const SemanticResult *semantic,
    const HIRProgram *hir, char **error_message)
{
    PgyCollectionEffectView view = {0};
    const char *stage = "input";
    size_t caller_index = SIZE_MAX, routine_index = SIZE_MAX;
    size_t block_index = SIZE_MAX, slot_index = SIZE_MAX;
    uint32_t routine_syntax_id = 0, caller_syntax_id = 0;
    ASTNode *failure_node = NULL;
    const char *failure_reason = NULL;
    const char *routine_name = NULL;
    bool valid = semantic != NULL && hir != NULL;
    if (valid) {
        stage = "HIR execution census";
        valid = hir_cfg_program_execution_current(hir,
            semantic->annotated_ast);
    }
    if (valid) {
        stage = "source identity";
        valid = semantic->annotated_ast != NULL &&
            hir->source_program_syntax_id ==
                ast_node_stable_id(semantic->annotated_ast) &&
            hir->routine_count <= hir->routine_capacity &&
            (hir->routine_count == 0 || hir->routines != NULL);
    }
    if (valid) {
        stage = "semantic effect snapshot";
        valid = semantic_collection_effect_snapshot_current(
            semantic->collection_effect_snapshot,
            semantic->annotated_ast, &view);
    }
    bool *seen = !valid || view.effect_count == 0 ? NULL : calloc(view.effect_count, sizeof(*seen));
    if (valid && view.effect_count != 0 && seen == NULL) {
        stage = "effect seen allocation";
        valid = false;
    }
    bool *occurrences = !valid || view.independent_string_result_count == 0 ? NULL :
        calloc(view.independent_string_result_count, sizeof(*occurrences));
    bool *writes = !valid || view.string_write_count == 0 ? NULL : calloc(view.string_write_count, sizeof(*writes));
    if (valid && ((view.independent_string_result_count != 0 && occurrences == NULL) ||
            (view.string_write_count != 0 && writes == NULL))) {
        stage = "source execution allocation";
        valid = false;
    }
    for (size_t c = 0; valid && c < view.caller_count; c++) {
        caller_index = c;
        const PgyCollectionEffectCaller *caller = &view.callers[c];
        caller_syntax_id = caller->decl_id;
        const HIRRoutine *actual = NULL;
        for (size_t r = 0; valid && r < hir->routine_count; r++) {
            routine_index = r;
            const HIRRoutine *routine = &hir->routines[r];
            if (routine->source_syntax_id != caller->decl_id) continue;
            stage = "effect caller identity";
            if (actual != NULL || routine->ast != caller->decl || routine->body != caller->body ||
                ast_node_stable_id(routine->ast) != caller->decl_id ||
                ast_node_stable_id(routine->body) != caller->body_id) {
                valid = false; break;
            }
            actual = routine;
        }
        routine_syntax_id = actual != NULL ? actual->source_syntax_id : 0;
        routine_name = actual != NULL ? actual->name : NULL;
        stage = "effect caller CFG";
        if (!valid || actual == NULL || !actual->has_cfg || actual->cfg.block_count == 0 ||
            actual->cfg.block_count > actual->cfg.block_capacity ||
            actual->cfg.blocks == NULL) { valid = false; break; }
        HIRCollectionOperations operations = {.routine = actual,
            .snapshot = semantic->collection_effect_snapshot, .effect_seen = seen,
            .caller = c, .effect_count = view.effect_count};
        for (size_t b = 0; valid && b < actual->cfg.block_count; b++) {
            block_index = b;
            const HIRBasicBlock *block = &actual->cfg.blocks[b];
            stage = "effect block storage";
            if (block->statement_count > block->statement_capacity ||
                (block->statement_count != 0 && block->statements == NULL)) {
                valid = false; break;
            }
            for (size_t s = 0; valid && s < block->statement_count; s++) {
                slot_index = s;
                stage = "effect statement replay";
                failure_node = block->statements[s];
                valid = failure_node != NULL &&
                    hir_owned_expression(&operations, failure_node);
                if (!valid && operations.failure_node != NULL) {
                    failure_node = (ASTNode *)operations.failure_node;
                    failure_reason = operations.failure_reason;
                }
            }
            if (valid) {
                slot_index = SIZE_MAX;
                stage = "effect condition replay";
                failure_node = block->terminator_condition;
                valid = hir_owned_expression(&operations, failure_node);
                if (!valid && operations.failure_node != NULL) {
                    failure_node = (ASTNode *)operations.failure_node;
                    failure_reason = operations.failure_reason;
                }
            }
            if (valid) {
                stage = "effect terminator replay";
                failure_node = block->terminator_value;
                valid = hir_owned_expression(&operations, failure_node);
                if (!valid && operations.failure_node != NULL) {
                    failure_node = (ASTNode *)operations.failure_node;
                    failure_reason = operations.failure_reason;
                }
            }
        }
    }
    for (size_t e = 0; valid && e < view.effect_count; e++) {
        slot_index = e;
        stage = "effect completeness";
        valid = seen[e];
    }
    for (size_t r = 0; valid && (view.source_use_count != 0 || view.nonowner_risk_count != 0 ||
            view.string_definition_count != 0 || view.permanent_element_count != 0 ||
            view.independent_string_result_count != 0) &&
            r < hir->routine_count; r++) {
        routine_index = r;
        caller_index = SIZE_MAX;
        block_index = SIZE_MAX;
        slot_index = SIZE_MAX;
        const HIRRoutine *routine = &hir->routines[r];
        routine_syntax_id = routine->source_syntax_id;
        routine_name = routine->name;
        caller_syntax_id = 0;
        if (!routine->has_cfg) continue;
        for (size_t c = 0; valid && c < view.caller_count; c++) {
            if (view.callers[c].decl_id == routine->source_syntax_id &&
                view.callers[c].independent_string_result_count != 0) {
                caller_index = c;
                caller_syntax_id = view.callers[c].decl_id;
                stage = "source routine execution";
                valid = hir_cfg_execution_current(routine) &&
                    hir_cfg_reachability_current(routine);
            }
        }
        if (!valid) break;
        stage = "source routine CFG";
        if (routine->cfg.block_count > routine->cfg.block_capacity ||
            (routine->cfg.block_count != 0 && routine->cfg.blocks == NULL)) { valid = false; break; }
        for (size_t b = 0; valid && b < routine->cfg.block_count; b++) {
            block_index = b;
            const HIRBasicBlock *block = &routine->cfg.blocks[b];
            stage = "source block storage";
            if (block->statement_count > block->statement_capacity ||
                (block->statement_count != 0 && block->statements == NULL)) { valid = false; break; }
            for (size_t s = 0; valid && s < block->statement_count; s++) {
                slot_index = s;
                ASTNode *statement = block->statements[s];
                stage = "source statement identity";
                if (statement == NULL) { valid = false; break; }
                if (statement->type == AST_RETURN) {
                    stage = "source return placement";
                    if (s + 1 != block->statement_count || block->terminator_kind != HIR_BLOCK_RETURN ||
                        block->terminator_value != ast_return_value(statement)) { valid = false; break; }
                } else {
                    stage = "source statement evaluation";
                    failure_node = statement;
                    if (!semantic_collection_effect_snapshot_source_evaluation(
                            semantic->collection_effect_snapshot,
                            routine->source_syntax_id, statement)) {
                        failure_reason = "source evaluation";
                        valid = false; break;
                    }
                }
                stage = "source statement replay";
                valid = hir_collection_source_expression_current_detail(
                    semantic->collection_effect_snapshot, routine, statement,
                    &failure_node, &failure_reason);
            }
            if (valid && block->terminator_condition != NULL) {
                slot_index = SIZE_MAX;
                stage = "source condition replay";
                failure_node = block->terminator_condition;
                valid = semantic_collection_effect_snapshot_source_evaluation(
                    semantic->collection_effect_snapshot, routine->source_syntax_id,
                    block->terminator_condition) &&
                    hir_collection_source_expression_current_detail(
                        semantic->collection_effect_snapshot, routine,
                        block->terminator_condition, &failure_node,
                        &failure_reason);
            }
            if (valid && block->terminator_value != NULL) {
                slot_index = SIZE_MAX;
                const ASTNode *statement = block->statement_count == 0 ? NULL :
                    block->statements[block->statement_count - 1];
                stage = block->terminator_kind == HIR_BLOCK_RETURN
                    ? "source return replay" : "source terminator replay";
                failure_node = block->terminator_value;
                valid = (block->terminator_kind == HIR_BLOCK_RETURN ?
                    semantic_collection_effect_snapshot_source_return(semantic->collection_effect_snapshot,
                        routine->source_syntax_id,
                        statement != NULL && statement->type == AST_RETURN ? statement : NULL,
                        block->terminator_value) :
                    semantic_collection_effect_snapshot_source_evaluation(
                        semantic->collection_effect_snapshot, routine->source_syntax_id,
                        block->terminator_value)) &&
                    hir_collection_source_expression_current_detail(
                        semantic->collection_effect_snapshot, routine,
                        block->terminator_value, &failure_node,
                        &failure_reason);
            }
            if (valid && view.independent_string_result_count != 0) {
                stage = "source block execution replay";
                valid = hir_collection_block_execution_current(semantic->collection_effect_snapshot,
                    routine, block, occurrences, &view, writes);
            }
        }
        if (valid && view.independent_string_result_count != 0) {
            block_index = SIZE_MAX;
            slot_index = SIZE_MAX;
            stage = "source execution completeness";
            valid = semantic_collection_effect_snapshot_source_execution_complete(
                semantic->collection_effect_snapshot, routine->source_syntax_id,
                occurrences, view.independent_string_result_count, writes, view.string_write_count);
        }
    }
    free(seen);
    free(occurrences);
    free(writes);
    if (!valid && error_message != NULL) {
        const ASTNode *callee = failure_node != NULL
                && failure_node->type == AST_CALL
            ? ast_call_callee(failure_node) : NULL;
        const char *call_name = callee != NULL
                && callee->type == AST_IDENTIFIER
            ? ast_identifier_name(callee) : NULL;
        char detail[512];
        (void)snprintf(detail, sizeof(detail),
            "HIR collection effect snapshot failed at %s"
            " (routine=%zu/%u, caller=%zu/%u, block=%zu, slot=%zu,"
            " node=%u kind=%d line=%u column=%u, routine_name=%s,"
            " call=%s, reason=%s, path=%s)",
            stage, routine_index, (unsigned)routine_syntax_id,
            caller_index, (unsigned)caller_syntax_id,
            block_index, slot_index,
            (unsigned)ast_node_stable_id(failure_node),
            failure_node != NULL ? (int)failure_node->type : -1,
            failure_node != NULL ? failure_node->line : 0,
            failure_node != NULL ? failure_node->column : 0,
            routine_name != NULL ? routine_name : "<none>",
            call_name != NULL ? call_name : "<none>",
            failure_reason != NULL ? failure_reason : "<none>",
            failure_node != NULL && failure_node->origin_path != NULL
                ? failure_node->origin_path : "<none>");
        *error_message = pergyra_strdup(detail);
    }
    return valid;
}

bool
hir_collection_owned_local_transitions_validate(const HIRRoutine *routine, char **error_message)
{
    const char *failure_stage = "allocation";
    size_t failure_block = SIZE_MAX;
    size_t failure_slot = SIZE_MAX;
    size_t binding_count = 0;
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[i]))
            binding_count++;
    if (binding_count == 0) return true;
    PgyCollectionOwnershipFact *bindings = calloc(binding_count, sizeof(*bindings));
    if (bindings == NULL) return false;
    size_t binding = 0;
    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[i]))
            bindings[binding++] = routine->collection_ownership_facts[i];
    HIRCollectionOperations operations = {.routine = routine};
    PgyCollectionTransitionBlock *blocks = calloc(routine->cfg.block_count, sizeof(*blocks));
    size_t predecessor_count = 0;
    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        if (routine->cfg.blocks[i].predecessor_count > SIZE_MAX - predecessor_count) {
            free(blocks); free(bindings); return false;
        }
        predecessor_count += routine->cfg.blocks[i].predecessor_count;
    }
    size_t *predecessors = predecessor_count == 0 ? NULL : calloc(predecessor_count, sizeof(*predecessors));
    bool valid = blocks != NULL && (predecessor_count == 0 || predecessors != NULL);
    size_t offset = 0;
    for (size_t i = 0; valid && i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        blocks[i] = (PgyCollectionTransitionBlock){
            .reachable = block->is_reachable,
            /* SIZE_MAX retirement rows name normal function return edges.
             * Exit/panic/unreachable blocks own no continuing cleanup edge
             * and must not be mistaken for a caller-visible return. */
            .terminal = block->terminator_kind == HIR_BLOCK_RETURN,
            .predecessor_start = offset,
            .predecessor_count = block->predecessor_count,
            .operation_start = operations.count,
            .operation_count = 0,
        };
        for (size_t p = 0; p < block->predecessor_count; p++)
            predecessors[offset++] = block->predecessors[p];
        for (size_t s = 0; valid && s < block->statement_count; s++) {
            failure_stage = "statement";
            failure_block = i;
            failure_slot = s;
            valid = hir_owned_expression(&operations, block->statements[s]);
        }
        if (valid) {
            failure_stage = "terminator-condition";
            failure_block = i;
            failure_slot = SIZE_MAX;
            valid = hir_owned_expression(
                &operations, block->terminator_condition);
        }
        if (valid) {
            failure_stage = "terminator-value";
            valid = hir_owned_expression(&operations, block->terminator_value);
        }
        blocks[i].operation_count = operations.count - blocks[i].operation_start;
    }
    for (size_t i = 0; valid && i < binding_count; i++) {
        if (bindings[i].origin != PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || bindings[i].disposition
                != PGY_COLLECTION_DISPOSITION_RETIRED)
            continue;
        failure_stage = "pending-owned-literal-retirement";
        valid = hir_pending_retirement_append(
            &operations, bindings[i].binding_syntax_id, true);
    }
    PgyCollectionTransitionView view = {
        .function_syntax_id = routine->source_syntax_id,
        .bindings = bindings,
        .binding_count = binding_count,
        .blocks = blocks,
        .block_count = routine->cfg.block_count,
        .predecessors = predecessors,
        .predecessor_count = predecessor_count,
        .operations = operations.rows,
        .operation_count = operations.count,
        .entry_block = routine->cfg.entry_block,
        /* HIR names only exact pending bindings. MIR must replace each one
         * with concrete cleanup/defer retirement rows. */
        .pending_retirement_bindings = operations.pending_retirements,
        .pending_retirement_count = operations.pending_retirement_count
    };
    if (valid) valid = pgy_collection_owned_local_transitions_validate(&view, error_message);
    if (!valid && error_message != NULL && *error_message == NULL) {
        char detail[512];
        (void)snprintf(detail, sizeof(detail),
            "HIR owned local collection transition witness is invalid"
            " (routine=%s/%u stage=%s block=%zu slot=%zu node=%u"
            " line=%u column=%u reason=%s binding=%u receipt_binding=%u"
            " receipt_source=%u receipt_kind=%u)",
            routine->name != NULL ? routine->name : "<none>",
            (unsigned)routine->source_syntax_id,
            failure_stage, failure_block, failure_slot,
            (unsigned)ast_node_stable_id(operations.failure_node),
            operations.failure_node != NULL
                ? operations.failure_node->line : 0,
            operations.failure_node != NULL
                ? operations.failure_node->column : 0,
            operations.failure_reason != NULL
                ? operations.failure_reason : "<none>",
            (unsigned)operations.failure_binding,
            (unsigned)operations.failure_receipt_binding,
            (unsigned)operations.failure_receipt_source,
            (unsigned)operations.failure_receipt_kind);
        *error_message = pergyra_strdup(detail);
    }
    free(operations.rows);
    free(operations.pending_retirements);
    free(predecessors);
    free(blocks);
    free(bindings);
    return valid;
}
