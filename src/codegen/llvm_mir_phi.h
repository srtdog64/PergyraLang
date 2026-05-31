#ifndef PERGYRA_LLVM_MIR_PHI_H
#define PERGYRA_LLVM_MIR_PHI_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_vars.h"

void llvm_mir_emit_true_phi_nodes(const MIRRoutine *routine,
                                  LLVMGenCtx *ctx,
                                  LLVMBasicBlockRef *llvm_blocks,
                                  LLVMBasicBlockRef *llvm_block_heads,
                                  LLVMMirVar *vars,
                                  size_t var_count);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_MIR_PHI_H */
