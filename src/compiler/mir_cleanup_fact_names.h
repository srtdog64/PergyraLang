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

const char *mir_cleanup_edge_fact_name_for_block(const MIRRoutine *routine,
                                                 size_t block_index);

#endif
