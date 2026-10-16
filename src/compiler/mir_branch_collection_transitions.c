#include "mir_branch_source_facts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "mir_decl_headers.h"
#include "collection_owned_local_transition_owner.h"
#include "../semantic/builtin_kind.h"
#include "mir_json_expression_graph_materialize.h"

#include "mir_branch_source_facts_internal.h"

typedef enum
{
    MIR_COLLECTION_STATE_BOTTOM = 0,
    MIR_COLLECTION_STATE_EMPTY,
    MIR_COLLECTION_STATE_BORROWED,
    MIR_COLLECTION_STATE_OWNED,
    MIR_COLLECTION_STATE_RETIRED,
    MIR_COLLECTION_STATE_CONFLICT
} MIRCollectionTransitionState;

static MIRCollectionTransitionState
mir_collection_state_join(MIRCollectionTransitionState left,
                          MIRCollectionTransitionState right)
{
    if (left == MIR_COLLECTION_STATE_BOTTOM)
        return right;
    if (right == MIR_COLLECTION_STATE_BOTTOM || left == right)
        return left;
    if ((left == MIR_COLLECTION_STATE_EMPTY
         && right == MIR_COLLECTION_STATE_BORROWED)
        || (left == MIR_COLLECTION_STATE_BORROWED
            && right == MIR_COLLECTION_STATE_EMPTY))
        return MIR_COLLECTION_STATE_BORROWED;
    if ((left == MIR_COLLECTION_STATE_EMPTY
         && right == MIR_COLLECTION_STATE_OWNED)
        || (left == MIR_COLLECTION_STATE_OWNED
            && right == MIR_COLLECTION_STATE_EMPTY))
        return MIR_COLLECTION_STATE_OWNED;
    return MIR_COLLECTION_STATE_CONFLICT;
}

static const char *
mir_collection_call_name(const MIRInstruction *inst)
{
    ASTNode *callee;

    if (inst == NULL || inst->expr0 == NULL
        || inst->expr0->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(inst->expr0);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(callee) != 0
        || !ast_call_semantic_callee_is_stdlib(inst->expr0)) {
        return NULL;
    }
    return ast_identifier_name(callee);
}

static bool
mir_collection_receipt_shape_ready(const MIRInstruction *inst,
                                   uint32_t binding_syntax_id)
{
    const char *name;
    ASTNode *receiver;
    uint32_t receiver_id;
    bool expected = false;

    if (inst == NULL)
        return false;
    name = mir_collection_call_name(inst);
    if (name == NULL)
        return !inst->has_collection_ownership_receipt;
    receiver = ast_call_argument(inst->expr0, 0);
    receiver_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    if (receiver_id != binding_syntax_id)
        return !inst->has_collection_ownership_receipt;

    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        if (strcmp(name, "ArrayPushOwnedString") == 0) {
            expected = inst->collection_ownership_source_binding_id == 0;
        } else if (strcmp(name, "ArrayPush") == 0) {
            ASTNode *value = ast_call_argument(inst->expr0, 1);
            expected = value != NULL && value->type == AST_CALL
                && inst->collection_ownership_source_binding_id != 0
                && ast_call_semantic_callee_decl_id(value)
                    == inst->collection_ownership_source_binding_id;
        }
        break;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        expected = strcmp(name, "ArrayPush") == 0
            || strcmp(name, "ArraySet") == 0
            || strcmp(name, "ArrayPop") == 0;
        break;
    case PGY_COLLECTION_EFFECT_DROP:
        expected = strcmp(name, "ArrayDropOwnedStrings") == 0;
        break;
    default:
        expected = false;
        break;
    }
    return inst->has_collection_ownership_receipt && expected
        && inst->collection_ownership_receiver_binding_id
            == binding_syntax_id
        && (inst->collection_ownership_effect_kind
                == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
            || inst->collection_ownership_source_binding_id == 0);
}

