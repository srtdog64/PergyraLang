#ifndef PGY_LLVM_MIR_LIFECYCLE_EMIT_H
#define PGY_LLVM_MIR_LIFECYCLE_EMIT_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir.h"

void llvm_mir_emit_lifecycle_guard(const MIRInstruction *inst,
                                   LLVMGenCtx *ctx);

#endif

#endif
