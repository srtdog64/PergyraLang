#ifndef PGY_LLVM_DOMAIN_PROJECTION_VALUE_HELPERS_H
#define PGY_LLVM_DOMAIN_PROJECTION_VALUE_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef llvm_load_domain_projection_path_value_by_name(
    LLVMGenCtx *ctx,
    const char *source_type_name,
    LLVMClassTypeEntry *source_cls,
    LLVMValueRef source_ptr,
    const char *field_name);

LLVMValueRef llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                                LLVMClassTypeEntry *target_cls,
                                                LLVMClassTypeEntry *source_cls,
                                                const char *source_type_name,
                                                ASTNode *refresh,
                                                LLVMValueRef source_ptr);

#endif
