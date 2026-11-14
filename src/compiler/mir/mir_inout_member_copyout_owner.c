#include "mir_inout_member_copyout_owner.h"

#include "mir_base_helpers.h"
#include "mir_branch_source_facts.h"

#include "../../common/string_compat.h"
#include "../../parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

#define MIR_INOUT_MEMBER_COPYOUT "inout-member-copyout"

typedef struct
{
    const MIRCollectionOwnershipFact *fact;
    size_t def_block;
    size_t def_instruction;
    size_t restore_block;
    size_t restore_instruction;
} MIRInoutMemberCopyout;

bool
mir_instruction_is_inout_member_copyout(const MIRInstruction *instruction)
{
    return instruction != NULL
        && instruction->kind == MIR_INST_ASSIGN
        && instruction->name != NULL
        && strcmp(instruction->name, MIR_INOUT_MEMBER_COPYOUT) == 0;
}

static bool
copyout_error(const MIRRoutine *routine, char **error_message,
              const char *reason, uint32_t binding)
{
    if (error_message != NULL && *error_message == NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' inout member copyout is %s (binding=%u)",
            routine != NULL && routine->name != NULL
                ? routine->name : "(anonymous)",
            reason != NULL ? reason : "invalid", binding);
    }
    return false;
}

static bool
member_path_matches(const ASTNode *left, const ASTNode *right,
                    uint32_t root_binding)
{
    if (left == NULL || right == NULL || root_binding == 0)
        return false;
    if (left->type == AST_MEMBER_ACCESS
        || right->type == AST_MEMBER_ACCESS) {
        const char *left_name;
        const char *right_name;
        if (left->type != AST_MEMBER_ACCESS
            || right->type != AST_MEMBER_ACCESS)
            return false;
        left_name = ast_member_name(left);
        right_name = ast_member_name(right);
        return left_name != NULL && right_name != NULL
            && strcmp(left_name, right_name) == 0
            && member_path_matches(ast_member_object(left),
                                   ast_member_object(right),
                                   root_binding);
    }
    return left->type == AST_IDENTIFIER
        && right->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(left) == root_binding
        && ast_identifier_binding_syntax_id(right) == root_binding;
}

static bool
routine_has_inout_parameter(const MIRRoutine *routine, uint32_t binding)
{
    size_t matches = 0;
    if (routine == NULL || binding == 0)
        return false;
    for (size_t p = 0; p < routine->param_count; p++) {
        FuncParam *parameter = mir_routine_param(routine, p);
        if (parameter != NULL
            && parameter->mode == PARAM_MODE_MUT_REF
            && ast_func_param_stable_id(parameter) == binding)
            matches++;
    }
    return matches == 1;
}

static bool
find_copyout_source(const MIRRoutine *routine,
                    const MIRCollectionOwnershipFact *fact,
                    MIRInoutMemberCopyout *copyout)
{
    size_t definitions = 0;
    size_t restorations = 0;

    if (routine == NULL || fact == NULL || copyout == NULL
        || fact->function_syntax_id != routine->source_syntax_id
        || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->disposition != PGY_COLLECTION_DISPOSITION_RETIRED
        || fact->binding_syntax_id == 0
        || fact->source_binding_syntax_id == 0
        || fact->member_move_source == NULL
        || fact->member_move_source_id == 0
        || ast_node_stable_id(fact->member_move_source)
            != fact->member_move_source_id
        || !routine_has_inout_parameter(
            routine, fact->source_binding_syntax_id))
        return false;

    memset(copyout, 0, sizeof(*copyout));
    copyout->fact = fact;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *instruction = &block->instructions[i];
            if (instruction->kind == MIR_INST_DEF
                && instruction->binding_syntax_id
                    == fact->binding_syntax_id
                && instruction->expr0 == fact->member_move_source) {
                definitions++;
                copyout->def_block = b;
                copyout->def_instruction = i;
            }
            if (instruction->kind != MIR_INST_ASSIGN
                || mir_instruction_is_inout_member_copyout(instruction)
                || instruction->ast == NULL
                || instruction->ast->type != AST_ASSIGNMENT
                || instruction->expr0 == NULL
                || instruction->expr1 == NULL
                || instruction->expr1->type != AST_IDENTIFIER
                || ast_identifier_binding_syntax_id(instruction->expr1)
                    != fact->binding_syntax_id
                || !member_path_matches(fact->member_move_source,
                    instruction->expr0,
                    fact->source_binding_syntax_id))
                continue;
            restorations++;
            copyout->restore_block = b;
            copyout->restore_instruction = i;
        }
    }
    return definitions == 1 && restorations == 1;
}

