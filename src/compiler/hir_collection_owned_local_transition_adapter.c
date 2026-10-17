#include "hir_collection_owned_local_transition_adapter.h"
#include "collection_owned_local_transition_owner.h"
#include "../common/string_compat.h"
#include "../semantic/builtin_kind.h"
#include "../semantic/semantic.h"
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
hir_owned_canonical_call(const ASTNode *call, const char *name)
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
        && ast_call_arg_count(call) == 1
        && clone_ready
        && strcmp(ast_identifier_name(callee), name) == 0;
}

/* Visit evaluated expressions in an already-owned CFG statement, not a
 * program root. Function/lambda bodies are separate routine authority. */
static bool hir_owned_expression(HIRCollectionOperations *operations, ASTNode *node);
static bool
hir_owned_expression_body(HIRCollectionOperations *operations, ASTNode *node)
{
    if (node == NULL) return true;
    if (operations->source_only && !semantic_collection_effect_snapshot_source_value(
            operations->snapshot, operations->routine->source_syntax_id, node)) return false;
    if (operations->source_only && node->type == AST_BLOCK) {
        for (size_t s = 0; s < ast_block_statement_count(node); s++)
            if (!hir_owned_expression(operations, ast_block_statement(node, s))) return false;
        return true;
    }
    if (operations->source_only && node->type == AST_DEFER_STMT)
        return hir_owned_expression(operations, ast_defer_body(node));
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
    if (operations->source_only && node->type == AST_WITH_STMT)
        return hir_owned_expression(operations, ast_with_body(node));
    if (node->type == AST_LET_DECL) {
        ASTNode *value = ast_let_initializer(node);
        if (operations->source_only && !semantic_collection_effect_snapshot_source_definition(
                operations->snapshot, operations->routine->source_syntax_id,
                ast_node_stable_id(node), true, node, value)) return false;
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
        const char *type = ast_call_semantic_value_type_name(value);
        const char *producer = fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS ? "MapKeys" : "Clone";
        if (!hir_owned_canonical_call(value, producer) || type == NULL
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
                    operations->snapshot, operations->routine->source_syntax_id, node, i, arguments[i])) return false;
            if (!hir_owned_expression(operations, arguments[i])) return false;
        }
        if (operations->source_only) return true;
        if (operations->snapshot != NULL) {
            size_t effect = SIZE_MAX;
            if (!semantic_collection_effect_snapshot_call(operations->snapshot,
                    operations->caller, node, &effect)) return false;
            if (effect == SIZE_MAX) return true;
            if (effect >= operations->effect_count || operations->effect_seen[effect])
                return false;
            operations->effect_seen[effect] = true;
            return true;
        }
        ASTNode *receiver = ast_call_argument(node, 0);
        uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
            ? ast_identifier_binding_syntax_id(receiver) : 0;
        const PgyCollectionOwnershipFact *fact = hir_owned_local_fact(operations->routine, binding);
        uint32_t kind = 0, receipt_binding = 0, source = 0;
        bool receipt = ast_call_semantic_collection_effect(node, &kind, &receipt_binding, &source);
        bool drop = hir_owned_canonical_call(node, "ArrayDropOwnedStrings");
        const PgyCollectionOwnershipFact *receipt_fact = receipt
            ? hir_owned_local_fact(operations->routine, receipt_binding) : NULL;
        if ((fact != NULL && (drop || receipt)) || receipt_fact != NULL) {
            if (fact == NULL) return false;
            if (!drop || count != 1 || !receipt || kind != PGY_COLLECTION_EFFECT_DROP
                || receipt_binding != binding || source != 0) return false;
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
                operations->snapshot, operations->routine->source_syntax_id, ast_return_value(node))) return false;
        return hir_owned_expression(operations, ast_return_value(node));
    } else if (node->type == AST_ASSIGNMENT) {
        ASTNode *place = ast_assignment_target(node);
        if (operations->source_only && !semantic_collection_effect_snapshot_source_definition(
                operations->snapshot, operations->routine->source_syntax_id,
                place != NULL && place->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(place) : 0,
                false, place, ast_assignment_value(node))) return false;
        return hir_owned_expression(operations, ast_assignment_target(node))
            && hir_owned_expression(operations, ast_assignment_value(node));
    }
    if (operations->source_only) {
        switch (node->type) {
        case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN:
        case AST_TYPE: case AST_BREAK: case AST_CONTINUE: return true;
        default: return false;
        }
    }
    return true;
}

static bool hir_owned_expression(HIRCollectionOperations *operations, ASTNode *node)
{
    if (!operations->source_only || node == NULL) return hir_owned_expression_body(operations, node);
    for (const HIRSourceExpressionVisit *p = operations->source_active; p != NULL; p = p->previous)
        if (p->node == node) return false;
    HIRSourceExpressionVisit visit = {node, operations->source_active};
    operations->source_active = &visit;
    bool current = hir_owned_expression_body(operations, node);
    operations->source_active = visit.previous;
    return current;
}

bool hir_collection_source_expression_current(const PgyCollectionEffectSnapshot *snapshot,
    const HIRRoutine *routine, ASTNode *root)
{
    if (snapshot == NULL || routine == NULL || routine->source_syntax_id == 0 || root == NULL ||
        !semantic_collection_effect_snapshot_source_root(snapshot, routine->source_syntax_id, root)) return false;
    HIRCollectionOperations operations = {.routine = routine, .snapshot = snapshot, .source_only = true};
    return hir_owned_expression(&operations, root);
}

/* Replay cached source claims once, then join expected calls to the actual
 * CFG. Source membership alone cannot witness an erased or duplicated call. */
