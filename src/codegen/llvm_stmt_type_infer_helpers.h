#ifndef PGY_LLVM_STMT_TYPE_INFER_HELPERS_H
#define PGY_LLVM_STMT_TYPE_INFER_HELPERS_H

#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

LLVMTypeRef llvm_stmt_infer_builtin_return_type(LLVMGenCtx *ctx,
                                                const char *callee);
LLVMFuncEntry *llvm_stmt_lookup_visible_function(LLVMGenCtx *ctx,
                                                 const char *callee);
LLVMTypeRef llvm_stmt_lookup_qualified_call_return_type(LLVMGenCtx *ctx,
                                                        const char *owner,
                                                        const char *member);
LLVMTypeRef llvm_stmt_lookup_declared_call_return_type(LLVMGenCtx *ctx,
                                                       const char *callee);
LLVMTypeRef llvm_stmt_promote_numeric_type(LLVMGenCtx *ctx,
                                           LLVMTypeRef left_ty,
                                           LLVMTypeRef right_ty);
bool llvm_stmt_call_is_slot_builtin(const char *callee);
bool llvm_stmt_slot_call_returns_value(const char *callee);
bool llvm_stmt_call_returns_collection_value(const char *callee);
const char *llvm_stmt_lookup_collection_get_inner(LLVMGenCtx *ctx,
                                                  const char *callee,
                                                  const char *collection);
const char *llvm_stmt_lookup_slot_or_view_inner(LLVMGenCtx *ctx,
                                                const char *receiver_name);
LLVMTypeRef llvm_stmt_infer_await_expr_type(LLVMGenCtx *ctx, ASTNode *expr);
LLVMTypeRef llvm_stmt_unknown_expr_type(LLVMGenCtx *ctx,
                                        ASTNode *expr,
                                        const char *reason);
LLVMTypeRef llvm_stmt_infer_call_expr_type(LLVMGenCtx *ctx, ASTNode *expr);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_STMT_TYPE_INFER_HELPERS_H */
