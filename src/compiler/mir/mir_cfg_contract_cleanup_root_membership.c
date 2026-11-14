#include "mir_cfg_contract_cleanup_root_membership.h"

bool
mir_cleanup_block_is_registered_root(const MIRRoutine *routine,
                                     size_t block_index)
{
    if (routine == NULL)
        return false;
    return (routine->has_cleanup_block && routine->cleanup_block == block_index)
        || (routine->has_rollback_block && routine->rollback_block == block_index)
        || (routine->has_invalidation_block
            && routine->invalidation_block == block_index);
}
