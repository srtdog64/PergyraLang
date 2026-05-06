#ifndef PERGYRA_MIR_NON_CFG_STMT_POPULATION_H
#define PERGYRA_MIR_NON_CFG_STMT_POPULATION_H

#include "mir.h"

bool mir_append_non_cfg_body_statements(MIRRoutine *routine,
                                        MIRBasicBlock *entry);

#endif /* PERGYRA_MIR_NON_CFG_STMT_POPULATION_H */