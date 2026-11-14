#ifndef PGY_LLVM_EXPR_ARRAY_RAW_NOMINAL_CALLS_H
#define PGY_LLVM_EXPR_ARRAY_RAW_NOMINAL_CALLS_H

#include "llvm_internal.h"

bool llvm_array_entry_uses_raw_nominal(LLVMGenCtx *ctx,
                                       LLVMArrayVarEntry *entry);
bool llvm_array_emit_storage_drop(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  const char *callee_name,
                                  LLVMValueRef arr_alloca,
                                  LLVMArrayVarEntry *entry,
                                  LLVMValueRef *out);
bool llvm_array_emit_raw_nominal_push(LLVMGenCtx *ctx,
                                      ASTNode *node,
                                      const char *callee_name,
                                      LLVMValueRef arr_alloca,
                                      LLVMArrayVarEntry *entry,
                                      LLVMValueRef value,
                                      LLVMValueRef *out);
bool llvm_array_emit_raw_nominal_set(LLVMGenCtx *ctx,
                                     ASTNode *node,
                                     const char *callee_name,
                                     LLVMValueRef arr_alloca,
                                     LLVMArrayVarEntry *entry,
                                     LLVMValueRef index64,
                                     LLVMValueRef value,
                                     LLVMValueRef *out);
bool llvm_array_emit_raw_nominal_pop(LLVMGenCtx *ctx,
                                     ASTNode *node,
                                     const char *callee_name,
                                     LLVMValueRef arr_alloca,
                                     LLVMArrayVarEntry *entry,
                                     LLVMValueRef *out);

#endif /* PGY_LLVM_EXPR_ARRAY_RAW_NOMINAL_CALLS_H */
