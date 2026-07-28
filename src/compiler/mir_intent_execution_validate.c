#include "mir_intent_execution.h"

#include "mir_decl_headers.h"
#include "mir_json_expression_graph.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

static char *
intent_execution_error(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *message;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }
    message = malloc((size_t)length + 1);
    if (message != NULL)
        (void)vsnprintf(message, (size_t)length + 1, fmt, args);
    va_end(args);
    return message;
}

static bool
intent_execution_reject(char **error_message,
                        const MIRRoutine *routine,
                        const char *detail)
{
    if (error_message != NULL) {
        *error_message = intent_execution_error(
            "MIR typed intent execution plan '%s' is invalid: %s",
            routine != NULL && routine->name != NULL
                ? routine->name : "(anonymous)",
            detail != NULL ? detail : "unknown fact drift");
    }
    return false;
}

static const MIRDeclHeader *
intent_execution_decl_by_identity(const MIRProgram *mir,
                                  uint32_t source_syntax_id,
                                  ASTNodeType ast_type,
                                  const char *name)
{
    const MIRDeclHeader *found = NULL;

    if (mir == NULL || source_syntax_id == 0 || name == NULL)
        return NULL;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *candidate = &mir->decl_headers[i];
        if (candidate->source_syntax_id == source_syntax_id
            && candidate->ast_type == ast_type
            && candidate->name != NULL
            && strcmp(candidate->name, name) == 0) {
            if (found != NULL)
                return NULL;
            found = candidate;
        }
    }
    return found;
}

static const MIRDeclHeader *
intent_execution_tobject_by_name(const MIRProgram *mir, const char *name)
{
    const MIRDeclHeader *found = NULL;

    if (mir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *candidate = &mir->decl_headers[i];
        if (candidate->ast_type == AST_CLASS_DECL
            && candidate->nominal_kind == NOMINAL_DECL_TOBJECT
            && candidate->name != NULL
            && strcmp(candidate->name, name) == 0) {
            if (found != NULL)
                return NULL;
            found = candidate;
        }
    }
    return found;
}

static bool
intent_execution_enum_branch_is_sealed(
    const MIRProgram *mir,
    uint32_t enum_syntax_id,
    const char *enum_name,
    const MIRIntentOutcomeBranchFact *branch)
{
    const MIRDeclHeader *header = intent_execution_decl_by_identity(
        mir, enum_syntax_id, AST_ENUM_DECL, enum_name);
    const MIRDeclEnumVariant *variant;
    const char *payload_type;

    if (header == NULL || branch == NULL
        || branch->variant_index >= header->variant_metadata_count) {
        return false;
    }
    variant = mir_decl_header_enum_variant(header, branch->variant_index);
    payload_type = mir_decl_enum_variant_param_type_name(variant, 0);
    return variant != NULL
        && mir_decl_enum_variant_name(variant) != NULL
        && strcmp(mir_decl_enum_variant_name(variant),
                  branch->variant_name) == 0
        && mir_decl_enum_variant_param_count(variant) == 1
        && payload_type != NULL
        && strcmp(payload_type, branch->payload_type_name) == 0
        && intent_execution_tobject_by_name(mir, payload_type) != NULL;
}

static bool
intent_execution_terminal_result_is_sealed(
    const MIRProgram *mir,
    const MIRIntentTerminalTransitionFact *row)
{
    const MIRDeclHeader *header;
    const MIRDeclEnumVariant *variant;
    const char *payload_type;

    if (row == NULL)
        return false;
    header = intent_execution_decl_by_identity(mir,
        row->result_enum_syntax_id, AST_ENUM_DECL,
        row->result_enum_name);
    if (header == NULL
        || row->result_variant_index >= header->variant_metadata_count) {
        return false;
    }
    variant = mir_decl_header_enum_variant(
        header, row->result_variant_index);
    payload_type = mir_decl_enum_variant_param_type_name(variant, 0);
    return variant != NULL
        && mir_decl_enum_variant_name(variant) != NULL
        && strcmp(mir_decl_enum_variant_name(variant),
                  row->result_variant_name) == 0
        && mir_decl_enum_variant_param_count(variant) == 1
        && payload_type != NULL
        && strcmp(payload_type, row->result_payload_type_name) == 0
        && intent_execution_tobject_by_name(mir, payload_type) != NULL;
}

