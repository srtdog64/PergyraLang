#ifndef PGY_LLVM_EXPR_CALL_COLLECTIONS_EXTENDED_H
#define PGY_LLVM_EXPR_CALL_COLLECTIONS_EXTENDED_H

#include "llvm_internal.h"

/* Scoped copies: emitting operands may grow scope/registry storage. */
typedef struct LLVMMapReceiverMetadata {
    const char *key_type;
    const char *value_type;
    LLVMTypeRef value_llvm_type;
} LLVMMapReceiverMetadata;
bool llvm_map_receiver_metadata_ready(LLVMGenCtx *ctx, ASTNode *receiver,
                                      LLVMMapReceiverMetadata *metadata);

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
bool llvm_collection_required_receiver_var(LLVMGenCtx *ctx,
                                           ASTNode *node,
                                           ASTNode *receiver,
                                           const char *callee_name,
                                           const char *collection_kind,
                                           LLVMVarEntry *receiver_out,
                                           LLVMValueRef *out);
bool llvm_collection_extended_error_out(LLVMGenCtx *ctx,
                                        ASTNode *node,
                                        LLVMValueRef *out,
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
