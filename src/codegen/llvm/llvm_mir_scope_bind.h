#ifndef PGY_LLVM_MIR_SCOPE_BIND_H
#define PGY_LLVM_MIR_SCOPE_BIND_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

void llvm_mir_bind_base_local_scope(LLVMGenCtx *ctx,
                                    const char *base_name,
                                    LLVMValueRef alloca,
                                    LLVMTypeRef type,
                                    const char *type_name);

#endif
