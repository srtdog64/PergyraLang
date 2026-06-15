#ifndef PGY_LLVM_EXPR_BOX_ARRAY_CALLS_H
#define PGY_LLVM_EXPR_BOX_ARRAY_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_box_array_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                                      const char *callee_name,
                                      LLVMValueRef *out);

#endif /* PGY_LLVM_EXPR_BOX_ARRAY_CALLS_H */
