#include "mir_cleanup_fact_names.h"

const char *
mir_cleanup_edge_fact_name_for_block(const MIRRoutine *routine,
                                     size_t block_index)
{
    if (routine != NULL && routine->has_rollback_block
        && routine->rollback_block == block_index) {
        return MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK;
    }
    if (routine != NULL && routine->has_invalidation_block
        && routine->invalidation_block == block_index) {
        return MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION;
    }
    return MIR_CLEANUP_FACT_EDGE;
}
