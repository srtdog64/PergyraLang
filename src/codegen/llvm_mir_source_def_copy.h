#ifndef PGY_LLVM_MIR_SOURCE_DEF_COPY_H
#define PGY_LLVM_MIR_SOURCE_DEF_COPY_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>

#include "llvm_internal.h"
#include "llvm_mir_vars.h"
#include "../compiler/mir.h"

bool llvm_mir_copy_source_def_to_versioned_local(const MIRInstruction *inst,
                                                 const MIRBasicBlock *mir_block,
                                                 LLVMGenCtx *ctx,
                                                 LLVMMirVar *vars,
                                                 size_t var_count);

#endif

#endif
