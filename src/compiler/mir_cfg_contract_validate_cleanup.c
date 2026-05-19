#include "mir_cfg_contract_validate_cleanup.h"

#include "mir_base_helpers.h"
#include "mir_cfg_contract_cleanup_fact.h"
#include "mir_cfg_contract_cleanup_root_membership.h"
#include "mir_cfg_contract_pin.h"

static const char *
mir_cfg_contract_cleanup_routine_name(const MIRRoutine *routine)
{
    return routine != NULL && routine->name != NULL
        ? routine->name
        : "(anonymous)";
}

bool
mir_cfg_contract_validate_cleanup_block_shape(const MIRRoutine *routine,
                                              const MIRBasicBlock *block,
                                              size_t block_index,
                                              char **error_message)
{
    if (!block->is_cleanup)
        return true;

    if (block->has_succ_true || block->has_succ_false) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup block[%zu] must not have normal CFG successors",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (!mir_cleanup_block_is_registered_root(routine, block_index)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup block[%zu] is not registered as a cleanup root",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (block->is_pin_region) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup block[%zu] must not be a pin region",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    return true;
}

bool
mir_cfg_contract_validate_reachable_cleanup_edges(const MIRRoutine *routine,
                                                  const MIRBasicBlock *block,
                                                  size_t block_index,
                                                  char **error_message)
{
    if (!block->is_reachable || block->is_cleanup)
        return true;

    if (!routine->has_cleanup_block) {
        if (block->has_cleanup_succ || block->has_rollback_succ
            || block->has_invalidation_succ) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has exceptional successor without cleanup root",
                    mir_cfg_contract_cleanup_routine_name(routine),
                    block_index);
            }
            return false;
        }
        return true;
    }

    if (!block->has_cleanup_succ) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] missing cleanup edge from reachable non-cleanup block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (block->cleanup_succ >= routine->block_count
        || block->cleanup_succ != routine->cleanup_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] cleanup edge does not target cleanup block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (!mir_block_has_expected_cleanup_edge_fact(routine, block_index)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] missing cleanup-edge MIR fact",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (block->is_pin_region && !mir_block_has_pin_cleanup_edge(block)) {
        const char *pin_reason = mir_block_pin_cleanup_missing_reason(block);
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' pin-region block[%zu] missing pin-unpin cleanup fact: %s",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index,
                pin_reason != NULL ? pin_reason : "unknown pin cleanup mismatch");
        }
        return false;
    }
    return true;
}

bool
mir_cfg_contract_validate_exceptional_root_presence(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message)
{
    if (!block->is_cleanup && block->has_rollback_succ
        && !routine->has_rollback_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] has rollback successor but no rollback block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (!block->is_cleanup && block->has_invalidation_succ
        && !routine->has_invalidation_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] has invalidation successor but no invalidation block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    return true;
}

bool
mir_cfg_contract_validate_exceptional_targets(const MIRRoutine *routine,
                                              const MIRBasicBlock *block,
                                              size_t block_index,
                                              char **error_message)
{
    if (routine->has_rollback_block && block->has_rollback_succ
        && block->rollback_succ != routine->rollback_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] rollback successor must target routine rollback block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    if (routine->has_invalidation_block && block->has_invalidation_succ
        && block->invalidation_succ != routine->invalidation_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] invalidation successor must target routine invalidation block",
                mir_cfg_contract_cleanup_routine_name(routine),
                block_index);
        }
        return false;
    }
    return true;
}

bool
mir_cfg_contract_validate_cleanup_convergence(const MIRRoutine *routine,
                                              char **error_message)
{
    const MIRBasicBlock *cleanup;

    if (!routine->has_cleanup_block)
        return true;

    cleanup = &routine->blocks[routine->cleanup_block];
    if (routine->has_rollback_block
        && (!cleanup->has_rollback_succ
            || cleanup->rollback_succ != routine->rollback_block)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup block does not converge to rollback block",
                mir_cfg_contract_cleanup_routine_name(routine));
        }
        return false;
    }
    if (routine->has_invalidation_block
        && (!cleanup->has_invalidation_succ
            || cleanup->invalidation_succ != routine->invalidation_block)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup block does not converge to invalidation block",
                mir_cfg_contract_cleanup_routine_name(routine));
        }
        return false;
    }
    if (routine->has_rollback_block && routine->has_invalidation_block) {
        const MIRBasicBlock *rollback =
            &routine->blocks[routine->rollback_block];
        if (!rollback->has_invalidation_succ
            || rollback->invalidation_succ != routine->invalidation_block
            || !rollback->has_cleanup_succ
            || rollback->cleanup_succ != routine->invalidation_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' rollback block does not converge to invalidation block",
                    mir_cfg_contract_cleanup_routine_name(routine));
            }
            return false;
        }
    }
    if (routine->has_rollback_block
        && !mir_block_has_expected_cleanup_edge_fact(
            routine, routine->rollback_block)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' rollback block missing cleanup-edge MIR fact",
                mir_cfg_contract_cleanup_routine_name(routine));
        }
        return false;
    }
    if (routine->has_invalidation_block
        && !mir_block_has_expected_cleanup_edge_fact(
            routine, routine->invalidation_block)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' invalidation block missing cleanup-edge MIR fact",
                mir_cfg_contract_cleanup_routine_name(routine));
        }
        return false;
    }
    return true;
}
