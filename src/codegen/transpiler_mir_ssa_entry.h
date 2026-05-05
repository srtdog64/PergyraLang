/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA entry-map helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_SSA_ENTRY_H
#define PERGYRA_TRANSPILER_MIR_SSA_ENTRY_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                            const MIRBasicBlock *block);

#endif /* PERGYRA_TRANSPILER_MIR_SSA_ENTRY_H */
