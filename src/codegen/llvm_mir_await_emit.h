#ifndef PGY_LLVM_MIR_AWAIT_EMIT_H
#define PGY_LLVM_MIR_AWAIT_EMIT_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

bool llvm_mir_try_emit_await_local_def(const MIRInstruction *inst,
                                       const MIRBasicBlock *mir_block,
                                       const MIRRoutine *routine,
                                       LLVMGenCtx *ctx,
                                       LLVMMirVar *vars,
                                       size_t var_count,
                                       bool *handled);

#endif
