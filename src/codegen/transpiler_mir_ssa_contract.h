#ifndef PGY_TRANSPILER_MIR_SSA_CONTRACT_H
#define PGY_TRANSPILER_MIR_SSA_CONTRACT_H

#include "transpiler_mir_ssa_map.h"

bool transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                              size_t inst_index,
                                              const ASTNode *expr,
                                              TranspilerSSANameMap *ssa_map_out);
bool transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                        const ASTNode *expr,
                                        const TranspilerSSANameMap *ssa_map,
                                        const char *routine_name,
                                        char *reason,
                                        size_t reason_cap);

#endif /* PGY_TRANSPILER_MIR_SSA_CONTRACT_H */
