#ifndef PGY_SRC_COMPILER_MIR_CFG_CONTRACT_ROOTS_H
#define PGY_SRC_COMPILER_MIR_CFG_CONTRACT_ROOTS_H

#include "mir.h"

bool mir_validate_cfg_contract_roots(const MIRRoutine *routine,
                                     char **error_message);

#endif /* PGY_SRC_COMPILER_MIR_CFG_CONTRACT_ROOTS_H */
