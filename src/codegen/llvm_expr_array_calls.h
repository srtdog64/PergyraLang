#ifndef PGY_LLVM_EXPR_ARRAY_CALLS_H
#define PGY_LLVM_EXPR_ARRAY_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_array_builtin_call(ASTNode *node,
                                  LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out);

bool llvm_emit_array_clone_value(ASTNode *node,
                                 LLVMGenCtx *ctx,
                                 LLVMValueRef value,
                                 LLVMValueRef *out);

#endif
