#ifndef PERGYRA_MIR_VALIDATION_H
#define PERGYRA_MIR_VALIDATION_H

#include "mir.h"

bool mir_validate_block_liveness_sets(const MIRRoutine *routine,
                                      const MIRBasicBlock *block,
                                      size_t block_index,
                                      char **error_message);
bool mir_validate_instruction_uses(const MIRRoutine *routine,
                                   const MIRBasicBlock *block,
                                   size_t block_index,
                                   char **error_message);

#endif /* PERGYRA_MIR_VALIDATION_H */
