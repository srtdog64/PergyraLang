#ifndef PGY_MIR_RESOURCE_RUNTIME_POPULATION_H
#define PGY_MIR_RESOURCE_RUNTIME_POPULATION_H

#include "mir.h"

bool mir_link_resource_runtime_facts(MIRRoutine *routine);
bool mir_materialize_resource_runtime_row(MIRRoutine *routine,
                                          const char *abi_type_name,
                                          const char *operation,
                                          MIRResourceRuntimeRow *out);
bool mir_materialize_resource_runtime_fact(MIRRoutine *routine,
                                           MIRInstruction *inst);

#endif /* PGY_MIR_RESOURCE_RUNTIME_POPULATION_H */
