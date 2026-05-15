#ifndef PGY_LLVM_EXPR_CALL_COLLECTIONS_EXTENDED_H
#define PGY_LLVM_EXPR_CALL_COLLECTIONS_EXTENDED_H

#include "llvm_internal.h"

LLVMTypeRef llvm_collection_required_value_type(LLVMGenCtx *ctx,
                                                ASTNode *node,
                                                const char *collection_kind,
                                                const char *var_name,
                                                const char *type_name,
                                                LLVMValueRef *out);
LLVMFuncEntry *llvm_required_collection_function(LLVMGenCtx *ctx,
                                                 ASTNode *node,
                                                 const char *callee_name,
                                                 const char *function_name);
LLVMVarEntry *llvm_collection_required_receiver_var(LLVMGenCtx *ctx,
                                                    ASTNode *node,
                                                    ASTNode *receiver,
                                                    const char *callee_name,
                                                    const char *collection_kind,
                                                    LLVMValueRef fallback,
                                                    LLVMValueRef *out);
bool llvm_collection_extended_error_out(LLVMGenCtx *ctx,
                                        ASTNode *node,
                                        LLVMValueRef *out,
                                        LLVMValueRef recovery,
                                        const char *message);
bool llvm_emit_list_extended_call(ASTNode *node,
                                  LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out);
bool llvm_emit_collection_extended_call(ASTNode *node,
                                        LLVMGenCtx *ctx,
                                        const char *callee_name,
                                        LLVMValueRef *out);

#endif
