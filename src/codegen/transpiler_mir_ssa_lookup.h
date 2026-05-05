#ifndef PERGYRA_TRANSPILER_MIR_SSA_LOOKUP_H
#define PERGYRA_TRANSPILER_MIR_SSA_LOOKUP_H

#include "transpiler.h"

const char *transpiler_find_prior_block_ssa_name(const MIRBasicBlock *block,
                                                 size_t limit_inst_index,
                                                 const char *base_name);
const char *transpiler_find_block_exit_ssa_name(const MIRBasicBlock *block,
                                                const char *base_name);
const char *transpiler_find_block_renamed_ssa_name(const MIRBasicBlock *block,
                                                   const char *base_name);
const char *transpiler_find_routine_exit_ssa_name(const MIRRoutine *mir_routine,
                                                  const char *base_name);

#endif /* PERGYRA_TRANSPILER_MIR_SSA_LOOKUP_H */
