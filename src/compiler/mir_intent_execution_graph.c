#include "mir_intent_execution_graph.h"

#include "mir_base_helpers.h"
#include "mir_json_expression_graph.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static bool
intent_execution_append_predecessor(MIRBasicBlock *block, size_t predecessor)
{
    size_t capacity;
    size_t *grown;

    if (block == NULL || mir_block_has_predecessor(block, predecessor))
        return block != NULL;
    if (block->predecessor_count == block->predecessor_capacity) {
        capacity = block->predecessor_capacity == 0
            ? 2 : block->predecessor_capacity * 2;
        if (capacity < block->predecessor_capacity
            || capacity > SIZE_MAX / sizeof(*grown)) {
            return false;
        }
        grown = realloc(block->predecessors, capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        block->predecessors = grown;
        block->predecessor_capacity = capacity;
    }
    block->predecessors[block->predecessor_count++] = predecessor;
    return true;
}

bool
intent_execution_set_goto(MIRRoutine *routine, size_t from, size_t to)
{
    MIRBasicBlock *source;

    if (routine == NULL || from >= routine->block_count
        || to >= routine->block_count || from == to) {
        return false;
    }
    source = &routine->blocks[from];
    source->succ_true = to;
    source->has_succ_true = true;
    source->has_succ_false = false;
    return intent_execution_append_predecessor(&routine->blocks[to], from);
}

bool
intent_execution_set_branch(MIRRoutine *routine,
                            size_t from,
                            size_t success,
                            size_t failure)
{
    MIRBasicBlock *source;

    if (routine == NULL || from >= routine->block_count
        || success >= routine->block_count || failure >= routine->block_count
        || success == failure) {
        return false;
    }
    source = &routine->blocks[from];
    source->succ_true = success;
    source->has_succ_true = true;
    source->succ_false = failure;
    source->has_succ_false = true;
    return intent_execution_append_predecessor(
               &routine->blocks[success], from)
        && intent_execution_append_predecessor(
               &routine->blocks[failure], from);
}

bool
intent_execution_append_block(MIRRoutine *routine,
                              bool reachable,
                              size_t *block_id_out)
{
    MIRBasicBlock block;

    if (routine == NULL || block_id_out == NULL)
        return false;
    memset(&block, 0, sizeof(block));
    block.id = routine->block_count;
    block.is_reachable = reachable;
    block.is_intent_execution_plan_block = true;
    block.source_hir_block_id = SIZE_MAX;
    *block_id_out = block.id;
    return append_block(routine, block);
}

static bool
intent_execution_instruction_matches_step_phase(
    const MIRInstruction *inst,
    const DIRIntentStep *step,
    const char *phase)
{
    return inst != NULL && step != NULL && phase != NULL
        && step->syntax_id != 0
        && mir_instruction_is_intent_stmt(inst, "IntentEval")
        && inst->has_source_statement_stable_id
        && inst->source_statement_stable_id == step->syntax_id
        && inst->arg0 != NULL && strcmp(inst->arg0, phase) == 0
        && inst->arg1 != NULL && step->name != NULL
        && strcmp(inst->arg1, step->name) == 0;
}

static bool
intent_execution_instruction_matches_step_carrier(
    const MIRInstruction *inst,
    const DIRIntentStep *step,
    const char *carrier_name)
{
    return inst != NULL && step != NULL && carrier_name != NULL
        && step->syntax_id != 0 && step->name != NULL
        && mir_instruction_is_intent_stmt(inst, carrier_name)
        && mir_instruction_source_is_intent_step(inst)
        && ast_node_stable_id(inst->ast) == step->syntax_id
        && ((strcmp(carrier_name, "IntentStep") == 0
                && inst->arg0 != NULL
                && strcmp(inst->arg0, step->name) == 0)
            || (strcmp(carrier_name, "IntentOutcomeBinding") == 0
                && inst->arg1 != NULL
                && strcmp(inst->arg1, step->name) == 0));
}

static size_t
intent_execution_step_carrier_match_count(const MIRRoutine *routine,
                                          const DIRIntentStep *step,
                                          const char *carrier_name)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (intent_execution_instruction_matches_step_carrier(
                    &block->instructions[i], step, carrier_name)) {
                count++;
            }
        }
    }
    return count;
}

