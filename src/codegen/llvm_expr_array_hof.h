#ifndef PGY_LLVM_EXPR_ARRAY_HOF_H
#define PGY_LLVM_EXPR_ARRAY_HOF_H

#include "llvm_internal.h"

bool llvm_array_emit_hof(ASTNode *node,
                         LLVMGenCtx *ctx,
                         const char *callee_name,
                         LLVMValueRef arr_alloca,
                         LLVMArrayVarEntry *entry,
                         const char *suffix,
                         int is_filter,
                         LLVMValueRef *out);

#endif /* PGY_LLVM_EXPR_ARRAY_HOF_H */
