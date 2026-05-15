#ifndef PGY_LLVM_EXPR_TASK_CALLS_H
#define PGY_LLVM_EXPR_TASK_CALLS_H

#include "llvm_internal.h"

bool llvm_is_task_runtime_builtin_name(const char *callee_name);
LLVMValueRef llvm_emit_task_runtime_call(ASTNode *node, LLVMGenCtx *ctx,
                                         const char *callee_name);

#endif
