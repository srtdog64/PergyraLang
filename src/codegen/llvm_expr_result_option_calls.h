#ifndef PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H
#define PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_result_option_call(ASTNode *node, LLVMGenCtx *ctx,
                                          const char *callee_name);
/* Layout owner for Some(value): a declared Option consumer, else the
 * checker-sealed type of the call. NULL when neither exists. */
LLVMTypeRef llvm_option_some_layout_type(LLVMGenCtx *ctx, ASTNode *call,
                                         const char **option_type_name_out);

#endif /* PGY_LLVM_EXPR_RESULT_OPTION_CALLS_H */
