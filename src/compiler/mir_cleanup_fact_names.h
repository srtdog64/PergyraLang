#ifndef PERGYRA_MIR_CLEANUP_FACT_NAMES_H
#define PERGYRA_MIR_CLEANUP_FACT_NAMES_H

#include "mir.h"

#define MIR_CLEANUP_FACT_EDGE "cleanup-edge"
#define MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK "cleanup-edge-from-rollback"
#define MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION "cleanup-edge-from-invalidation"
#define MIR_CLEANUP_FACT_PIN_UNPIN_EDGE "pin-unpin-cleanup-edge"

#define MIR_CLEANUP_FACT_ANCHOR "cleanup"
#define MIR_PIN_CLEANUP_ACCESS_READ "read"
#define MIR_PIN_CLEANUP_ACCESS_WRITE "write"

static inline const char *
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

#endif
