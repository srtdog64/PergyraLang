#ifndef PERGYRA_LLVM_EXPR_MATH_CALLS_H
#define PERGYRA_LLVM_EXPR_MATH_CALLS_H

bool llvm_emit_scalar_math_call(ASTNode *node, LLVMGenCtx *ctx,
                                const char *callee_name, LLVMValueRef *out);

#endif /* PERGYRA_LLVM_EXPR_MATH_CALLS_H */
