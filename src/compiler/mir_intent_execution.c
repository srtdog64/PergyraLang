#include "mir_intent_execution.h"
#include "mir_intent_execution_graph.h"

#include "dir.h"
#include "mir_base_helpers.h"
#include "mir_json_expression_graph.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static uint32_t intent_execution_digest_rows(uint32_t hash,
                                             const MIRRoutine *routine);

static const DIRIntentInfo *
intent_execution_dir_info(const DIRProgram *dir, const MIRRoutine *routine)
{
    if (dir == NULL || routine == NULL || routine->source_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        if (info->node_id < dir->node_count
            && dir->nodes[info->node_id].source_syntax_id
                == routine->source_syntax_id) {
            return info;
        }
    }
    return NULL;
}

static bool
intent_execution_append_terminal_instruction(
    MIRRoutine *routine,
    MIRBasicBlock *block,
    const DIRIntentTerminal *terminal,
    const char *definition_name,
    size_t *instruction_id_out)
{
    MIRInstruction inst;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_RETURN;
    inst.name = "IntentTerminalResult";
    inst.slot_anchor = terminal->step_name;
    inst.arg0 = terminal->result_variant_name;
    inst.arg1 = terminal->result_payload_name;
    inst.result_name = pergyra_strdup(definition_name);
    inst.abi_type_name = pgy_arena_strdup(
        &routine->scratch, terminal->result_type_name);
    inst.ast = terminal->expr;
    inst.expr0 = terminal->expr;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_kind = HIR_BLOCK_RETURN;
    inst.source_terminator_has_value = true;
    mir_instruction_capture_source_provenance(&inst, terminal->expr);
    if (inst.result_name == NULL || inst.abi_type_name == NULL) {
        free((void *)inst.result_name);
        return false;
    }
    if (!mir_commit_instruction(routine, block, &inst)) {
        free((void *)inst.result_name);
        return false;
    }
    *instruction_id_out = inst.id;
    return true;
}

static const MIRIntentStepTransitionFact *
intent_execution_step_by_syntax_id(const MIRRoutine *routine,
                                   uint32_t step_syntax_id)
{
    const MIRIntentStepTransitionFact *found = NULL;

    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *row =
            &routine->intent_step_transitions[i];
        if (row->step_syntax_id == step_syntax_id) {
            if (found != NULL)
                return NULL;
            found = row;
        }
    }
    return found;
}

static const MIRIntentStepTransitionFact *
intent_execution_root_step(const MIRRoutine *routine)
{
    const MIRIntentStepTransitionFact *root = NULL;

    if (routine == NULL)
        return NULL;
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *candidate =
            &routine->intent_step_transitions[i];
        if (candidate->has_predecessor)
            continue;
        if (candidate->predecessor_transition_id != 0
            || candidate->predecessor_step_syntax_id != 0
            || candidate->predecessor_step_name != NULL
            || root != NULL) {
            return NULL;
        }
        root = candidate;
    }
    return root;
}

static const MIRIntentStepTransitionFact *
intent_execution_child_step(const MIRRoutine *routine,
                            const MIRIntentStepTransitionFact *parent)
{
    const MIRIntentStepTransitionFact *child = NULL;

    if (routine == NULL || parent == NULL)
        return NULL;
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *candidate =
            &routine->intent_step_transitions[i];
        if (!candidate->has_predecessor
            || candidate->predecessor_transition_id
                != parent->transition_id) {
            continue;
        }
        if (candidate->predecessor_step_syntax_id
                != parent->step_syntax_id
            || candidate->predecessor_step_name == NULL
            || parent->step_name == NULL
            || strcmp(candidate->predecessor_step_name,
                      parent->step_name) != 0
            || child != NULL) {
            return NULL;
        }
        child = candidate;
    }
    return child;
}