static bool
intent_execution_take_step_carrier(MIRRoutine *routine,
                                   const DIRIntentStep *step,
                                   const char *carrier_name,
                                   MIRInstruction *instruction_out)
{
    if (routine == NULL || instruction_out == NULL)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (!intent_execution_instruction_matches_step_carrier(
                    &block->instructions[i], step, carrier_name)) {
                continue;
            }
            *instruction_out = block->instructions[i];
            if (i + 1 < block->instruction_count) {
                memmove(&block->instructions[i], &block->instructions[i + 1],
                    (block->instruction_count - i - 1)
                        * sizeof(*block->instructions));
            }
            block->instruction_count--;
            return true;
        }
    }
    return false;
}

static size_t
intent_execution_instruction_match_count(const MIRRoutine *routine,
                                         const DIRIntentStep *step,
                                         const char *phase)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (intent_execution_instruction_matches_step_phase(
                    &block->instructions[i], step, phase)) {
                count++;
            }
        }
    }
    return count;
}

static bool
intent_execution_take_instruction(MIRRoutine *routine,
                                  const DIRIntentStep *step,
                                  const char *phase,
                                  MIRInstruction *instruction_out)
{
    if (routine == NULL || instruction_out == NULL)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (!intent_execution_instruction_matches_step_phase(
                    &block->instructions[i], step, phase)) {
                continue;
            }
            *instruction_out = block->instructions[i];
            if (i + 1 < block->instruction_count) {
                memmove(&block->instructions[i], &block->instructions[i + 1],
                    (block->instruction_count - i - 1)
                        * sizeof(*block->instructions));
            }
            block->instruction_count--;
            return true;
        }
    }
    return false;
}

static const char *
intent_execution_call_target_name(ASTNode *expression)
{
    ASTNode *callee;

    if (expression == NULL || expression->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(expression);
    if (callee == NULL)
        return NULL;
    if (callee->type == AST_IDENTIFIER)
        return ast_identifier_name(callee);
    if (callee->type == AST_MEMBER_ACCESS)
        return ast_member_name(callee);
    return NULL;
}

static bool
intent_execution_append_branch_instruction(
    MIRRoutine *routine,
    MIRBasicBlock *block,
    const DIRIntentStep *step,
    ASTNode *outcome_expression,
    size_t *instruction_id_out)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_BRANCH;
    inst.name = "IntentOutcomeBranch";
    inst.slot_anchor = step->outcome_binding_name;
    inst.arg0 = step->success_branch.variant_name;
    inst.arg1 = step->failure_branch.variant_name;
    inst.expr0 = outcome_expression;
    inst.ast = outcome_expression;
    inst.branch_shape = MIR_BRANCH_EXPR;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_kind = HIR_BLOCK_BRANCH;
    inst.source_terminator_has_value = true;
    mir_instruction_capture_source_provenance(&inst, outcome_expression);
    if (!mir_commit_instruction(routine, block, &inst))
        return false;
    *instruction_id_out = inst.id;
    return true;
}

static bool
intent_execution_append_payload_definition(
    MIRRoutine *routine,
    MIRBasicBlock *block,
    const DIRIntentOutcomeBranch *branch,
    const char *instruction_name)
{
    MIRInstruction inst;

    if (branch == NULL || branch->payload_name == NULL
        || branch->payload_type_name == NULL) {
        return false;
    }
    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_DEF;
    inst.name = instruction_name;
    inst.slot_anchor = branch->payload_name;
    inst.arg0 = branch->variant_name;
    inst.arg1 = branch->enum_type_name;
    inst.result_name = pergyra_strdup(branch->payload_name);
    inst.abi_type_name = pgy_arena_strdup(
        &routine->scratch, branch->payload_type_name);
    if (inst.result_name == NULL || inst.abi_type_name == NULL) {
        free((void *)inst.result_name);
        return false;
    }
    if (!mir_commit_instruction(routine, block, &inst)) {
        free((void *)inst.result_name);
        return false;
    }
    return true;
}

static bool
intent_execution_append_completion(MIRRoutine *routine,
                                   MIRBasicBlock *block,
                                   const DIRIntentStep *step,
                                   size_t *instruction_id_out)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_STMT;
    inst.name = "IntentStepCompleted";
    inst.slot_anchor = step->name;
    inst.arg0 = step->success_branch.variant_name;
    inst.arg1 = step->name;
    if (!mir_commit_instruction(routine, block, &inst))
        return false;
    *instruction_id_out = inst.id;
    return true;
}

