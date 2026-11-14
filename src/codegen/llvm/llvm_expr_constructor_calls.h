#ifndef PGY_LLVM_EXPR_CONSTRUCTOR_CALLS_H
#define PGY_LLVM_EXPR_CONSTRUCTOR_CALLS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_constructor_call(ASTNode *node, LLVMGenCtx *ctx,
                                        const char *callee_name);

#endif