static bool
dominates(const MIRRoutine *routine, size_t dominator, size_t block)
{
    const HIRRoutine *hir = routine != NULL ? routine->hir_routine : NULL;
    if (hir == NULL || !hir->has_cfg
        || dominator >= hir->cfg.block_count
        || block >= hir->cfg.block_count)
        return false;
    for (size_t fuel = 0; fuel <= hir->cfg.block_count; fuel++) {
        const HIRBasicBlock *current;
        if (block == dominator)
            return true;
        current = &hir->cfg.blocks[block];
        if (!current->has_immediate_dominator
            || current->immediate_dominator == block)
            return false;
        block = current->immediate_dominator;
    }
    return false;
}

static bool
block_reaches(const MIRRoutine *routine, size_t from, size_t target,
              bool *valid)
{
    bool *seen;
    size_t *queue;
    size_t head = 0;
    size_t tail = 0;
    bool reached = false;

    if (valid == NULL)
        return false;
    *valid = false;
    if (routine == NULL || from >= routine->block_count
        || target >= routine->block_count)
        return false;
    seen = calloc(routine->block_count, sizeof(*seen));
    queue = calloc(routine->block_count, sizeof(*queue));
    if (seen == NULL || queue == NULL)
        goto done;
    queue[tail++] = from;
    seen[from] = true;
    while (head < tail) {
        size_t b = queue[head++];
        const MIRBasicBlock *block = &routine->blocks[b];
        size_t successors[5];
        size_t count = 0;
        if (b == target) {
            reached = true;
            break;
        }
        if (block->has_succ_true) successors[count++] = block->succ_true;
        if (block->has_succ_false) successors[count++] = block->succ_false;
        if (block->has_cleanup_succ) successors[count++] = block->cleanup_succ;
        if (block->has_rollback_succ) successors[count++] = block->rollback_succ;
        if (block->has_invalidation_succ)
            successors[count++] = block->invalidation_succ;
        for (size_t i = 0; i < count; i++) {
            size_t next = successors[i];
            if (next >= routine->block_count || seen[next])
                continue;
            seen[next] = true;
            queue[tail++] = next;
        }
    }
done:
    *valid = seen != NULL && queue != NULL;
    free(seen);
    free(queue);
    return reached;
}

static bool
copyout_required(const MIRRoutine *routine,
                 const MIRInoutMemberCopyout *copyout,
                 size_t return_block,
                 size_t return_instruction,
                 bool *required)
{
    bool def_dominates;
    bool restore_dominates;
    bool reachability_valid = false;
    if (required == NULL || copyout == NULL || routine == NULL)
        return false;
    *required = false;
    def_dominates = copyout->def_block == return_block
        ? copyout->def_instruction < return_instruction
        : dominates(routine, copyout->def_block, return_block);
    if (!def_dominates)
        return true;
    restore_dominates = copyout->restore_block == return_block
        ? copyout->restore_instruction < return_instruction
        : dominates(routine, copyout->restore_block, return_block);
    if (restore_dominates)
        return true;
    bool restore_reaches = block_reaches(routine,
        copyout->restore_block, return_block, &reachability_valid);
    if (!reachability_valid || restore_reaches)
        return false;
    *required = true;
    return true;
}

static bool
copyout_shape_matches(const MIRInstruction *instruction,
                      const MIRInstruction *restore)
{
    return mir_instruction_is_inout_member_copyout(instruction)
        && restore != NULL
        && instruction->ast == NULL
        && instruction->expr0 == restore->expr0
        && instruction->expr1 == restore->expr1
        && instruction->arg1 != NULL && restore->arg1 != NULL
        && strcmp(instruction->arg1, restore->arg1) == 0
        && instruction->abi_type_name == restore->abi_type_name
        && instruction->type_layout == restore->type_layout
        && instruction->abi_layout_id == restore->abi_layout_id
        && instruction->has_source_location
            == restore->has_source_location
        && instruction->source_line == restore->source_line
        && instruction->source_column == restore->source_column
        && instruction->source_stable_id == restore->source_stable_id
        && instruction->source_node_type == AST_ASSIGNMENT
        && !instruction->has_source_statement_index
        && !instruction->has_source_statement_stable_id
        && !instruction->requires_source_statement_emit
        && !instruction->requires_source_local_decl_emit
        && !instruction->has_collection_ownership_receipt;
}

