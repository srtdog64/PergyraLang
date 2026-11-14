#ifndef PGY_LLVM_EXPR_COLLECTION_BASE_CALLS_H
#define PGY_LLVM_EXPR_COLLECTION_BASE_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_collection_base_call(ASTNode *node,
                                    LLVMGenCtx *ctx,
                                    const char *callee_name,
                                    LLVMValueRef *out);

#endif
