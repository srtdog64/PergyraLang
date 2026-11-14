#ifndef PGY_LLVM_MIR_RESOURCE_VIEW_H
#define PGY_LLVM_MIR_RESOURCE_VIEW_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_vars.h"

bool llvm_mir_def_is_resource_view_alias(const MIRInstruction *inst);
bool llvm_mir_bind_resource_view_def_alias(const MIRInstruction *inst,
                                           const MIRBasicBlock *block,
                                           LLVMGenCtx *ctx,
                                           LLVMMirVar *vars,
                                           size_t var_count);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_MIR_RESOURCE_VIEW_H */
