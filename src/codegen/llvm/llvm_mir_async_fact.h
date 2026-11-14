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
bool llvm_mir_async_fact_future_inner_from_source_local(
    const MIRRoutine *routine,
    const char *future_name,
    char *inner_out,
    size_t inner_out_size,
    bool *is_remote_out);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_MIR_ASYNC_FACT_H */
