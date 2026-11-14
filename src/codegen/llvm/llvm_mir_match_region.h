#ifndef PGY_LLVM_MIR_MATCH_REGION_H
#define PGY_LLVM_MIR_MATCH_REGION_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const MIRInstruction *llvm_mir_find_incoming_match_branch(
    const MIRRoutine *routine,
    const MIRBasicBlock *block);
bool llvm_mir_case_true_region_contains(const MIRRoutine *routine,
                                        const MIRBasicBlock *case_block,
                                        size_t target_id);

#endif

#endif /* PGY_LLVM_MIR_MATCH_REGION_H */
