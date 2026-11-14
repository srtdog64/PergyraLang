#ifndef PGY_LLVM_EXPR_PROJECTION_PATH_HELPERS_H
#define PGY_LLVM_EXPR_PROJECTION_PATH_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef llvm_load_projection_path_value_by_name(
    LLVMGenCtx *ctx,
    const char *source_type_name,
    LLVMClassTypeEntry *source_cls,
    LLVMValueRef source_ptr,
    const char *field_name,
    ASTNode *diag_node);

LLVMValueRef llvm_emit_subject_projection(ASTNode *node, LLVMGenCtx *ctx);

#endif
