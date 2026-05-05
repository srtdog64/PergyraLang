#ifndef PGY_LLVM_EXPR_IDENTIFIER_SLOT_HELPERS_H
#define PGY_LLVM_EXPR_IDENTIFIER_SLOT_HELPERS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx);
LLVMVarEntry *llvm_resolve_slot_target(LLVMGenCtx *ctx,
                                       ASTNode *slot_arg,
                                       const char **inner_out,
                                       const char **source_name_out,
                                       bool *secure_out);
LLVMValueRef llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx);

#endif
