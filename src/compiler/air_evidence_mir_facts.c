#include <stdint.h>
#include <string.h>

#include "air_internal.h"
#include "mir_cfg_contract_cleanup_fact.h"
#include "mir_cfg_contract_cleanup_root_membership.h"

bool
air_mir_cleanup_root_is_valid(const MIRRoutine *routine)
{
    return routine != NULL
        && routine->blocks != NULL
        && routine->has_cleanup_block
        && routine->cleanup_block < routine->block_count
        && routine->blocks[routine->cleanup_block].is_cleanup
        && routine->blocks[routine->cleanup_block].is_reachable;
}

size_t
air_mir_routine_cleanup_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (!air_mir_cleanup_root_is_valid(routine))
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        const bool registered_cleanup_root =
            mir_cleanup_block_is_registered_root(routine, i);

        if (block->is_cleanup && !registered_cleanup_root)
            continue;
        if (block->is_reachable
            && !block->is_cleanup
            && block->has_cleanup_succ
            && block->cleanup_succ == routine->cleanup_block
            && air_mir_cleanup_root_is_valid(routine)
            && mir_block_has_expected_cleanup_edge_fact(routine, i)) {
            count++;
            continue;
        }
        if (block->is_reachable
            && !block->is_cleanup
            && mir_block_has_expected_cleanup_edge_fact(routine, i))
            count++;
        if (routine->has_rollback_block
            && i == routine->rollback_block
            && mir_block_has_expected_cleanup_edge_fact(routine, i))
            count++;
        if (routine->has_invalidation_block
            && i == routine->invalidation_block
            && mir_block_has_expected_cleanup_edge_fact(routine, i))
            count++;
    }
    return count;
}

static bool
air_mir_instruction_has_terminator_provenance(const MIRInstruction *inst)
{
    if (inst == NULL
        || !mir_instruction_has_source_terminator_kind(inst))
        return false;
    if (inst->kind == MIR_INST_BRANCH)
        return mir_instruction_source_terminator_matches(
            inst, HIR_BLOCK_BRANCH);
    if (inst->kind == MIR_INST_RETURN)
        return mir_instruction_source_terminator_matches(
            inst, HIR_BLOCK_RETURN);
    return false;
}

const char *
air_mir_routine_provider_name(const MIRRoutine *routine)
{
    if (routine == NULL)
        return NULL;
    if (!air_name_is_empty(routine->name))
        return routine->name;
    if (!air_name_is_empty(routine->owner_name))
        return routine->owner_name;
    return NULL;
}

size_t
air_mir_routine_terminator_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->instruction_count > 0 && block->instructions == NULL)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            if (air_mir_instruction_has_terminator_provenance(
                    &block->instructions[j])) {
                count++;
            }
        }
    }
    if (count > 0)
        return count;
    if (routine->hir_routine == NULL)
        return 0;

    /*
     * Void fallthrough routines do not materialize a branch/return instruction,
     * but they still carry HIR-backed reachable blocks. Strict AIR needs proof
     * that MIR owns CFG exit provenance; it must not force source authors to add
     * explicit `return;` solely to satisfy evidence accounting.
     */
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->is_reachable
            && !block->is_cleanup
            && mir_block_has_hir_source_mapping(block)) {
            count++;
        }
    }
    return count;
}

size_t
air_mir_routine_select_receive_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->instruction_count > 0 && block->instructions == NULL)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            if (mir_instruction_uses_select_receive_statement_emit(
                    &block->instructions[j])) {
                count++;
            }
        }
    }
    return count;
}

/*
 * Count the "unproven retain" sites in a routine: lifecycle guard CHECK
 * instructions. A CHECK guard is emitted only where the static lifecycle
 * analyzer could NOT discharge the precondition (an ambiguous control-flow
 * join), so each one is retained runtime evidence the analysis *failed to
 * erase* - bucket C (improvable), distinct from a boundary that is inherently
 * runtime-visible (bucket A) or kept as a policy summary (bucket B). A SET guard
 * records a proven transition and is not counted (it carries no unproven cost).
 */
size_t
air_mir_routine_unproven_retain_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->instruction_count > 0 && block->instructions == NULL)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (mir_instruction_has_lifecycle_guard(inst)
                && mir_instruction_lifecycle_guard_kind(inst)
                       == MIR_LIFECYCLE_GUARD_CHECK) {
                count++;
            }
        }
    }
    return count;
}

/*
 * Count the "inherent concurrency retain" sites in a routine: parallel/async/
 * spawn blocks and channel send/receive operations. Concurrency is a runtime
 * coordination fact that no analysis can erase (it bottoms out in pthread
 * mutex/cond), so each site is an INHERENT / bucket-A retain. Counting these
 * lets AIR *declare* the concurrency residue program-wide even when a bare
 * `parallel{}`/`channel` is not an intent-step boundary node, closing the
 * declared-vs-measured gap the erasure dashboard records for such programs.
 */
size_t
air_mir_routine_inherent_concurrency_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->instruction_count > 0 && block->instructions == NULL)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (mir_instruction_source_matches_ast_type(inst, AST_PARALLEL_BLOCK)
                || mir_instruction_source_matches_ast_type(inst, AST_ASYNC_BLOCK)
                || mir_instruction_source_matches_ast_type(inst, AST_SPAWN_EXPR)
                || mir_instruction_source_matches_ast_type(inst, AST_CHANNEL_SEND)
                || mir_instruction_source_matches_ast_type(inst, AST_CHANNEL_RECV)
                || mir_instruction_uses_channel_receive_statement_emit(inst)
                || mir_instruction_uses_select_receive_statement_emit(inst)) {
                count++;
            }
        }
    }
    return count;
}

static bool
air_mir_abi_type_is_capability_slot(const char *abi_type_name)
{
    return abi_type_name != NULL
        && (strncmp(abi_type_name, "SecureSlot<", 11) == 0
            || strncmp(abi_type_name, "DeviceSlot<", 11) == 0);
}

static bool
air_mir_instruction_is_slot_capability_retain(const MIRInstruction *inst)
{
    const char *abi_type_name = inst != NULL && inst->type_layout != NULL
        ? inst->type_layout->abi_type_name
        : NULL;

    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && air_mir_abi_type_is_capability_slot(abi_type_name);
}

/*
 * Count bucket-B slot capability retains. These are policy-visible
 * SecureSlot/DeviceSlot operations declared from MIR's canonical ABI layout
 * fact. The C/LLVM backends may optimize a local token check away, but when a
 * method or boundary forces it to survive, AIR must already have declared that
 * residue instead of learning about it from the physical nm pass.
 */
size_t
air_mir_routine_slot_capability_retain_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->instruction_count > 0 && block->instructions == NULL)
            continue;
        for (size_t j = 0; j < block->instruction_count; j++) {
            if (air_mir_instruction_is_slot_capability_retain(
                    &block->instructions[j])) {
                count++;
            }
        }
    }
    return count;
}

AIREvidenceKind
air_mir_cleanup_evidence_kind(void)
{
    return AIR_EVIDENCE_MIR_CLEANUP;
}

AIREvidenceKind
air_mir_terminator_evidence_kind(void)
{
    return AIR_EVIDENCE_MIR_TERMINATOR;
}

AIREvidenceKind
air_mir_select_receive_evidence_kind(void)
{
    return AIR_EVIDENCE_MIR_SELECT_RECEIVE;
}
