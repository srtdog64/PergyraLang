#ifndef PGY_LLVM_MIR_LOCAL_ARRAY_REGISTRY_H
#define PGY_LLVM_MIR_LOCAL_ARRAY_REGISTRY_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool llvm_mir_register_source_local_array_fact(const MIRRoutine *routine,
                                               LLVMGenCtx *ctx,
                                               const MIRInstruction *inst,
                                               const char *base_name,
                                               ASTNode *value_expr,
                                               LLVMValueRef alloca);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_MIR_LOCAL_ARRAY_REGISTRY_H */
