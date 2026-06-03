#ifndef PERGYRA_LLVM_MIR_ASYNC_FACT_H
#define PERGYRA_LLVM_MIR_ASYNC_FACT_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMTypeRef llvm_mir_async_fact_type_from_channel_recv(
    const MIRRoutine *routine,
    LLVMGenCtx *ctx,
    const MIRInstruction *inst);
LLVMTypeRef llvm_mir_async_fact_type_from_await(const MIRRoutine *routine,
                                                LLVMGenCtx *ctx,
                                                const MIRInstruction *inst);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_MIR_ASYNC_FACT_H */
