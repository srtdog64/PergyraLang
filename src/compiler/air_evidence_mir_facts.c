#include <stdint.h>

#include "air_internal.h"
#include "mir_cfg_contract_cleanup_fact.h"
#include "mir_cfg_contract_cleanup_root_membership.h"

bool
air_mir_cleanup_root_is_valid(const MIRRoutine *routine)
{
    return routine != NULL
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
