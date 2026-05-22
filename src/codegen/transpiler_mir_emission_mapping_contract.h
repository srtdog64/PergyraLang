#ifndef PGY_TRANSPILER_MIR_EMISSION_MAPPING_CONTRACT_H
#define PGY_TRANSPILER_MIR_EMISSION_MAPPING_CONTRACT_H

#include "transpiler.h"

bool transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                                   const MIRRoutine *routine,
                                                   const ASTNode *func_decl,
                                                   bool require_non_cleanup,
                                                   char *reason,
                                                   size_t reason_cap);

#endif /* PGY_TRANSPILER_MIR_EMISSION_MAPPING_CONTRACT_H */
