#ifndef PGY_LLVM_MIR_STORE_COERCION_H
#define PGY_LLVM_MIR_STORE_COERCION_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_mir_coerce_value_for_store(LLVMGenCtx *ctx,
                                             LLVMValueRef value,
                                             LLVMTypeRef target_type);

#endif

#endif
