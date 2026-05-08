#ifndef PERGYRA_LLVM_EXPR_EVENT_CALLS_H
#define PERGYRA_LLVM_EXPR_EVENT_CALLS_H

bool llvm_emit_event_invocation_call(ASTNode *node, LLVMGenCtx *ctx,
                                     const char *callee_name,
                                     LLVMValueRef *out);
LLVMValueRef llvm_emit_event_subscribe_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_event_unsubscribe_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_event_invoke_expr(ASTNode *node, LLVMGenCtx *ctx);

#endif /* PERGYRA_LLVM_EXPR_EVENT_CALLS_H */
