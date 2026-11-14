#ifndef PGY_LLVM_EXPR_DOMAIN_QUERY_CALLS_H
#define PGY_LLVM_EXPR_DOMAIN_QUERY_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_domain_query_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name,
                                 LLVMValueRef *out);

#endif /* PGY_LLVM_EXPR_DOMAIN_QUERY_CALLS_H */
