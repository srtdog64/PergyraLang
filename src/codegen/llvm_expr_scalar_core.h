#ifndef PGY_LLVM_EXPR_SCALAR_CORE_H
#define PGY_LLVM_EXPR_SCALAR_CORE_H

#include "llvm_internal.h"

LLVMTypeRef llvm_function_signature_from_callable_entry(
    LLVMGenCtx *ctx, const LLVMCallableVarEntry *entry);

LLVMValueRef llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx);

LLVMValueRef llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx);

#endif
