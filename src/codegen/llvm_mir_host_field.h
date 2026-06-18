#ifndef PGY_LLVM_MIR_HOST_FIELD_H
#define PGY_LLVM_MIR_HOST_FIELD_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

bool llvm_mir_copy_host_field_to_versioned_local(LLVMGenCtx *ctx,
                                                 const char *field_name,
                                                 LLVMMirVar *target);

#endif
