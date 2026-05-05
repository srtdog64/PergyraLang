#ifndef PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H
#define PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_result_option_call(ASTNode *node, LLVMGenCtx *ctx,
                                          const char *callee_name);

#endif /* PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H */
