#ifndef PERGYRA_MIR_CFG_CONTRACT_CLEANUP_ROOT_MEMBERSHIP_H
#define PERGYRA_MIR_CFG_CONTRACT_CLEANUP_ROOT_MEMBERSHIP_H

#include <stdbool.h>
#include <stddef.h>

#include "mir.h"

bool mir_cleanup_block_is_registered_root(const MIRRoutine *routine,
                                          size_t block_index);

#endif /* PERGYRA_MIR_CFG_CONTRACT_CLEANUP_ROOT_MEMBERSHIP_H */
