#ifndef PGY_SRC_COMPILER_MIR_CFG_CONTRACT_CLEANUP_ROOTS_H
#define PGY_SRC_COMPILER_MIR_CFG_CONTRACT_CLEANUP_ROOTS_H

#include <stdbool.h>

#include "mir.h"

bool mir_validate_cfg_contract_cleanup_roots(const MIRRoutine *routine,
                                             bool requires_cleanup_for_body,
                                             char **error_message);

#endif /* PGY_SRC_COMPILER_MIR_CFG_CONTRACT_CLEANUP_ROOTS_H */
