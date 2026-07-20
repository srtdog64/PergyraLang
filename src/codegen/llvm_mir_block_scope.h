#ifndef PGY_LLVM_MIR_BLOCK_SCOPE_H
#define PGY_LLVM_MIR_BLOCK_SCOPE_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

void llvm_mir_seed_block_entry_scope(const MIRBasicBlock *mir_block,
                                     LLVMGenCtx *ctx,
                                     LLVMMirVar *vars,
                                     size_t var_count);
void llvm_mir_seed_block_phi_scope(const MIRBasicBlock *mir_block,
                                   LLVMGenCtx *ctx,
                                   LLVMMirVar *vars,
                                   size_t var_count);

#endif
