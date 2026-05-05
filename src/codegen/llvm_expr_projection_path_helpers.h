#ifndef PGY_LLVM_EXPR_PROJECTION_PATH_HELPERS_H
#define PGY_LLVM_EXPR_PROJECTION_PATH_HELPERS_H

#include "llvm_internal.h"

ASTNode *llvm_find_projection_nominal_decl(LLVMGenCtx *ctx, const char *name);

LLVMValueRef llvm_load_projection_path_value(LLVMGenCtx *ctx,
                                             ASTNode *source_decl,
                                             LLVMClassTypeEntry *source_cls,
                                             LLVMValueRef source_ptr,
                                             const char *field_name);

LLVMValueRef llvm_emit_subject_projection(ASTNode *node, LLVMGenCtx *ctx);

#endif