static const MIRInstruction *
intent_execution_instruction(const MIRRoutine *routine,
                             size_t block_id,
                             size_t instruction_id)
{
    const MIRInstruction *found = NULL;

    if (routine == NULL || block_id >= routine->block_count)
        return NULL;
    for (size_t i = 0;
         i < routine->blocks[block_id].instruction_count; i++) {
        const MIRInstruction *candidate =
            &routine->blocks[block_id].instructions[i];
        if (candidate->id == instruction_id) {
            if (found != NULL)
                return NULL;
            found = candidate;
        }
    }
    return found;
}

static const MIRIntentStepTransitionFact *
intent_execution_step(const MIRRoutine *routine,
                      uint32_t transition_id,
                      uint32_t step_syntax_id,
                      const char *step_name)
{
    const MIRIntentStepTransitionFact *found = NULL;

    if (routine == NULL || transition_id == 0 || step_syntax_id == 0
        || step_name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *candidate =
            &routine->intent_step_transitions[i];
        if (candidate->transition_id == transition_id
            && candidate->step_syntax_id == step_syntax_id
            && candidate->step_name != NULL
            && strcmp(candidate->step_name, step_name) == 0) {
            if (found != NULL)
                return NULL;
            found = candidate;
        }
    }
    return found;
}

static bool
intent_execution_plan_block(const MIRRoutine *routine, size_t block_id)
{
    return routine != NULL && block_id < routine->block_count
        && routine->blocks[block_id].is_intent_execution_plan_block;
}

static bool
intent_execution_branch_row_ready(const MIRIntentOutcomeBranchFact *branch)
{
    return branch != NULL && branch->variant_index != SIZE_MAX
        && branch->variant_name != NULL && branch->variant_name[0] != '\0'
        && branch->payload_name != NULL && branch->payload_name[0] != '\0'
        && branch->payload_type_name != NULL
        && branch->payload_type_name[0] != '\0';
}

static bool
intent_execution_validate_graph(ASTNode *expression,
                                size_t expected_root,
                                uint32_t expected_digest)
{
    size_t root = SIZE_MAX;
    uint32_t digest = 0;

    return expression != NULL
        && mir_expression_graph_identity(expression, &root, &digest)
        && root == expected_root && digest == expected_digest;
}

static bool
intent_execution_validate_compensations(
    const MIRRoutine *routine,
    const MIRIntentStepTransitionFact *row)
{
    if (row->compensation_count > 0 && row->compensations == NULL)
        return false;
    for (size_t i = 0; i < row->compensation_count; i++) {
        const MIRIntentCompensationFact *fact = &row->compensations[i];
        const MIRInstruction *inst = intent_execution_instruction(
            routine, fact->instruction_block_id, fact->instruction_id);
        if (fact->transition_id != row->transition_id
            || fact->expression_syntax_id == 0
            || fact->call_target_name == NULL
            || fact->call_target_name[0] == '\0'
            || fact->call_target_syntax_id == 0
            || !intent_execution_plan_block(
                routine, fact->instruction_block_id)
            || inst == NULL
            || !mir_instruction_is_intent_stmt(inst, "IntentEval")
            || inst->arg0 == NULL || strcmp(inst->arg0, "compensate") != 0
            || inst->expr0 != fact->expression
            || ast_node_stable_id(fact->expression)
                != fact->expression_syntax_id
            || ast_call_semantic_callee_decl_id(fact->expression)
                != fact->call_target_syntax_id
            || !intent_execution_validate_graph(
                fact->expression, fact->graph_root_id,
                fact->graph_digest)) {
            return false;
        }
    }
    return true;
}

