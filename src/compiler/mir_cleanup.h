#ifndef PERGYRA_MIR_CLEANUP_H
#define PERGYRA_MIR_CLEANUP_H

#include <stdbool.h>

#include "mir.h"

bool mir_add_cleanup_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op);
bool mir_append_cleanup_block(MIRRoutine *routine, const RIRScope *rir_scope);
bool mir_materialize_cleanup_edges(MIRRoutine *routine);

#endif
