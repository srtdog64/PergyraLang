#ifndef PGY_MIR_LOWER_POPULATION_H
#define PGY_MIR_LOWER_POPULATION_H

#include "mir.h"

bool mir_populate_instructions(MIRRoutine *routine);
bool mir_link_resource_runtime_facts(MIRRoutine *routine);

#endif