static bool
intent_execution_capture_compensations(MIRRoutine *routine,
                                      const DIRIntentStep *step,
                                      size_t block_id,
                                      MIRIntentStepTransitionFact *row)
{
    size_t count = intent_execution_instruction_match_count(
        routine, step, "compensate");

    if (count == 0)
        return true;
    row->compensations = calloc(count, sizeof(*row->compensations));
    if (row->compensations == NULL)
        return false;
    row->compensation_count = count;
    for (size_t i = 0; i < count; i++) {
        MIRInstruction inst;
        MIRIntentCompensationFact *fact = &row->compensations[i];

        if (!intent_execution_take_instruction(
                routine, step, "compensate", &inst)
            || inst.expr0 == NULL || inst.expr0->type != AST_CALL
            || ast_node_stable_id(inst.expr0) == 0
            || ast_call_semantic_callee_decl_id(inst.expr0) == 0
            || intent_execution_call_target_name(inst.expr0) == NULL
            || !mir_expression_graph_identity(inst.expr0,
                &fact->graph_root_id, &fact->graph_digest)
            || !append_instruction(&routine->blocks[block_id], inst)) {
            return false;
        }
        fact->transition_id = row->transition_id;
        fact->expression_syntax_id = ast_node_stable_id(inst.expr0);
        fact->instruction_block_id = block_id;
        fact->instruction_id = inst.id;
        fact->call_target_name = intent_execution_call_target_name(inst.expr0);
        fact->call_target_syntax_id =
            ast_call_semantic_callee_decl_id(inst.expr0);
        fact->expression = inst.expr0;
    }
    return true;
}

bool
intent_execution_materialize_step(MIRRoutine *routine,
                                  const DIRIntentStep *step,
                                  MIRIntentStepTransitionFact *row,
                                  size_t compensation_block_id)
{
    MIRInstruction step_carrier;
    MIRInstruction outcome_carrier;
    MIRInstruction outcome;
    MIRBasicBlock *branch_block;
    MIRBasicBlock *success_block;
    MIRBasicBlock *failure_block;

    if (intent_execution_step_carrier_match_count(
            routine, step, "IntentStep") != 1
        || intent_execution_step_carrier_match_count(
            routine, step, "IntentOutcomeBinding") != 1
        || intent_execution_instruction_match_count(
            routine, step, "on") != 1
        || !intent_execution_take_step_carrier(
            routine, step, "IntentStep", &step_carrier)
        || !intent_execution_take_step_carrier(
            routine, step, "IntentOutcomeBinding", &outcome_carrier)
        || !intent_execution_take_instruction(routine, step, "on", &outcome)
        || outcome.expr0 == NULL || outcome.expr0->type != AST_CALL
        || ast_call_semantic_callee_decl_id(outcome.expr0)
            != step->outcome_action_decl_syntax_id
        || outcome.result_name == NULL
        || outcome.abi_type_name == NULL
        || strcmp(outcome.result_name, step->outcome_binding_name) != 0
        || strcmp(outcome.abi_type_name, step->outcome_binding_type_name) != 0) {
        return false;
    }
    branch_block = &routine->blocks[row->branch_block_id];
    if (!append_instruction(branch_block, step_carrier)
        || !append_instruction(branch_block, outcome_carrier)
        || !append_instruction(branch_block, outcome)) {
        return false;
    }
    row->outcome_instruction_block_id = row->branch_block_id;
    row->outcome_instruction_id = outcome.id;
    row->outcome_expression = outcome.expr0;

    if (!intent_execution_append_branch_instruction(
            routine, branch_block, step, outcome.expr0,
            &row->branch_instruction_id)) {
        return false;
    }
    success_block = &routine->blocks[row->success.successor_block_id];
    failure_block = &routine->blocks[row->failure.successor_block_id];
    if (!intent_execution_append_payload_definition(
            routine, success_block, &step->success_branch,
            "IntentSuccessPayload")
        || !intent_execution_append_completion(
            routine, success_block, step,
            &row->completion_instruction_id)
        || !intent_execution_append_payload_definition(
            routine, failure_block, &step->failure_branch,
            "IntentFailurePayload")) {
        return false;
    }
    row->completion_block_id = row->success.successor_block_id;
    if (!intent_execution_capture_compensations(
            routine, step, compensation_block_id, row)) {
        return false;
    }
    return true;
}