static bool
intent_execution_materialize_terminal(
    MIRRoutine *routine,
    const DIRIntentTerminal *terminal,
    MIRIntentTerminalRole role,
    size_t block_id,
    MIRIntentTerminalTransitionFact *row)
{
    const MIRIntentStepTransitionFact *source;
    const MIRIntentOutcomeBranchFact *branch;
    char *definition;

    source = intent_execution_step_by_syntax_id(
        routine, terminal->step_syntax_id);
    if (source == NULL || terminal->expr == NULL
        || ast_node_stable_id(terminal->expr) == 0
        || terminal->result_type_name == NULL
        || terminal->result_enum_decl_syntax_id == 0
        || terminal->result_variant_index == SIZE_MAX
        || terminal->result_variant_name == NULL
        || terminal->result_payload_name == NULL
        || terminal->result_payload_type_name == NULL
        || terminal->result_payload_decl_syntax_id == 0) {
        return false;
    }
    branch = role == MIR_INTENT_TERMINAL_SUCCESS
        ? &source->success : &source->failure;
    if (strcmp(terminal->result_payload_name, branch->payload_name) != 0
        || strcmp(terminal->result_payload_type_name,
                  branch->payload_type_name) != 0
        || branch->payload_decl_syntax_id == 0
        || terminal->result_payload_decl_syntax_id
            != branch->payload_decl_syntax_id) {
        return false;
    }
    definition = mir_strdup_fmt(
        "intent.result.%u", (unsigned)ast_node_stable_id(terminal->expr));
    if (definition == NULL)
        return false;
    row->result_definition_name = pgy_arena_strdup(
        &routine->scratch, definition);
    free(definition);
    if (row->result_definition_name == NULL
        || !intent_execution_append_terminal_instruction(
            routine, &routine->blocks[block_id], terminal,
            row->result_definition_name, &row->result_instruction_id)
        || !mir_expression_graph_identity(terminal->expr,
            &row->graph_root_id, &row->graph_digest)) {
        return false;
    }
    row->terminal_transition_id = ast_node_stable_id(terminal->expr);
    row->routine_syntax_id = routine->source_syntax_id;
    row->role = role;
    row->source_transition_id = source->transition_id;
    row->source_step_syntax_id = source->step_syntax_id;
    row->source_step_name = source->step_name;
    row->source_variant_index = branch->variant_index;
    row->source_variant_name = branch->variant_name;
    row->source_payload_name = branch->payload_name;
    row->source_payload_type_name = branch->payload_type_name;
    row->source_payload_decl_syntax_id = branch->payload_decl_syntax_id;
    row->result_instruction_block_id = block_id;
    row->result_type_name = terminal->result_type_name;
    row->result_enum_name = terminal->result_type_name;
    row->result_enum_syntax_id = terminal->result_enum_decl_syntax_id;
    row->result_variant_index = terminal->result_variant_index;
    row->result_variant_name = terminal->result_variant_name;
    row->result_payload_name = terminal->result_payload_name;
    row->result_payload_type_name = terminal->result_payload_type_name;
    row->result_payload_decl_syntax_id =
        terminal->result_payload_decl_syntax_id;
    row->expression_syntax_id = ast_node_stable_id(terminal->expr);
    row->expression = terminal->expr;
    row->sealed = true;
    return true;
}

static void
intent_execution_detach_hir_skeleton(MIRRoutine *routine)
{
    size_t source_block_count = routine != NULL && routine->hir_routine != NULL
        && routine->hir_routine->has_cfg
            ? routine->hir_routine->cfg.block_count : 0;

    for (size_t i = 0; i < source_block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        block->has_succ_true = false;
        block->has_succ_false = false;
        if (i == routine->entry_block)
            continue;
        block->is_reachable = false;
        free(block->predecessors);
        block->predecessors = NULL;
        block->predecessor_count = 0;
        block->predecessor_capacity = 0;
    }
}

