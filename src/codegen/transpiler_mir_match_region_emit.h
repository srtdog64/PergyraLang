#ifndef PGY_TRANSPILER_MIR_MATCH_REGION_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_REGION_EMIT_H

#include "transpiler.h"

const MIRInstruction *transpiler_mir_find_incoming_match_branch(
    const MIRRoutine *routine,
    const MIRBasicBlock *block);
bool transpiler_mir_case_true_region_contains(const MIRRoutine *routine,
                                              const MIRBasicBlock *case_block,
                                              size_t target_id);

#endif /* PGY_TRANSPILER_MIR_MATCH_REGION_EMIT_H */
