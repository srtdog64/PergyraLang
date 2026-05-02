static bool
mir_validate_cfg_contract_roots(const MIRRoutine *routine, char **error_message)
{
    if (routine->entry_block >= routine->block_count) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt("MIR routine '%s' has invalid entry block",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_cleanup_block && routine->cleanup_block >= routine->block_count) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt("MIR routine '%s' has invalid cleanup block",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_rollback_block && routine->rollback_block >= routine->block_count) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt("MIR routine '%s' has invalid rollback block",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_invalidation_block
        && routine->invalidation_block >= routine->block_count) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt("MIR routine '%s' has invalid invalidation block",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_cleanup_block && routine->entry_block == routine->cleanup_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' entry and cleanup blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_rollback_block && routine->entry_block == routine->rollback_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' entry and rollback blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_invalidation_block
        && routine->entry_block == routine->invalidation_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' entry and invalidation blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (!routine->has_cleanup_block && routine->has_rollback_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has rollback block without cleanup root",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (!routine->has_cleanup_block && routine->has_invalidation_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalidation block without cleanup root",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_cleanup_block && routine->has_rollback_block
        && routine->cleanup_block == routine->rollback_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup and rollback blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_cleanup_block && routine->has_invalidation_block
        && routine->cleanup_block == routine->invalidation_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' cleanup and invalidation blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    if (routine->has_rollback_block && routine->has_invalidation_block
        && routine->rollback_block == routine->invalidation_block) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' rollback and invalidation blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    return true;
}