bool
mir_materialize_intent_execution_plan(MIRRoutine *routine,
                                      const DIRProgram *dir)
{
    const DIRIntentInfo *info;
    size_t *compensation_blocks = NULL;
    size_t success_terminal_block = SIZE_MAX;
    size_t *failure_terminal_blocks = NULL;

    if (routine == NULL || routine->kind != MIR_SCOPE_INTENT)
        return true;
    info = intent_execution_dir_info(dir, routine);
    if (info == NULL)
        return false;
    if (!info->has_typed_result)
        return routine->return_type_name != NULL
            && strcmp(routine->return_type_name, "Bool") == 0;
    if (routine->return_type_name == NULL
        || strcmp(routine->return_type_name, info->return_type_name) != 0
        || info->step_count == 0
        || info->failure_terminal_count != info->step_count) {
        return false;
    }

    routine->intent_step_transitions = calloc(
        info->step_count, sizeof(*routine->intent_step_transitions));
    compensation_blocks = calloc(info->step_count, sizeof(*compensation_blocks));
    failure_terminal_blocks = calloc(
        info->failure_terminal_count, sizeof(*failure_terminal_blocks));
    routine->intent_terminal_transitions = calloc(
        info->failure_terminal_count + 1,
        sizeof(*routine->intent_terminal_transitions));
    if (routine->intent_step_transitions == NULL
        || compensation_blocks == NULL || failure_terminal_blocks == NULL
        || routine->intent_terminal_transitions == NULL) {
        goto fail;
    }
    routine->intent_step_transition_count = info->step_count;
    routine->intent_step_transition_capacity = info->step_count;
    routine->intent_terminal_transition_count =
        info->failure_terminal_count + 1;
    routine->intent_terminal_transition_capacity =
        routine->intent_terminal_transition_count;

    for (size_t i = 0; i < info->step_count; i++) {
        const DIRIntentStep *step = &info->steps[i];
        MIRIntentStepTransitionFact *row =
            &routine->intent_step_transitions[i];

        if (step->syntax_id == 0 || step->name == NULL
            || step->outcome_binding_name == NULL
            || step->outcome_binding_type_name == NULL
            || step->outcome_action_decl_syntax_id == 0
            || step->success_branch.enum_decl_syntax_id == 0
            || step->success_branch.payload_decl_syntax_id == 0
            || step->failure_branch.payload_decl_syntax_id == 0
            || step->success_branch.enum_decl_syntax_id
                != step->failure_branch.enum_decl_syntax_id
            || !intent_execution_append_block(
                routine, true, &row->branch_block_id)
            || !intent_execution_append_block(
                routine, true, &row->success.successor_block_id)
            || !intent_execution_append_block(
                routine, true, &row->failure.successor_block_id)
            || !intent_execution_append_block(
                routine, false, &compensation_blocks[i])) {
            goto fail;
        }
        row->transition_id = step->syntax_id;
        row->routine_syntax_id = routine->source_syntax_id;
        row->step_syntax_id = step->syntax_id;
        row->step_name = step->name;
        row->has_predecessor = step->predecessor_step_syntax_id != 0;
        row->predecessor_transition_id = step->predecessor_step_syntax_id;
        row->predecessor_step_syntax_id = step->predecessor_step_syntax_id;
        row->predecessor_step_name = step->predecessor_step_name;
        row->action_syntax_id = step->outcome_action_decl_syntax_id;
        row->outcome_result_name = step->outcome_binding_name;
        row->outcome_type_name = step->outcome_binding_type_name;
        row->outcome_enum_name = step->success_branch.enum_type_name;
        row->outcome_enum_syntax_id =
            step->success_branch.enum_decl_syntax_id;
        row->success.variant_index = step->success_branch.variant_index;
        row->success.variant_name = step->success_branch.variant_name;
        row->success.payload_name = step->success_branch.payload_name;
        row->success.payload_type_name =
            step->success_branch.payload_type_name;
        row->success.payload_decl_syntax_id =
            step->success_branch.payload_decl_syntax_id;
        row->failure.variant_index = step->failure_branch.variant_index;
        row->failure.variant_name = step->failure_branch.variant_name;
        row->failure.payload_name = step->failure_branch.payload_name;
        row->failure.payload_type_name =
            step->failure_branch.payload_type_name;
        row->failure.payload_decl_syntax_id =
            step->failure_branch.payload_decl_syntax_id;
        if (!intent_execution_materialize_step(
                routine, step, row, compensation_blocks[i])) {
            goto fail;
        }
        row->sealed = true;
    }

    if (!intent_execution_append_block(
            routine, true, &success_terminal_block)) {
        goto fail;
    }
    for (size_t i = 0; i < info->failure_terminal_count; i++) {
        if (!intent_execution_append_block(
                routine, true, &failure_terminal_blocks[i])) {
            goto fail;
        }
    }
    if (!intent_execution_materialize_terminal(
            routine, &info->success_terminal, MIR_INTENT_TERMINAL_SUCCESS,
            success_terminal_block,
            &routine->intent_terminal_transitions[0])) {
        goto fail;
    }
    for (size_t i = 0; i < info->failure_terminal_count; i++) {
        if (!intent_execution_materialize_terminal(
                routine, &info->failure_terminals[i],
                MIR_INTENT_TERMINAL_FAILURE,
                failure_terminal_blocks[i],
                &routine->intent_terminal_transitions[i + 1])) {
            goto fail;
        }
    }

    intent_execution_detach_hir_skeleton(routine);
    {
        const MIRIntentStepTransitionFact *root =
            intent_execution_root_step(routine);
        if (root == NULL || !intent_execution_set_goto(
                routine, routine->entry_block, root->branch_block_id)) {
            goto fail;
        }
    }
    for (size_t i = 0; i < info->step_count; i++) {
        MIRIntentStepTransitionFact *row =
            &routine->intent_step_transitions[i];
        const MIRIntentStepTransitionFact *child =
            intent_execution_child_step(routine, row);
        bool has_declared_child = false;
        size_t success_target;
        size_t failure_target = SIZE_MAX;

        for (size_t j = 0; j < info->step_count; j++) {
            if (routine->intent_step_transitions[j].has_predecessor
                && routine->intent_step_transitions[j]
                    .predecessor_transition_id == row->transition_id) {
                has_declared_child = true;
                break;
            }
        }
        if (has_declared_child && child == NULL) {
            goto fail;
        }
        success_target = child != NULL
            ? child->branch_block_id : success_terminal_block;

        for (size_t t = 0; t < info->failure_terminal_count; t++) {
            if (info->failure_terminals[t].step_syntax_id
                == row->step_syntax_id) {
                if (failure_target != SIZE_MAX) {
                    goto fail;
                }
                failure_target = failure_terminal_blocks[t];
            }
        }
        if (failure_target == SIZE_MAX
            || !intent_execution_set_branch(
                routine, row->branch_block_id,
                row->success.successor_block_id,
                row->failure.successor_block_id)
            || !intent_execution_set_goto(
                routine, row->success.successor_block_id, success_target)
            || !intent_execution_set_goto(
                routine, row->failure.successor_block_id, failure_target)) {
            goto fail;
        }
    }
    routine->intent_execution_plan_admitted = true;
    routine->intent_execution_plan_digest =
        mir_intent_execution_routine_digest(routine);
    free(compensation_blocks);
    free(failure_terminal_blocks);
    return true;

fail:
    free(compensation_blocks);
    free(failure_terminal_blocks);
    return false;
}

