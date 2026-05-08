#ifndef PGY_LLVM_EXPR_CALL_DISPATCH_H
#define PGY_LLVM_EXPR_CALL_DISPATCH_H

#include "llvm_internal.h"

LLVMValueRef llvm_call_error_recovery(LLVMGenCtx *ctx,
                                      ASTNode *node,
                                      const char *message);
LLVMValueRef llvm_call_arg_error_recovery(LLVMGenCtx *ctx,
                                          ASTNode *node,
                                          const char *callee_name,
                                          size_t arg_index);
LLVMValueRef llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx);

#endif
