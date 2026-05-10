#ifndef PGY_LLVM_EXPR_CALL_VARIABLE_H
#define PGY_LLVM_EXPR_CALL_VARIABLE_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_callable_variable_call(ASTNode *node,
                                              LLVMGenCtx *ctx,
                                              const char *callee_name,
                                              LLVMValueRef *args,
                                              unsigned emitted_argc);

#endif
