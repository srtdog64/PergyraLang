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
bool llvm_emit_collection_extended_call(ASTNode *node,
                                        LLVMGenCtx *ctx,
                                        const char *callee_name,
                                        LLVMValueRef *out);

#endif
