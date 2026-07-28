#include "mir_cfg_contract_validate.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_control.h"
#include "mir_cfg_contract_roots.h"
#include "mir_cfg_contract_cleanup_roots.h"
#include "mir_cfg_contract_edges.h"
#include "mir_cfg_contract_validate_cleanup.h"

static const char *
mir_cfg_contract_routine_name(const MIRRoutine *routine)
{
    return routine != NULL && routine->name != NULL
        ? routine->name
        : "(anonymous)";
}

static bool
mir_cfg_contract_requires_cleanup_for_body(const MIRRoutine *routine,
                                           bool require_cleanup)
{
    if (require_cleanup)
        return true;
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!block->is_cleanup && block->is_pin_region)
            return true;
    }
    return false;
}

static bool
mir_cfg_contract_block_requires_source_mapping(
    const MIRBasicBlock *block,
    bool require_cleanup_source_mapping,
    bool require_mapping_for_all_blocks)
{
    if (block == NULL)
        return false;
    if (block->is_intent_execution_plan_block)
        return false;
    if (require_mapping_for_all_blocks)
        return !block->is_cleanup;
    return block->is_reachable
        && (!block->is_cleanup || require_cleanup_source_mapping);
}

static bool
mir_cfg_contract_validate_instruction_inventory(const MIRRoutine *routine,
                                                const MIRBasicBlock *block,
                                                size_t block_index,
                                                char **error_message)
{
    if (block->instruction_count == 0 || block->instructions != NULL)
        return true;

    if (error_message != NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' block[%zu] has instruction count without instruction inventory",
            mir_cfg_contract_routine_name(routine),
            block_index);
    }
    return false;
}

static bool
mir_cfg_contract_validate_source_mapping(const MIRRoutine *routine,
                                         const MIRBasicBlock *block,
                                         size_t block_index,
                                         size_t cfg_block_count,
                                         bool *hir_block_seen,
                                         char **error_message)
{
    size_t source_id = block->source_hir_block_id;

    if (source_id == SIZE_MAX || source_id >= cfg_block_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] uses invalid source_hir_block_id",
                mir_cfg_contract_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (hir_block_seen[source_id]) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' source_hir_block_id collision on block[%zu] -> hir[%zu]",
                mir_cfg_contract_routine_name(routine),
                block_index,
                source_id);
        }
        return false;
    }
    hir_block_seen[source_id] = true;
    return true;
}

static bool
mir_cfg_contract_validate_pin_region_shape(const MIRRoutine *routine,
                                           const MIRBasicBlock *block,
                                           size_t block_index,
                                           char **error_message)
{
    if (!block->is_reachable || block->is_cleanup || !block->is_pin_region)
        return true;

    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0') {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' pin-region block[%zu] missing pin source name",
                mir_cfg_contract_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0') {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' pin-region block[%zu] missing pin view name",
                mir_cfg_contract_routine_name(routine),
                block_index);
        }
        return false;
    }
    return true;
}

static bool
mir_cfg_contract_validate_no_cfg_owned_stmt_fallback(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    size_t cfg_block_count,
    char **error_message)
{
    if (cfg_block_count == 0 || !block->is_reachable || block->is_cleanup)
        return true;

    for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
        const MIRInstruction *inst = &block->instructions[inst_id];
        if (inst->kind == MIR_INST_STMT
            && mir_instruction_source_is_cfg_owned_control(inst)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] keeps CFG-owned control statement as fallback STMT",
                    mir_cfg_contract_routine_name(routine),
                    block_index);
            }
            return false;
        }
    }
    return true;
}

static bool
mir_cfg_contract_validate_loop_fact_payloads(const MIRRoutine *routine,
                                             const MIRBasicBlock *block,
                                             size_t block_index,
                                             char **error_message)
{
    if (!block->is_reachable || block->is_cleanup
        || (!block->has_succ_true && !block->has_succ_false)) {
        return true;
    }

    for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
        const MIRInstruction *inst = &block->instructions[inst_id];
        if (inst->kind == MIR_INST_LOOP_INIT
            && (inst->arg0 == NULL || inst->expr0 == NULL
                || inst->expr1 == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has incomplete loop-init fact",
                    mir_cfg_contract_routine_name(routine),
                    block_index);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                || inst->branch_shape == MIR_BRANCH_FOR_IN)
            && (inst->arg0 == NULL || inst->expr0 == NULL
                || inst->expr1 == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has incomplete loop-branch fact",
                    mir_cfg_contract_routine_name(routine),
                    block_index);
            }
            return false;
        }
    }
    return true;
}

