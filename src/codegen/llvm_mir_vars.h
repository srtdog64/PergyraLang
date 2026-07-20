#ifndef PERGYRA_LLVM_MIR_VARS_H
#define PERGYRA_LLVM_MIR_VARS_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

typedef struct {
    const char *mir_name;
    const char *abi_type_name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
} LLVMMirVar;

LLVMMirVar *llvm_mir_get_var_entry(LLVMMirVar *vars,
                                   size_t count,
                                   const char *name);
LLVMValueRef llvm_mir_get_var(LLVMMirVar *vars,
                              size_t count,
                              const char *name);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_MIR_VARS_H */
