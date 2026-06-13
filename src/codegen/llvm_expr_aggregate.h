#ifndef PGY_LLVM_EXPR_AGGREGATE_H
#define PGY_LLVM_EXPR_AGGREGATE_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_emit_tuple_literal_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_array_literal_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_map_literal_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_cast_expr(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_array_access_expr(ASTNode *node, LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_EXPR_AGGREGATE_H */
