#ifndef PGY_LLVM_EXPR_CALL_COLLECTIONS_MAP_EXPORTS_H
#define PGY_LLVM_EXPR_CALL_COLLECTIONS_MAP_EXPORTS_H

#include "llvm_internal.h"

LLVMFuncEntry *llvm_required_hashmap_raw_export(LLVMGenCtx *ctx,
                                                ASTNode *node,
                                                const char *callee_name,
                                                const char *operation,
                                                const char *key_name);
LLVMFuncEntry *llvm_required_hashmap_raw_string_value_export(
    LLVMGenCtx *ctx,
    ASTNode *node,
    const char *callee_name,
    const char *operation,
    const char *key_name);
LLVMTypeRef llvm_hashmap_key_array_type(LLVMGenCtx *ctx,
                                        const char *key_name);

#endif /* PGY_LLVM_EXPR_CALL_COLLECTIONS_MAP_EXPORTS_H */
