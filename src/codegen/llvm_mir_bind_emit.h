#ifndef PGY_LLVM_MIR_BIND_EMIT_H
#define PGY_LLVM_MIR_BIND_EMIT_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>

#include "llvm_internal.h"
#include "../compiler/mir.h"

bool llvm_mir_emit_bind_statement(const MIRInstruction *inst,
                                  LLVMGenCtx *ctx);

#endif

#endif
