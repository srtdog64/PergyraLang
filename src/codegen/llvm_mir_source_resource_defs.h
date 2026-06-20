#ifndef PGY_LLVM_MIR_SOURCE_RESOURCE_DEFS_H
#define PGY_LLVM_MIR_SOURCE_RESOURCE_DEFS_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>

#include "llvm_internal.h"
#include "../compiler/mir.h"

bool llvm_mir_try_emit_source_resource_let(const MIRInstruction *inst,
                                           LLVMValueRef alloca,
                                           LLVMGenCtx *ctx,
                                           const char *expected_type_name,
                                           bool *handled);

#endif

#endif