static bool
mir_collection_relevant_call_for_binding(const MIRInstruction *inst,
                                         uint32_t binding_syntax_id)
{
    const char *name = mir_collection_call_name(inst);
    ASTNode *receiver;

    if (name == NULL
        || (strcmp(name, "ArrayPushOwnedString") != 0
            && strcmp(name, "ArrayPush") != 0
            && strcmp(name, "ArraySet") != 0
            && strcmp(name, "ArrayPop") != 0
            && strcmp(name, "ArrayDropOwnedStrings") != 0)) {
        return false;
    }
    receiver = ast_call_argument(inst->expr0, 0);
    return receiver != NULL && receiver->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(receiver) == binding_syntax_id;
}

static bool
mir_collection_binding_requires_transition_validation(
    const MIRRoutine *routine,
    uint32_t binding_syntax_id)
{
    if (routine == NULL || binding_syntax_id == 0)
        return false;
    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_row = 0;
             inst_row < block->instruction_count;
             inst_row++) {
            const MIRInstruction *inst = &block->instructions[inst_row];
            if (mir_collection_relevant_call_for_binding(
                    inst, binding_syntax_id))
                return true;
            if (inst->has_collection_ownership_receipt
                && inst->collection_ownership_receiver_binding_id
                    == binding_syntax_id
                && (inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_DROP
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                    || inst->collection_ownership_effect_kind
                        == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION)) {
                return true;
            }
        }
    }
    return false;
}

static bool
mir_collection_transition_apply(const MIRInstruction *inst,
                                MIRCollectionTransitionState *state)
{
    if (inst == NULL || state == NULL
        || !inst->has_collection_ownership_receipt)
        return false;
    switch ((PgyCollectionOwnershipEffectKind)
                inst->collection_ownership_effect_kind) {
    case PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_OWNED)
            return false;
        *state = MIR_COLLECTION_STATE_OWNED;
        return true;
    case PGY_COLLECTION_EFFECT_SHALLOW_MUTATION:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_BORROWED)
            return false;
        *state = MIR_COLLECTION_STATE_BORROWED;
        return true;
    case PGY_COLLECTION_EFFECT_DROP:
        if (*state != MIR_COLLECTION_STATE_EMPTY
            && *state != MIR_COLLECTION_STATE_OWNED)
            return false;
        *state = MIR_COLLECTION_STATE_RETIRED;
        return true;
    default:
        return false;
    }
}

static bool
mir_collection_transition_error(const MIRRoutine *routine,
                                char **error_message,
                                uint32_t binding_syntax_id,
                                const char *stage)
{
    if (error_message != NULL) {
        const MIRSourceLocalType *local = mir_routine_source_local_type_fact_by_binding_syntax_id(
            routine, binding_syntax_id);
        char detail[512];
        snprintf(detail, sizeof(detail),
            "MIR collection ownership transition is invalid "
            "(routine=%s binding=%u local=%s stage=%s)",
            routine != NULL && routine->name != NULL
                ? routine->name : "<unknown>",
            binding_syntax_id,
            local != NULL && local->name != NULL
                ? local->name : "<unknown>",
            stage != NULL ? stage : "unknown");
        *error_message = pergyra_strdup(detail);
    }
    return false;
}

