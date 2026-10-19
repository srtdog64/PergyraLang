#ifndef PGY_LLVM_EXPR_ARRAY_CALLS_H
#define PGY_LLVM_EXPR_ARRAY_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_array_builtin_call(ASTNode *node,
                                  LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out);

/* The runtime kernel suffix of an Array value's element type, or NULL when
 * the runtime has no kernels for it. */
const char *llvm_array_value_suffix(LLVMGenCtx *ctx, LLVMValueRef array);

bool llvm_emit_array_clone_value(ASTNode *node,
                                 LLVMGenCtx *ctx,
                                 LLVMValueRef value,
                                 LLVMValueRef *out);

#endif