static MIRInstruction
make_copyout(MIRRoutine *routine, const MIRInstruction *restore)
{
    MIRInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.id = routine->instruction_count++;
    instruction.kind = MIR_INST_ASSIGN;
    instruction.name = MIR_INOUT_MEMBER_COPYOUT;
    instruction.arg1 = restore->arg1;
    instruction.expr0 = restore->expr0;
    instruction.expr1 = restore->expr1;
    instruction.abi_type_name = restore->abi_type_name;
    instruction.type_layout = restore->type_layout;
    instruction.abi_layout_id = restore->abi_layout_id;
    instruction.has_source_location = restore->has_source_location;
    instruction.source_line = restore->source_line;
    instruction.source_column = restore->source_column;
    instruction.source_stable_id = restore->source_stable_id;
    instruction.source_node_type = AST_ASSIGNMENT;
    return instruction;
}

static bool
process_copyouts(MIRRoutine *mutable_routine, const MIRRoutine *routine,
                 bool issue, char **error_message)
{
    size_t expected = 0;
    size_t actual = 0;

    if (routine == NULL || (issue && mutable_routine != routine))
        return false;
    for (size_t b = 0; b < routine->block_count; b++)
        for (size_t i = 0; i < routine->blocks[b].instruction_count; i++)
            actual += mir_instruction_is_inout_member_copyout(
                &routine->blocks[b].instructions[i]);
    if (issue && actual != 0)
        return copyout_error(routine, error_message,
            "already materialized before synthesis", 0);

    for (size_t f = 0; f < routine->collection_ownership_fact_count; f++) {
        const MIRCollectionOwnershipFact *fact =
            &routine->collection_ownership_facts[f];
        MIRInoutMemberCopyout copyout;
        if (fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != PGY_COLLECTION_DISPOSITION_RETIRED
            || !routine_has_inout_parameter(
                routine, fact->source_binding_syntax_id))
            continue;
        if (!find_copyout_source(routine, fact, &copyout))
            return copyout_error(routine, error_message,
                "missing exact extraction or restoration", fact->binding_syntax_id);
        MIRInstruction restore =
            routine->blocks[copyout.restore_block]
                .instructions[copyout.restore_instruction];
        for (size_t b = 0; b < routine->block_count; b++) {
            const MIRBasicBlock *block = &routine->blocks[b];
            for (size_t i = 0; i < block->instruction_count; i++) {
                if (block->instructions[i].kind != MIR_INST_RETURN)
                    continue;
                bool required = false;
                if (!copyout_required(routine, &copyout, b, i, &required))
                    return copyout_error(routine, error_message,
                        "path-ambiguous", fact->binding_syntax_id);
                if (!required)
                    continue;
                expected++;
                if (issue) {
                    MIRBasicBlock *mutable_block =
                        &mutable_routine->blocks[b];
                    MIRInstruction instruction =
                        make_copyout(mutable_routine, &restore);
                    if (!insert_instruction(mutable_block, i,
                            instruction)) {
                        mutable_routine->instruction_count--;
                        return copyout_error(routine, error_message,
                            "not materialized", fact->binding_syntax_id);
                    }
                    block = mutable_block;
                    i++;
                    continue;
                }
                size_t matches = 0;
                for (size_t p = 0; p < i; p++)
                    matches += copyout_shape_matches(
                        &block->instructions[p], &restore);
                if (matches != 1)
                    return copyout_error(routine, error_message,
                        "missing, duplicated, or redirected",
                        fact->binding_syntax_id);
            }
        }
    }
    if (!issue && actual != expected)
        return copyout_error(routine, error_message,
            "orphaned or duplicated", 0);
    return true;
}

bool
mir_synthesize_inout_member_copyouts(MIRRoutine *routine,
                                     char **error_message)
{
    return process_copyouts(routine, routine, true, error_message);
}

bool
mir_validate_inout_member_copyouts(const MIRRoutine *routine,
                                   char **error_message)
{
    return process_copyouts(NULL, routine, false, error_message);
}