static bool
mir_collection_owned_birth_ready(const MIRRoutine *routine, const MIRInstruction *inst,
                                const MIRCollectionOwnershipFact *fact)
{
    ASTNode *call = inst != NULL ? inst->expr0 : NULL;
    const MIRSourceLocalType *target = fact != NULL
        ? mir_routine_source_local_type_fact_by_binding_syntax_id(routine, fact->binding_syntax_id) : NULL;
    if (inst == NULL || fact == NULL || inst->kind != MIR_INST_DEF
        || inst->source_stable_id != fact->binding_syntax_id
        || inst->binding_syntax_id != fact->binding_syntax_id
        || call == NULL || target == NULL
        || target->initializer_call_syntax_id != fact->origin_syntax_id
        || ast_node_stable_id(call) != target->initializer_call_syntax_id
        || target->initializer_call_value_type_name == NULL
        || strcmp(target->initializer_call_value_type_name, "Array<String>") != 0)
        return false;
    uint32_t producer = fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS ? 1 : 2;
    if (target->initializer_collection_producer != producer) return false;
    MIRJsonExpressionGraph graph = {0};
    int root = mir_json_expression_graph_build_for_routine(&graph, call, routine);
    bool ready = false;
    if (root >= 0 && (size_t)root < graph.count
        && strcmp(graph.nodes[root].kind, "call_argument") == 0) {
        int call_row = graph.nodes[root].left;
        if (call_row >= 0 && (size_t)call_row < graph.count) {
            const MIRJsonExpressionGraphNode *node = &graph.nodes[call_row];
            int callee_row = node->left;
            while (callee_row >= 0 && (size_t)callee_row < graph.count
                && strcmp(graph.nodes[callee_row].kind, "generic_callee") == 0)
                callee_row = graph.nodes[callee_row].left;
            ready = strcmp(node->kind, "call") == 0
                && strcmp(node->call_target_kind, "direct") == 0
                && node->call_target_syntax_id == 0
                && strcmp(node->call_target_name, producer == 1 ? "MapKeys" : "Clone") == 0
                && callee_row >= 0 && (size_t)callee_row < graph.count
                && graph.nodes[callee_row].binding_syntax_id == 0
                && strcmp(graph.nodes[callee_row].binding_kind, "none") == 0;
        }
    }
    mir_json_expression_graph_dispose(&graph);
    return ready;
}

/* The existing wire has one receipt per statement. Never hide a nested
 * release from that statement's receipt authority. Unsupported nested owned
 * releases refuse here rather than acquire a receipt-free permission. */
static bool
mir_collection_owned_nested_calls_ready(const MIRRoutine *routine,
                                       ASTNode *expression)
{
    if (expression == NULL) return true;
    MIRJsonExpressionGraph graph = {0};
    int root = mir_json_expression_graph_build_for_routine(&graph, expression, routine);
    bool ready = root >= 0;
    for (size_t call = 0; ready && call < graph.count; call++) {
        const MIRJsonExpressionGraphNode *node = &graph.nodes[call];
        if (strcmp(node->kind, "call") != 0
            || strcmp(node->call_target_kind, "direct") != 0
            || node->call_target_syntax_id != 0
            || strcmp(node->call_target_name, "ArrayDropOwnedStrings") != 0)
            continue;
        for (size_t argument = 0; ready && argument < graph.count; argument++) {
            if (strcmp(graph.nodes[argument].kind, "call_argument") != 0
                || graph.nodes[argument].left != (int)call)
                continue;
            int receiver = graph.nodes[argument].right;
            if (receiver < 0 || (size_t)receiver >= graph.count) { ready = false; break; }
            const MIRJsonExpressionGraphNode *value = &graph.nodes[receiver];
            if (strcmp(value->kind, "identifier") != 0) continue;
            for (size_t row = 0; row < routine->source_local_type_count; row++) {
                const MIRSourceLocalType *local = &routine->source_local_types[row];
                if (local->name != NULL && strcmp(local->name, value->text) == 0
                    && pgy_collection_owned_local_fact_tracked(
                        mir_routine_collection_ownership_fact(routine, local->binding_syntax_id))
                    && (root < 0 || graph.nodes[root].left != (int)call)) {
                    /* A scalar-expression statement has one wire receipt;
                     * a nested owned release cannot borrow its parent's. */
                    ready = false;
                }
            }
        }
    }
    mir_json_expression_graph_dispose(&graph);
    return ready;
}

