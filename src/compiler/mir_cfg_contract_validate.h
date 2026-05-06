#ifndef PGY_MIR_CFG_CONTRACT_VALIDATE_H
#define PGY_MIR_CFG_CONTRACT_VALIDATE_H

#include <stdbool.h>

#include "mir.h"

bool mir_validate_cfg_contract_state(const MIRRoutine *routine,
                                     bool require_cleanup,
                                     bool require_cleanup_source_mapping,
                                     bool require_mapping_for_all_blocks,
                                     char **error_message);

#endif /* PGY_MIR_CFG_CONTRACT_VALIDATE_H */