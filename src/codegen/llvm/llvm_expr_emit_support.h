#ifndef PGY_LLVM_EXPR_EMIT_SUPPORT_H
#define PGY_LLVM_EXPR_EMIT_SUPPORT_H

#include "llvm_internal.h"

LLVMValueRef llvm_expression_error(LLVMGenCtx *ctx,
                                   ASTNode *node,
                                   const char *message);
LLVMValueRef llvm_void_expression_placeholder(LLVMGenCtx *ctx,
                                              ASTNode *node,
                                              const char *owner);
LLVMValueRef llvm_zero_value_for_type(LLVMGenCtx *ctx, LLVMTypeRef type);
bool         llvm_expr_runtime_name(LLVMGenCtx *ctx,
                                    ASTNode *node,
                                    char *out,
                                    size_t out_size,
                                    const char *prefix,
                                    const char *type_name);
bool         llvm_expr_lambda_name(LLVMGenCtx *ctx,
                                   ASTNode *node,
                                   char *out,
                                   size_t out_size,
                                   int lambda_id);

#endif
