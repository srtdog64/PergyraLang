#ifndef PGY_LLVM_EXPR_TASK_CHANNEL_CALLS_H
#define PGY_LLVM_EXPR_TASK_CHANNEL_CALLS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_task_channel_call(ASTNode *node, LLVMGenCtx *ctx,
                                         const char *callee_name);

#endif
