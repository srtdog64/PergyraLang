#ifndef PERGYRA_MIR_CFG_CONTRACT_CLEANUP_FACT_H
#define PERGYRA_MIR_CFG_CONTRACT_CLEANUP_FACT_H

#include "mir.h"

bool mir_block_has_cleanup_edge_fact(const MIRBasicBlock *block,
                                     const char *edge_name);

#endif
