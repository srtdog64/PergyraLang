#ifndef PGY_LLVM_EXPR_MEMBER_LVALUE_H
#define PGY_LLVM_EXPR_MEMBER_LVALUE_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx,
                                         LLVMTypeRef *out_field_type);

#endif