static bool
mir_cfg_contract_validate_unreachable_exceptional_edges(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message)
{
    if (block->is_reachable || block->is_cleanup
        || (!block->has_cleanup_succ
            && !block->has_rollback_succ
            && !block->has_invalidation_succ)) {
        return true;
    }

    if (error_message != NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' unreachable block[%zu] has exceptional successor",
            mir_cfg_contract_routine_name(routine),
            block_index);
    }
    return false;
}

static bool
mir_cfg_contract_validate_block(const MIRRoutine *routine,
                                const MIRBasicBlock *block,
                                size_t block_index,
                                size_t cfg_block_count,
                                bool *hir_block_seen,
                                bool require_cleanup_source_mapping,
                                bool require_mapping_for_all_blocks,
                                char **error_message)
{
    bool require_source_mapping =
        mir_cfg_contract_block_requires_source_mapping(
            block, require_cleanup_source_mapping,
            require_mapping_for_all_blocks);

    if (!mir_cfg_contract_validate_instruction_inventory(
            routine, block, block_index, error_message))
        return false;
    if (cfg_block_count > 0 && require_source_mapping
        && !mir_cfg_contract_validate_source_mapping(
            routine, block, block_index, cfg_block_count,
            hir_block_seen, error_message))
        return false;
    if (!mir_cfg_contract_validate_cleanup_block_shape(
            routine, block, block_index, error_message))
        return false;
    if (!mir_cfg_contract_validate_pin_region_shape(
            routine, block, block_index, error_message))
        return false;
    if (!mir_cfg_contract_validate_no_cfg_owned_stmt_fallback(
            routine, block, block_index, cfg_block_count, error_message))
        return false;
    if (!mir_cfg_contract_validate_loop_fact_payloads(
            routine, block, block_index, error_message))
        return false;
    if (!mir_cfg_contract_validate_unreachable_exceptional_edges(
            routine, block, block_index, error_message))
        return false;
    if (!mir_cfg_contract_validate_reachable_cleanup_edges(
            routine, block, block_index, error_message))
        return false;
    if (!mir_cfg_contract_validate_exceptional_root_presence(
            routine, block, block_index, error_message))
        return false;
    if (!mir_validate_cfg_contract_block_edges(
            routine, block_index, error_message))
        return false;
    return mir_cfg_contract_validate_exceptional_targets(
        routine, block, block_index, error_message);
}

bool
mir_validate_cfg_contract_state(const MIRRoutine *routine,
                                bool require_cleanup,
                                bool require_cleanup_source_mapping,
                                bool require_mapping_for_all_blocks,
                                char **error_message)
{
    bool *hir_block_seen = NULL;
    bool requires_cleanup_for_body;
    const size_t cfg_block_count =
        (routine != NULL && routine->hir_routine != NULL
         && routine->hir_routine->has_cfg)
            ? routine->hir_routine->cfg.block_count
            : 0;

    if (routine == NULL)
        return false;

    if (routine->blocks == NULL || routine->block_count == 0)
        return false;

    requires_cleanup_for_body =
        mir_cfg_contract_requires_cleanup_for_body(routine, require_cleanup);

    if (!mir_validate_cfg_contract_roots(routine, error_message))
        return false;
    if (!mir_validate_cfg_contract_cleanup_roots(
            routine, requires_cleanup_for_body, error_message))
        return false;

    if (cfg_block_count > 0) {
        hir_block_seen = calloc(cfg_block_count, sizeof(bool));
        if (hir_block_seen == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!mir_cfg_contract_validate_block(
                routine, block, i, cfg_block_count, hir_block_seen,
                require_cleanup_source_mapping,
                require_mapping_for_all_blocks,
                error_message)) {
            free(hir_block_seen);
            return false;
        }
    }

    if (!mir_cfg_contract_validate_cleanup_convergence(
            routine, error_message)) {
        free(hir_block_seen);
        return false;
    }

    free(hir_block_seen);
    return true;
}
