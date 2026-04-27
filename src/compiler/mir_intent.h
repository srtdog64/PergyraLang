#ifndef PERGYRA_MIR_INTENT_H
#define PERGYRA_MIR_INTENT_H

#include <stdbool.h>

#include "mir.h"

bool mir_append_intent_invalidation_markers(MIRRoutine *routine, MIRBasicBlock *block);
bool mir_append_intent_step_instructions(MIRRoutine *routine, MIRBasicBlock *block);

#endif