static bool
intent_execution_validate_step(const MIRRoutine *routine,
                               const MIRIntentStepTransitionFact *row)
{
    const MIRInstruction *outcome;
    const MIRInstruction *branch;
    const MIRInstruction *completion;
    size_t completion_count = 0;

    if (row == NULL || !row->sealed || row->transition_id == 0
        || row->transition_id != row->step_syntax_id
        || row->routine_syntax_id != routine->source_syntax_id
        || row->step_name == NULL || row->step_name[0] == '\0'
        || row->action_syntax_id == 0
        || row->outcome_result_name == NULL
        || row->outcome_result_name[0] == '\0'
        || row->outcome_type_name == NULL
        || row->outcome_type_name[0] == '\0'
        || row->outcome_enum_name == NULL
        || strcmp(row->outcome_enum_name, row->outcome_type_name) != 0
        || row->outcome_enum_syntax_id == 0
        || !intent_execution_branch_row_ready(&row->success)
        || !intent_execution_branch_row_ready(&row->failure)
        || row->success.variant_index == row->failure.variant_index
        || strcmp(row->success.variant_name,
                  row->failure.variant_name) == 0
        || row->success.successor_block_id
            == row->failure.successor_block_id
        || row->completion_block_id
            != row->success.successor_block_id) {
        return false;
    }
    if (row->has_predecessor) {
        if (row->predecessor_transition_id == 0
            || row->predecessor_step_syntax_id == 0
            || row->predecessor_step_name == NULL
            || intent_execution_step(routine,
                row->predecessor_transition_id,
                row->predecessor_step_syntax_id,
                row->predecessor_step_name) == NULL) {
            return false;
        }
    } else if (row->predecessor_transition_id != 0
               || row->predecessor_step_syntax_id != 0
               || row->predecessor_step_name != NULL) {
        return false;
    }
    if (!intent_execution_plan_block(routine, row->branch_block_id)
        || !intent_execution_plan_block(
            routine, row->success.successor_block_id)
        || !intent_execution_plan_block(
            routine, row->failure.successor_block_id)) {
        return false;
    }
    outcome = intent_execution_instruction(
        routine, row->outcome_instruction_block_id,
        row->outcome_instruction_id);
    branch = intent_execution_instruction(
        routine, row->branch_block_id, row->branch_instruction_id);
    completion = intent_execution_instruction(
        routine, row->completion_block_id, row->completion_instruction_id);
    if (outcome == NULL
        || !mir_instruction_is_intent_stmt(outcome, "IntentEval")
        || outcome->arg0 == NULL || strcmp(outcome->arg0, "on") != 0
        || outcome->expr0 == NULL || outcome->expr0->type != AST_CALL
        || ast_call_semantic_callee_decl_id(outcome->expr0)
            != row->action_syntax_id
        || outcome->result_name == NULL
        || strcmp(outcome->result_name, row->outcome_result_name) != 0
        || outcome->abi_type_name == NULL
        || strcmp(outcome->abi_type_name, row->outcome_type_name) != 0
        || outcome->expr0 != row->outcome_expression
        || branch == NULL || branch->kind != MIR_INST_BRANCH
        || branch->name == NULL
        || strcmp(branch->name, "IntentOutcomeBranch") != 0
        || completion == NULL
        || !mir_instruction_is_intent_stmt(
            completion, "IntentStepCompleted")
        || !routine->blocks[row->branch_block_id].has_succ_true
        || routine->blocks[row->branch_block_id].succ_true
            != row->success.successor_block_id
        || !routine->blocks[row->branch_block_id].has_succ_false
        || routine->blocks[row->branch_block_id].succ_false
            != row->failure.successor_block_id) {
        return false;
    }
    for (size_t b = 0; b < routine->block_count; b++) {
        for (size_t i = 0; i < routine->blocks[b].instruction_count; i++) {
            const MIRInstruction *inst = &routine->blocks[b].instructions[i];
            if (mir_instruction_is_intent_stmt(
                    inst, "IntentStepCompleted")
                && inst->slot_anchor != NULL
                && strcmp(inst->slot_anchor, row->step_name) == 0) {
                completion_count++;
                if (b != row->success.successor_block_id
                    || inst->id != row->completion_instruction_id) {
                    return false;
                }
            }
        }
    }
    return completion_count == 1
        && intent_execution_validate_compensations(routine, row);
}

static bool
intent_execution_has_child(const MIRRoutine *routine,
                           uint32_t transition_id)
{
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        if (routine->intent_step_transitions[i].has_predecessor
            && routine->intent_step_transitions[i]
                .predecessor_transition_id == transition_id) {
            return true;
        }
    }
    return false;
}

