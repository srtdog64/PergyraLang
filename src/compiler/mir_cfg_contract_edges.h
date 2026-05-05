#ifndef PERGYRA_MIR_CFG_CONTRACT_EDGES_H
#define PERGYRA_MIR_CFG_CONTRACT_EDGES_H

#include "mir.h"

bool mir_validate_cfg_contract_block_edges(const MIRRoutine *routine,
                                           size_t block_index,
                                           char **error_message);

#endif /* PERGYRA_MIR_CFG_CONTRACT_EDGES_H */
