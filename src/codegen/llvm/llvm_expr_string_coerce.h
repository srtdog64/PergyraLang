#ifndef PGY_LLVM_EXPR_STRING_COERCE_H
#define PGY_LLVM_EXPR_STRING_COERCE_H

#include "llvm_internal.h"

LLVMValueRef llvm_coerce_value_to_string(LLVMValueRef value, LLVMGenCtx *ctx);

#endif
