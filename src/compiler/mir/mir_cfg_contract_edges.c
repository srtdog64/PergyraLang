#include "mir_cfg_contract_edges.h"

#include "mir_base_helpers.h"

static bool
mir_validate_edge_predecessor_link(const MIRRoutine *routine,
                                   size_t source_index,
                                   size_t target_index,
                                   const char *edge_label,
                                   char **error_message)
{
    const MIRBasicBlock *target_block;

    if (routine == NULL || target_index >= routine->block_count)
        return false;

    target_block = &routine->blocks[target_index];
    if (target_block->predecessor_count > 0
        && target_block->predecessors == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] has predecessor count without predecessor inventory",
                routine->name != NULL ? routine->name : "(anonymous)",
                target_index);
        }
        return false;
    }
    if (mir_block_has_predecessor(target_block, source_index))
        return true;

    if (error_message != NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' block[%zu] missing predecessor link to %s successor %zu",
            routine->name != NULL ? routine->name : "(anonymous)",
            source_index,
            edge_label != NULL ? edge_label : "unknown",
            target_index);
    }
    return false;
}

static bool
mir_validate_successor_index(const MIRRoutine *routine,
                             size_t source_index,
                             size_t target_index,
                             const char *edge_label,
                             char **error_message)
{
    if (routine != NULL && target_index < routine->block_count)
        return true;

    if (error_message != NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' block[%zu] has invalid %s successor %zu",
            routine != NULL && routine->name != NULL ? routine->name : "(anonymous)",
            source_index,
            edge_label != NULL ? edge_label : "unknown",
            target_index);
    }
    return false;
}

static bool
mir_block_has_forward_edge_to(const MIRBasicBlock *block, size_t target_index)
{
    if (block == NULL)
        return false;
    return (block->has_succ_true && block->succ_true == target_index)
        || (block->has_succ_false && block->succ_false == target_index)
        || (block->has_cleanup_succ && block->cleanup_succ == target_index)
        || (block->has_rollback_succ && block->rollback_succ == target_index)
        || (block->has_invalidation_succ
            && block->invalidation_succ == target_index);
}

static bool
mir_validate_block_predecessors(const MIRRoutine *routine,
                                size_t block_index,
                                char **error_message)
{
    const MIRBasicBlock *block = &routine->blocks[block_index];

    if (block->predecessor_count > 0 && block->predecessors == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] has predecessor count without predecessor inventory",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }
    if (block->predecessor_count > block->predecessor_capacity) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] has predecessor count above predecessor capacity",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }

    for (size_t p = 0; p < block->predecessor_count; p++) {
        if (block->predecessors[p] == block_index) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] contains self predecessor",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index);
            }
            return false;
        }
        if (block->predecessors[p] >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid predecessor %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    block->predecessors[p]);
            }
            return false;
        }
        if (!mir_block_has_forward_edge_to(
                &routine->blocks[block->predecessors[p]], block_index)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] predecessor %zu has no matching forward edge",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    block->predecessors[p]);
            }
            return false;
        }
    }

    for (size_t p = 0; p < block->predecessor_count; p++) {
        for (size_t q = p + 1; q < block->predecessor_count; q++) {
            if (block->predecessors[p] == block->predecessors[q]) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has duplicate predecessor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        block->predecessors[p]);
                }
                return false;
            }
        }
    }
    return true;
}

bool
mir_validate_cfg_contract_block_edges(const MIRRoutine *routine,
                                      size_t block_index,
                                      char **error_message)
{
    const MIRBasicBlock *block = &routine->blocks[block_index];

    if (block->predecessor_count > 0
        && !mir_validate_block_predecessors(routine, block_index, error_message))
        return false;

    if (block->has_succ_true
        && !mir_validate_successor_index(routine, block_index, block->succ_true,
                                         "true", error_message))
        return false;
    if (block->has_succ_false
        && !mir_validate_successor_index(routine, block_index, block->succ_false,
                                         "false", error_message))
        return false;
    if (block->has_cleanup_succ
        && !mir_validate_successor_index(routine, block_index, block->cleanup_succ,
                                         "cleanup", error_message))
        return false;
    if (block->has_rollback_succ
        && !mir_validate_successor_index(routine, block_index, block->rollback_succ,
                                         "rollback", error_message))
        return false;
    if (block->has_invalidation_succ
        && !mir_validate_successor_index(routine, block_index,
                                         block->invalidation_succ,
                                         "invalidation", error_message))
        return false;

    if (block->has_succ_true
        && !mir_validate_edge_predecessor_link(routine, block_index,
                                               block->succ_true, "true",
                                               error_message))
        return false;
    if (block->has_succ_false
        && !mir_validate_edge_predecessor_link(routine, block_index,
                                               block->succ_false, "false",
                                               error_message))
        return false;
    if (block->has_cleanup_succ
        && !mir_validate_edge_predecessor_link(routine, block_index,
                                               block->cleanup_succ, "cleanup",
                                               error_message))
        return false;
    if (block->has_rollback_succ
        && !mir_validate_edge_predecessor_link(routine, block_index,
                                               block->rollback_succ, "rollback",
                                               error_message))
        return false;
    if (block->has_invalidation_succ
        && !mir_validate_edge_predecessor_link(routine, block_index,
                                               block->invalidation_succ,
                                               "invalidation", error_message))
        return false;

    return true;
}