static uint32_t
intent_execution_hash_string(uint32_t hash, const char *value)
{
    const unsigned char *cursor =
        (const unsigned char *)(value != NULL ? value : "");
    size_t length = value != NULL ? strlen(value) : 0;

    hash = (uint32_t)(((uint64_t)hash * 131u + length) % 268435456u);
    while (*cursor != '\0') {
        hash = (uint32_t)(((uint64_t)hash * 131u + *cursor)
                          % 268435456u);
        cursor++;
    }
    return hash;
}

static uint32_t
intent_execution_hash_int(uint32_t hash, uint32_t value)
{
    return (uint32_t)(((uint64_t)hash * 131u + value + 2u)
                      % 268435456u);
}

static uint32_t
intent_execution_digest_rows(uint32_t hash, const MIRRoutine *routine)
{
    for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
        const MIRIntentStepTransitionFact *row =
            &routine->intent_step_transitions[i];
        hash = intent_execution_hash_int(hash, row->transition_id);
        hash = intent_execution_hash_int(hash, row->routine_syntax_id);
        hash = intent_execution_hash_int(hash, row->step_syntax_id);
        hash = intent_execution_hash_string(hash, row->step_name);
        hash = intent_execution_hash_int(
            hash, row->has_predecessor ? 1u : 0u);
        hash = intent_execution_hash_int(
            hash, row->predecessor_transition_id);
        hash = intent_execution_hash_int(
            hash, row->predecessor_step_syntax_id);
        hash = intent_execution_hash_string(
            hash, row->predecessor_step_name);
        hash = intent_execution_hash_int(hash, row->action_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->outcome_instruction_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->outcome_instruction_id);
        hash = intent_execution_hash_string(hash, row->outcome_result_name);
        hash = intent_execution_hash_string(hash, row->outcome_type_name);
        hash = intent_execution_hash_string(hash, row->outcome_enum_name);
        hash = intent_execution_hash_int(hash, row->outcome_enum_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->branch_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->branch_instruction_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->success.variant_index);
        hash = intent_execution_hash_string(hash, row->success.variant_name);
        hash = intent_execution_hash_string(
            hash, row->success.payload_name);
        hash = intent_execution_hash_string(
            hash, row->success.payload_type_name);
        hash = intent_execution_hash_int(
            hash, row->success.payload_decl_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->success.successor_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->failure.variant_index);
        hash = intent_execution_hash_string(hash, row->failure.variant_name);
        hash = intent_execution_hash_string(
            hash, row->failure.payload_name);
        hash = intent_execution_hash_string(
            hash, row->failure.payload_type_name);
        hash = intent_execution_hash_int(
            hash, row->failure.payload_decl_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->failure.successor_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->completion_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->completion_instruction_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->compensation_count);
        for (size_t c = 0; c < row->compensation_count; c++) {
            const MIRIntentCompensationFact *fact = &row->compensations[c];
            hash = intent_execution_hash_int(hash, fact->transition_id);
            hash = intent_execution_hash_int(
                hash, fact->expression_syntax_id);
            hash = intent_execution_hash_int(
                hash, (uint32_t)fact->instruction_block_id);
            hash = intent_execution_hash_int(
                hash, (uint32_t)fact->instruction_id);
            hash = intent_execution_hash_int(
                hash, (uint32_t)fact->graph_root_id);
            hash = intent_execution_hash_int(hash, fact->graph_digest);
            hash = intent_execution_hash_string(
                hash, fact->call_target_name);
            hash = intent_execution_hash_int(
                hash, fact->call_target_syntax_id);
        }
    }
    for (size_t i = 0; i < routine->intent_terminal_transition_count; i++) {
        const MIRIntentTerminalTransitionFact *row =
            &routine->intent_terminal_transitions[i];
        hash = intent_execution_hash_int(
            hash, row->terminal_transition_id);
        hash = intent_execution_hash_int(hash, row->routine_syntax_id);
        hash = intent_execution_hash_string(
            hash, mir_intent_terminal_role_name(row->role));
        hash = intent_execution_hash_int(hash, row->source_transition_id);
        hash = intent_execution_hash_int(
            hash, row->source_step_syntax_id);
        hash = intent_execution_hash_string(hash, row->source_step_name);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->source_variant_index);
        hash = intent_execution_hash_string(hash, row->source_variant_name);
        hash = intent_execution_hash_string(
            hash, row->source_payload_name);
        hash = intent_execution_hash_string(
            hash, row->source_payload_type_name);
        hash = intent_execution_hash_int(
            hash, row->source_payload_decl_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->result_instruction_block_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->result_instruction_id);
        hash = intent_execution_hash_string(
            hash, row->result_definition_name);
        hash = intent_execution_hash_string(hash, row->result_type_name);
        hash = intent_execution_hash_string(hash, row->result_enum_name);
        hash = intent_execution_hash_int(hash, row->result_enum_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->result_variant_index);
        hash = intent_execution_hash_string(hash, row->result_variant_name);
        hash = intent_execution_hash_string(
            hash, row->result_payload_name);
        hash = intent_execution_hash_string(
            hash, row->result_payload_type_name);
        hash = intent_execution_hash_int(
            hash, row->result_payload_decl_syntax_id);
        hash = intent_execution_hash_int(hash, row->expression_syntax_id);
        hash = intent_execution_hash_int(
            hash, (uint32_t)row->graph_root_id);
        hash = intent_execution_hash_int(hash, row->graph_digest);
    }
    return hash;
}

uint32_t
mir_intent_execution_routine_digest(const MIRRoutine *routine)
{
    uint32_t hash = intent_execution_hash_string(
        71u, PGY_MIR_INTENT_EXECUTION_SCHEMA);

    if (routine == NULL || !routine->intent_execution_plan_admitted)
        return 0;
    hash = intent_execution_digest_rows(hash, routine);
    return 1073741824u + hash;
}

uint32_t
mir_intent_execution_program_digest(const MIRProgram *mir)
{
    uint32_t hash = intent_execution_hash_string(
        71u, PGY_MIR_INTENT_EXECUTION_SCHEMA);

    if (mir != NULL) {
        for (size_t i = 0; i < mir->routine_count; i++) {
            const MIRRoutine *routine = &mir->routines[i];
            if (routine->intent_execution_plan_admitted)
                hash = intent_execution_digest_rows(hash, routine);
        }
    }
    return 1073741824u + hash;
}
