#ifndef PGY_LLVM_EXPR_HOST_SPAWN_LITERAL_HELPERS_H
#define PGY_LLVM_EXPR_HOST_SPAWN_LITERAL_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_projection_from_binding(LLVMGenCtx *ctx,
                                               const char *target_class_name,
                                               const char *source_name);

#endif
