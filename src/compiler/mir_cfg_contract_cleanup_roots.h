static bool
mir_validate_cfg_contract_cleanup_roots(const MIRRoutine *routine,
                                        bool requires_cleanup_for_body,
                                        char **error_message)
{
    if (routine->has_cleanup_block) {
        if (routine->cleanup_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid cleanup block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        const MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        if (!cleanup->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->cleanup_block);
            }
            return false;
        }
    }

    if (routine->has_rollback_block) {
        if (routine->rollback_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid rollback block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        const MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        if (!rollback->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' rollback block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->rollback_block);
            }
            return false;
        }
    }

    if (routine->has_invalidation_block) {
        if (routine->invalidation_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid invalidation block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        const MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        if (!invalidation->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' invalidation block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->invalidation_block);
            }
            return false;
        }
    }

    if (requires_cleanup_for_body && !routine->has_cleanup_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' requires cleanup block for exceptional flow",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }

    return true;
}