static bool
mir_collection_validate_owned_local_transitions(
    const MIRRoutine *routine, char **error_message)
{
    size_t binding_count = 0, instruction_count = 0, predecessor_count = 0;
    PgyCollectionOwnershipFact *bindings = NULL;
    PgyCollectionTransitionBlock *blocks = NULL;
    PgyCollectionTransitionOperation *operations = NULL;
    size_t *predecessors = NULL;
    bool valid = false;
    const char *failure_stage = "owned-operation-admission";
    uint32_t failure_binding = 0;
    for (size_t row = 0; row < routine->collection_ownership_fact_count; row++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[row]))
            binding_count++;
    if (binding_count == 0) return true;
    if (routine->block_count == 0 || routine->blocks == NULL
        || routine->entry_block >= routine->block_count)
        return mir_collection_transition_error(routine, error_message, 0, "missing-owned-cfg");
    for (size_t b = 0; b < routine->block_count; b++) {
        if (routine->blocks[b].instruction_count > SIZE_MAX - instruction_count
            || routine->blocks[b].predecessor_count > SIZE_MAX - predecessor_count)
            goto done;
        instruction_count += routine->blocks[b].instruction_count;
        predecessor_count += routine->blocks[b].predecessor_count;
    }
    bindings = calloc(binding_count, sizeof(*bindings));
    blocks = calloc(routine->block_count, sizeof(*blocks));
    operations = calloc(instruction_count == 0 ? 1 : instruction_count, sizeof(*operations));
    predecessors = calloc(predecessor_count == 0 ? 1 : predecessor_count, sizeof(*predecessors));
    if (bindings == NULL || blocks == NULL || operations == NULL || predecessors == NULL)
        goto done;
    size_t next_binding = 0, next_operation = 0, next_predecessor = 0;
    for (size_t row = 0; row < routine->collection_ownership_fact_count; row++)
        if (pgy_collection_owned_local_fact_tracked(&routine->collection_ownership_facts[row]))
            bindings[next_binding++] = routine->collection_ownership_facts[row];
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        blocks[b].reachable = block->is_reachable;
        blocks[b].terminal = !block->has_succ_true && !block->has_succ_false
            && !block->has_cleanup_succ && !block->has_rollback_succ
            && !block->has_invalidation_succ;
        blocks[b].predecessor_start = next_predecessor;
        blocks[b].predecessor_count = block->predecessor_count;
        if ((block->instruction_count != 0 && block->instructions == NULL)
            || (block->predecessor_count != 0 && block->predecessors == NULL))
            goto done;
        for (size_t p = 0; p < block->predecessor_count; p++)
            predecessors[next_predecessor++] = block->predecessors[p];
        blocks[b].operation_start = next_operation;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            /* A local declaration's expr1 is its captured type metadata,
             * not an evaluated operand. Assignment DEF targets remain
             * evaluated expressions and must retain the nested-call check. */
            failure_stage = "owned-evaluated-expression";
            failure_binding = inst->binding_syntax_id;
            if (!mir_collection_owned_nested_calls_ready(routine, inst->expr0)
                || (!(inst->kind == MIR_INST_DEF
                      && inst->requires_source_local_decl_emit)
                    && !mir_collection_owned_nested_calls_ready(routine, inst->expr1)))
                goto done;
            const MIRCollectionOwnershipFact *fact =
                mir_routine_collection_ownership_fact(routine, inst->binding_syntax_id);
            PgyCollectionTransitionOperation op = {0};
            if (pgy_collection_owned_local_fact_tracked(fact)) {
                failure_stage = "owned-definition-identity";
                const char *type = mir_collection_binding_type_name(routine, fact->binding_syntax_id);
                if (inst->kind != MIR_INST_DEF || inst->source_stable_id != fact->binding_syntax_id
                    || type == NULL || strcmp(type, "Array<String>") != 0)
                    goto done;
                op.binding_syntax_id = fact->binding_syntax_id;
                op.syntax_id = fact->binding_syntax_id;
                if (fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
                    failure_stage = "owned-move-source";
                    if (!mir_collection_binding_move_source_value_matches(
                            routine, inst, fact->source_binding_syntax_id))
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_MOVE;
                    op.source_binding_syntax_id = fact->source_binding_syntax_id;
                } else {
                    failure_stage = "owned-canonical-birth";
                    if (!mir_collection_owned_birth_ready(routine, inst, fact)) goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_BIRTH;
                }
            } else {
                const char *name = mir_collection_call_name(inst);
                ASTNode *receiver = name != NULL ? ast_call_argument(inst->expr0, 0) : NULL;
                uint32_t receiver_id = receiver != NULL && receiver->type == AST_IDENTIFIER
                    ? ast_identifier_binding_syntax_id(receiver) : 0;
                fact = mir_routine_collection_ownership_fact(routine, receiver_id);
                if (pgy_collection_owned_local_fact_tracked(fact)
                    && name != NULL && strcmp(name, "ArrayDropOwnedStrings") == 0) {
                    failure_stage = "owned-drop-receipt";
                    failure_binding = receiver_id;
                    uint32_t effect = 0, issued_receiver = 0, issued_source = 0;
                    if (inst->kind != MIR_INST_STMT || ast_call_arg_count(inst->expr0) != 1
                        || ast_call_semantic_callee_decl_id(inst->expr0) != 0
                        || ast_call_semantic_callee_value_binding_id(inst->expr0) != 0
                        || !ast_call_semantic_collection_effect(inst->expr0,
                            &effect, &issued_receiver, &issued_source)
                        || effect != PGY_COLLECTION_EFFECT_DROP
                        || issued_receiver != receiver_id || issued_source != 0
                        || !mir_collection_receipt_shape_ready(inst, receiver_id)
                        || inst->collection_ownership_effect_kind != PGY_COLLECTION_EFFECT_DROP)
                        goto done;
                    op.kind = PGY_COLLECTION_TRANSITION_DROP;
                    op.binding_syntax_id = receiver_id;
                    op.syntax_id = ast_node_stable_id(inst->expr0);
                } else if (inst->has_collection_ownership_receipt
                    && pgy_collection_owned_local_fact_tracked(
                        mir_routine_collection_ownership_fact(routine,
                            inst->collection_ownership_receiver_binding_id)))
                {
                    failure_stage = "owned-orphan-receipt";
                    goto done;
                }
            }
            if (op.kind != 0) operations[next_operation++] = op;
        }
        blocks[b].operation_count = next_operation - blocks[b].operation_start;
    }
    PgyCollectionTransitionView view = {
        routine->source_syntax_id, bindings, binding_count, blocks,
        routine->block_count, predecessors, predecessor_count,
        operations, next_operation, routine->entry_block
    };
    valid = pgy_collection_owned_local_transitions_validate(&view, error_message);
