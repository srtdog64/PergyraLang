#ifndef PGY_MIR_CFG_CONTRACT_VALIDATE_CLEANUP_H
#define PGY_MIR_CFG_CONTRACT_VALIDATE_CLEANUP_H

#include <stdbool.h>
#include <stddef.h>

#include "mir.h"

bool mir_cfg_contract_validate_cleanup_block_shape(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message);
bool mir_cfg_contract_validate_reachable_cleanup_edges(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message);
bool mir_cfg_contract_validate_exceptional_root_presence(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message);
bool mir_cfg_contract_validate_exceptional_targets(
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    size_t block_index,
    char **error_message);
bool mir_cfg_contract_validate_cleanup_convergence(
    const MIRRoutine *routine,
    char **error_message);

#endif /* PGY_MIR_CFG_CONTRACT_VALIDATE_CLEANUP_H */
