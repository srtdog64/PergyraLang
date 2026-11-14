#ifndef PERGYRA_LLVM_EXPR_MEMBER_ACCESS_H
#define PERGYRA_LLVM_EXPR_MEMBER_ACCESS_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx);

#endif