static bool
intent_execution_cycle_free(const MIRRoutine *routine,
                            const MIRIntentStepTransitionFact *start)
{
    const MIRIntentStepTransitionFact *row = start;

    for (size_t traversed = 0;
         traversed <= routine->intent_step_transition_count; traversed++) {
        if (!row->has_predecessor)
            return true;
        row = intent_execution_step(routine,
            row->predecessor_transition_id,
            row->predecessor_step_syntax_id,
            row->predecessor_step_name);
        if (row == NULL || row == start)
            return false;
    }
    return false;
}

static bool
intent_execution_validate_terminal(
    const MIRRoutine *routine,
    const MIRIntentTerminalTransitionFact *row)
{
    const MIRIntentStepTransitionFact *source;
    const MIRIntentOutcomeBranchFact *branch;
    const MIRInstruction *result;

    if (row == NULL || !row->sealed
        || row->terminal_transition_id == 0
        || row->routine_syntax_id != routine->source_syntax_id
        || (row->role != MIR_INTENT_TERMINAL_SUCCESS
            && row->role != MIR_INTENT_TERMINAL_FAILURE)
        || row->result_type_name == NULL
        || routine->return_type_name == NULL
        || strcmp(row->result_type_name, routine->return_type_name) != 0
        || row->result_enum_name == NULL
        || strcmp(row->result_enum_name, row->result_type_name) != 0
        || row->result_enum_syntax_id == 0
        || row->result_variant_index == SIZE_MAX
        || row->result_variant_name == NULL
        || row->result_payload_name == NULL
        || row->result_payload_type_name == NULL
        || row->expression == NULL
        || ast_node_stable_id(row->expression)
            != row->expression_syntax_id
        || !intent_execution_plan_block(
            routine, row->result_instruction_block_id)) {
        return false;
    }
    source = intent_execution_step(routine,
        row->source_transition_id, row->source_step_syntax_id,
        row->source_step_name);
    if (source == NULL)
        return false;
    branch = row->role == MIR_INTENT_TERMINAL_SUCCESS
        ? &source->success : &source->failure;
    if (row->source_variant_index != branch->variant_index
        || row->source_variant_name == NULL
        || strcmp(row->source_variant_name, branch->variant_name) != 0
        || row->source_payload_name == NULL
        || strcmp(row->source_payload_name, branch->payload_name) != 0
        || row->source_payload_type_name == NULL
        || strcmp(row->source_payload_type_name,
                  branch->payload_type_name) != 0
        || strcmp(row->result_payload_name,
                  row->source_payload_name) != 0
        || strcmp(row->result_payload_type_name,
                  row->source_payload_type_name) != 0
        || (row->role == MIR_INTENT_TERMINAL_SUCCESS
            && intent_execution_has_child(
                routine, row->source_transition_id))) {
        return false;
    }
    result = intent_execution_instruction(routine,
        row->result_instruction_block_id, row->result_instruction_id);
    return result != NULL && result->kind == MIR_INST_RETURN
        && result->name != NULL
        && strcmp(result->name, "IntentTerminalResult") == 0
        && result->result_name != NULL
        && row->result_definition_name != NULL
        && strcmp(result->result_name,
                  row->result_definition_name) == 0
        && result->abi_type_name != NULL
        && strcmp(result->abi_type_name, row->result_type_name) == 0
        && result->expr0 == row->expression
        && intent_execution_validate_graph(
            row->expression, row->graph_root_id, row->graph_digest);
}

