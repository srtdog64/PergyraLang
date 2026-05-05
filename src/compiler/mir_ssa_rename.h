#ifndef PERGYRA_MIR_SSA_RENAME_H
#define PERGYRA_MIR_SSA_RENAME_H

#include "mir.h"

bool mir_apply_ssa_rename(MIRRoutine *routine);
bool mir_populate_use_edges(MIRRoutine *routine);

#endif
