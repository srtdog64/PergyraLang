#ifndef PGY_MIR_LOOP_COLLECTION_CLEANUP_H
#define PGY_MIR_LOOP_COLLECTION_CLEANUP_H

#include "../mir.h"

bool mir_synthesize_loop_collection_cleanup(MIRRoutine *routine, char **error);
bool mir_validate_loop_collection_cleanup(const MIRRoutine *routine, char **error);
bool mir_loop_collection_cleanup_edge(const MIRBasicBlock *block,
    uint32_t binding, size_t target);
/* Borrow the ABI fact issued with a validated scalar retirement. */
const MIRResourceRuntimeRow *mir_collection_retirement_runtime_row(
    const MIRInstruction *instruction);

#endif