bool
mir_validate_intent_execution_plan(const MIRRoutine *routine,
                                   char **error_message)
{
    if (routine == NULL)
        return intent_execution_reject(error_message, routine, "null routine");
    if (routine->kind != MIR_SCOPE_INTENT) {
        if (routine->intent_execution_plan_admitted
            || routine->intent_step_transition_count != 0
            || routine->intent_terminal_transition_count != 0) {
            return intent_execution_reject(
                error_message, routine,
                "non-intent routine carries intent execution facts");
        }
        return true;
    }
    if (!routine->intent_execution_plan_admitted) {
        if (routine->intent_step_transition_count != 0
            || routine->intent_terminal_transition_count != 0
            || routine->intent_execution_plan_digest != 0) {
            return intent_execution_reject(
                error_message, routine,
                "unadmitted plan exposes partial rows");
        }
        if (routine->return_type_name != NULL
            && strcmp(routine->return_type_name, "Bool") != 0) {
            return intent_execution_reject(
                error_message, routine,
                "typed return has no admitted execution plan");
        }
        return true;
    }
    if (!routine->has_signature || routine->return_type_name == NULL
        || strcmp(routine->return_type_name, "Bool") == 0
        || routine->intent_step_transition_count == 0
        || routine->intent_step_transitions == NULL
        || routine->intent_terminal_transition_count
            != routine->intent_step_transition_count + 1
        || routine->intent_terminal_transitions == NULL) {
        return intent_execution_reject(
            error_message, routine,
            "admitted plan has incomplete signature or row inventory");
    }
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *row =
            &routine->intent_step_transitions[i];
        if (!intent_execution_validate_step(routine, row)
            || !intent_execution_cycle_free(routine, row)) {
            return intent_execution_reject(
                error_message, routine,
                "step transition cross-seal failed");
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->intent_step_transitions[j].transition_id
                    == row->transition_id
                || (routine->intent_step_transitions[j].step_name != NULL
                    && strcmp(routine->intent_step_transitions[j].step_name,
                              row->step_name) == 0)) {
                return intent_execution_reject(
                    error_message, routine,
                    "duplicate step transition identity");
            }
        }
    }
    for (size_t i = 0; i < routine->intent_terminal_transition_count; i++) {
        const MIRIntentTerminalTransitionFact *row =
            &routine->intent_terminal_transitions[i];
        if (!intent_execution_validate_terminal(routine, row)) {
            return intent_execution_reject(
                error_message, routine,
                "terminal transition cross-seal failed");
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->intent_terminal_transitions[j]
                    .terminal_transition_id
                == row->terminal_transition_id) {
                return intent_execution_reject(
                    error_message, routine,
                    "duplicate terminal transition identity");
            }
        }
    }
    for (size_t s = 0; s < routine->intent_step_transition_count; s++) {
        const MIRIntentStepTransitionFact *step =
            &routine->intent_step_transitions[s];
        size_t failures = 0;
        size_t successes = 0;
        for (size_t t = 0;
             t < routine->intent_terminal_transition_count; t++) {
            const MIRIntentTerminalTransitionFact *terminal =
                &routine->intent_terminal_transitions[t];
            if (terminal->source_transition_id != step->transition_id)
                continue;
            if (terminal->role == MIR_INTENT_TERMINAL_FAILURE)
                failures++;
            else if (terminal->role == MIR_INTENT_TERMINAL_SUCCESS)
                successes++;
        }
        if (failures != 1
            || (intent_execution_has_child(routine, step->transition_id)
                ? successes != 0 : successes != 1)) {
            return intent_execution_reject(
                error_message, routine,
                "terminal coverage is not one failure per step and one leaf success");
        }
    }
    if (routine->intent_execution_plan_digest
        != mir_intent_execution_routine_digest(routine)) {
        return intent_execution_reject(
            error_message, routine, "plan digest drifted");
    }
    return true;
}

bool
mir_validate_intent_execution_program(const MIRProgram *mir,
                                      char **error_message)
{
    if (mir == NULL)
        return intent_execution_reject(
            error_message, NULL, "null program");
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];

        if (!routine->intent_execution_plan_admitted)
            continue;
        for (size_t s = 0;
             s < routine->intent_step_transition_count; s++) {
            const MIRIntentStepTransitionFact *step =
                &routine->intent_step_transitions[s];
            if (!intent_execution_enum_branch_is_sealed(mir,
                    step->outcome_enum_syntax_id,
                    step->outcome_enum_name, &step->success)
                || !intent_execution_enum_branch_is_sealed(mir,
                    step->outcome_enum_syntax_id,
                    step->outcome_enum_name, &step->failure)) {
                return intent_execution_reject(error_message, routine,
                    "outcome enum variant/payload tobject cross-seal failed");
            }
        }
        for (size_t t = 0;
             t < routine->intent_terminal_transition_count; t++) {
            if (!intent_execution_terminal_result_is_sealed(
                    mir, &routine->intent_terminal_transitions[t])) {
                return intent_execution_reject(error_message, routine,
                    "terminal result enum variant/payload tobject cross-seal failed");
            }
        }
    }
    return true;
}
