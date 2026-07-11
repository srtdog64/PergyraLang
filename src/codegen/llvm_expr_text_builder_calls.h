#ifndef PGY_LLVM_EXPR_TEXT_BUILDER_CALLS_H
#define PGY_LLVM_EXPR_TEXT_BUILDER_CALLS_H

#include "llvm_internal.h"

bool llvm_emit_text_builder_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                                         const char *callee_name,
                                         LLVMValueRef *out);

#endif