done:
    free(bindings); free(blocks); free(operations); free(predecessors);
    if (!valid && error_message != NULL && *error_message == NULL)
        return mir_collection_transition_error(routine, error_message, failure_binding, failure_stage);
    return valid;
}

bool
mir_validate_collection_ownership_transitions(const MIRRoutine *routine,
                                              char **error_message)
{
    if (routine == NULL)
        return false;
    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_row = 0;
             inst_row < block->instruction_count;
             inst_row++) {
            const MIRInstruction *inst = &block->instructions[inst_row];
            const MIRCollectionOwnershipFact *fact;

            if (!inst->has_collection_ownership_receipt)
                continue;
            fact = mir_routine_collection_ownership_fact(
                routine, inst->collection_ownership_receiver_binding_id);
            if (fact == NULL
                || (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
                    && !pgy_collection_owned_local_fact_tracked(fact))
                || !mir_collection_receipt_shape_ready(
                    inst, fact->binding_syntax_id)) {
                return mir_collection_transition_error(
                    routine, error_message,
                    inst->collection_ownership_receiver_binding_id,
                    "orphan-or-forged-receipt");
            }
        }
    }
    if (!mir_collection_validate_owned_local_transitions(routine, error_message))
        return false;
    for (size_t fact_row = 0;
         fact_row < routine->collection_ownership_fact_count;
         fact_row++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[fact_row];
        MIRCollectionTransitionState *inputs;
        MIRCollectionTransitionState *outputs;
        size_t iteration_limit;
        bool changed = true;

        if (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL)
            continue;
        /* The operation, not the optional receipt, decides whether the state
         * machine is required. Otherwise deleting every receipt would make
         * its own absence invisible. */
        if (!mir_collection_binding_requires_transition_validation(
                routine, fact->binding_syntax_id))
            continue;
        if (routine->block_count == 0 || routine->blocks == NULL
            || routine->entry_block >= routine->block_count) {
            return mir_collection_transition_error(
                routine, error_message, fact->binding_syntax_id,
                "missing-cfg");
        }
        inputs = calloc(routine->block_count, sizeof(*inputs));
        outputs = calloc(routine->block_count, sizeof(*outputs));
        if (inputs == NULL || outputs == NULL) {
            free(inputs);
            free(outputs);
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
        iteration_limit = routine->block_count * 8 + 1;
        for (size_t iteration = 0;
             changed && iteration < iteration_limit;
             iteration++) {
            changed = false;
            for (size_t block_id = 0;
                 block_id < routine->block_count;
                 block_id++) {
                const MIRBasicBlock *block = &routine->blocks[block_id];
                MIRCollectionTransitionState joined =
                    block_id == routine->entry_block
                        ? MIR_COLLECTION_STATE_EMPTY
                        : MIR_COLLECTION_STATE_BOTTOM;
                MIRCollectionTransitionState state;

                if (!block->is_reachable)
                    continue;
                for (size_t pred_row = 0;
                     pred_row < block->predecessor_count;
                     pred_row++) {
                    size_t predecessor = block->predecessors[pred_row];
                    if (predecessor >= routine->block_count) {
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            "predecessor");
                    }
                    if (routine->blocks[predecessor].is_reachable) {
                        joined = mir_collection_state_join(
                            joined, outputs[predecessor]);
                    }
                }
                if (joined == MIR_COLLECTION_STATE_BOTTOM)
                    continue;
                if (joined == MIR_COLLECTION_STATE_CONFLICT) {
                    free(inputs);
                    free(outputs);
                    return mir_collection_transition_error(
                        routine, error_message, fact->binding_syntax_id,
                        "branch-join");
                }
                state = joined;
                for (size_t inst_row = 0;
                     inst_row < block->instruction_count;
                     inst_row++) {
                    const MIRInstruction *inst =
                        &block->instructions[inst_row];
                    bool relevant = mir_collection_relevant_call_for_binding(
                        inst, fact->binding_syntax_id);
                    if (inst->has_collection_ownership_receipt
                        && inst->collection_ownership_receiver_binding_id
                            != fact->binding_syntax_id)
                        continue;
                    if (!relevant
                        && !inst->has_collection_ownership_receipt)
                        continue;
                    if (!mir_collection_receipt_shape_ready(
                            inst, fact->binding_syntax_id)) {
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            relevant ? "missing-or-forged-receipt"
                                     : "cross-binding-receipt");
                    }
                    if (!mir_collection_transition_apply(inst, &state)) {
                        free(inputs);
                        free(outputs);
                        return mir_collection_transition_error(
                            routine, error_message, fact->binding_syntax_id,
                            "invalid-state-transition");
                    }
                }
                if (inputs[block_id] != joined
                    || outputs[block_id] != state) {
                    inputs[block_id] = joined;
                    outputs[block_id] = state;
                    changed = true;
                }
            }
        }
        if (changed) {
            free(inputs);
            free(outputs);
            return mir_collection_transition_error(
                routine, error_message, fact->binding_syntax_id,
                "non-convergent-cfg");
        }
        for (size_t block_id = 0;
             block_id < routine->block_count;
             block_id++) {
            if (routine->blocks[block_id].is_reachable
                && outputs[block_id] == MIR_COLLECTION_STATE_BOTTOM) {
                free(inputs);
                free(outputs);
                return mir_collection_transition_error(
                    routine, error_message, fact->binding_syntax_id,
                    "unresolved-reachable-block");
            }
        }
        free(inputs);
        free(outputs);
    }
    return true;
}
