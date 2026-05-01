#ifndef PGY_LLVM_STMT_TYPE_INFER_HELPERS_H
#define PGY_LLVM_STMT_TYPE_INFER_HELPERS_H

#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

LLVMTypeRef llvm_stmt_infer_scalar_builtin_type(LLVMGenCtx *ctx,
                                                const char *callee);
bool llvm_stmt_call_is_slot_builtin(const char *callee);
bool llvm_stmt_slot_call_returns_value(const char *callee);
bool llvm_stmt_call_returns_collection_size(const char *callee);
bool llvm_stmt_call_returns_collection_bool(const char *callee);
bool llvm_stmt_call_returns_collection_value(const char *callee);
bool llvm_stmt_call_returns_domain_bool(const char *callee);
const char *llvm_stmt_lookup_collection_get_inner(LLVMGenCtx *ctx,
                                                  const char *callee,
                                                  const char *collection);
const char *llvm_stmt_lookup_slot_or_view_inner(LLVMGenCtx *ctx,
                                                const char *receiver_name);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_LLVM_STMT_TYPE_INFER_HELPERS_H */