bool hir_collection_effect_snapshot_current(const SemanticResult *semantic,
    const HIRProgram *hir, char **error_message)
{
    PgyCollectionEffectView view = {0};
    bool valid = semantic != NULL && hir != NULL &&
        hir->source_program_syntax_id == ast_node_stable_id(semantic->annotated_ast) &&
        hir->routine_count <= hir->routine_capacity &&
        (hir->routine_count == 0 || hir->routines != NULL) &&
        semantic_collection_effect_snapshot_current(semantic->collection_effect_snapshot,
            semantic->annotated_ast, &view);
    bool *seen = !valid || view.effect_count == 0 ? NULL : calloc(view.effect_count, sizeof(*seen));
    if (valid && view.effect_count != 0 && seen == NULL) valid = false;
    for (size_t c = 0; valid && c < view.caller_count; c++) {
        const PgyCollectionEffectCaller *caller = &view.callers[c];
        const HIRRoutine *actual = NULL;
        for (size_t r = 0; valid && r < hir->routine_count; r++) {
            const HIRRoutine *routine = &hir->routines[r];
            if (routine->source_syntax_id != caller->decl_id) continue;
            if (actual != NULL || routine->ast != caller->decl || routine->body != caller->body ||
                ast_node_stable_id(routine->ast) != caller->decl_id ||
                ast_node_stable_id(routine->body) != caller->body_id) {
                valid = false; break;
            }
            actual = routine;
        }
        if (!valid || actual == NULL || !actual->has_cfg || actual->cfg.block_count == 0 ||
            actual->cfg.block_count > actual->cfg.block_capacity ||
            actual->cfg.blocks == NULL) { valid = false; break; }
        HIRCollectionOperations operations = {.routine = actual,
            .snapshot = semantic->collection_effect_snapshot, .effect_seen = seen,
            .caller = c, .effect_count = view.effect_count};
        for (size_t b = 0; valid && b < actual->cfg.block_count; b++) {
            const HIRBasicBlock *block = &actual->cfg.blocks[b];
            if (block->statement_count > block->statement_capacity ||
                (block->statement_count != 0 && block->statements == NULL)) {
                valid = false; break;
            }
            for (size_t s = 0; valid && s < block->statement_count; s++)
                valid = block->statements[s] != NULL &&
                    hir_owned_expression(&operations, block->statements[s]);
            if (valid) valid = hir_owned_expression(&operations, block->terminator_condition);
            if (valid) valid = hir_owned_expression(&operations, block->terminator_value);
        }
    }
    for (size_t e = 0; valid && e < view.effect_count; e++)
        valid = seen[e];
    for (size_t r = 0; valid && view.source_use_count != 0 && r < hir->routine_count; r++) {
        const HIRRoutine *routine = &hir->routines[r];
        if (!routine->has_cfg) continue;
        if (routine->cfg.block_count > routine->cfg.block_capacity ||
            (routine->cfg.block_count != 0 && routine->cfg.blocks == NULL)) { valid = false; break; }
        for (size_t b = 0; valid && b < routine->cfg.block_count; b++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[b];
            if (block->statement_count > block->statement_capacity ||
                (block->statement_count != 0 && block->statements == NULL)) { valid = false; break; }
            for (size_t s = 0; valid && s < block->statement_count; s++)
                valid = hir_collection_source_expression_current(semantic->collection_effect_snapshot,
                    routine, block->statements[s]);
            if (valid && block->terminator_condition != NULL)
                valid = hir_collection_source_expression_current(semantic->collection_effect_snapshot,
                    routine, block->terminator_condition);
            if (valid && block->terminator_value != NULL)
                valid = (block->terminator_kind != HIR_BLOCK_RETURN ||
                    semantic_collection_effect_snapshot_source_return(semantic->collection_effect_snapshot,
                        routine->source_syntax_id, block->terminator_value)) &&
                    hir_collection_source_expression_current(semantic->collection_effect_snapshot,
                        routine, block->terminator_value);
        }
    }
    free(seen);
    if (!valid && error_message != NULL)
        *error_message = pergyra_strdup("HIR collection effect snapshot is missing, stale or crosswired");
    return valid;
}

bool
hir_collection_owned_local_transitions_validate(const HIRRoutine *routine, char **error_message)
{
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
        blocks[i] = (PgyCollectionTransitionBlock){block->is_reachable,
            !block->has_succ_true && !block->has_succ_false,
            offset, block->predecessor_count, operations.count, 0};
        for (size_t p = 0; p < block->predecessor_count; p++)
            predecessors[offset++] = block->predecessors[p];
        for (size_t s = 0; valid && s < block->statement_count; s++)
            valid = hir_owned_expression(&operations, block->statements[s]);
        if (valid) valid = hir_owned_expression(&operations, block->terminator_condition);
        if (valid) valid = hir_owned_expression(&operations, block->terminator_value);
        blocks[i].operation_count = operations.count - blocks[i].operation_start;
    }
    PgyCollectionTransitionView view = {routine->source_syntax_id,
        bindings, binding_count,
        blocks, routine->cfg.block_count, predecessors, predecessor_count,
        operations.rows, operations.count, routine->cfg.entry_block};
    if (valid) valid = pgy_collection_owned_local_transitions_validate(&view, error_message);
    if (!valid && error_message != NULL && *error_message == NULL)
        *error_message = pergyra_strdup("HIR owned local collection transition witness is invalid");
    free(operations.rows);
    free(predecessors);
    free(blocks);
    free(bindings);
    return valid;
}
