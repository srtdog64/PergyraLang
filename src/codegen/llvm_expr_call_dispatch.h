#ifndef PGY_LLVM_EXPR_CALL_DISPATCH_H
#define PGY_LLVM_EXPR_CALL_DISPATCH_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx);

#endif
