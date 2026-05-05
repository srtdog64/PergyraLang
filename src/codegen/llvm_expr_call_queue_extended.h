#ifndef PGY_LLVM_EXPR_CALL_QUEUE_EXTENDED_H
#define PGY_LLVM_EXPR_CALL_QUEUE_EXTENDED_H

#include "llvm_internal.h"

bool llvm_emit_queue_extended_call(ASTNode *node,
                                   LLVMGenCtx *ctx,
                                   const char *callee_name,
                                   LLVMValueRef *out);

#endif
