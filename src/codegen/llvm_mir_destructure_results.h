#ifndef PGY_LLVM_MIR_DESTRUCTURE_RESULTS_H
#define PGY_LLVM_MIR_DESTRUCTURE_RESULTS_H
#ifdef PGY_LLVM_ENABLED
#include "llvm_mir_vars.h"

bool llvm_mir_allocate_destructure_results(const MIRRoutine *routine,
    LLVMGenCtx *ctx, LLVMMirVar **vars, size_t *capacity, size_t *count);
bool llvm_mir_store_destructure_results(const MIRInstruction *inst,
    LLVMGenCtx *ctx, LLVMMirVar *vars, size_t count);

#endif
#endif
