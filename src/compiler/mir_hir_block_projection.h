#ifndef PGY_MIR_HIR_BLOCK_PROJECTION_H
#define PGY_MIR_HIR_BLOCK_PROJECTION_H

#include "mir.h"

/* Project one HIR routine's CFG topology and source facts into MIR blocks. */
bool mir_build_blocks_from_hir(MIRRoutine *routine,
                               const HIRRoutine *hir_routine);

#endif
