#ifndef PGY_LLVM_MEMBER_CALL_EMIT_H
#define PGY_LLVM_MEMBER_CALL_EMIT_H

#include "llvm_internal.h"

LLVMValueRef llvm_emit_member_call(ASTNode *node, LLVMGenCtx *ctx);

#endif
