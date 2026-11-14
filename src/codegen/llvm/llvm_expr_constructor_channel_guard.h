#ifndef PGY_LLVM_EXPR_CONSTRUCTOR_CHANNEL_GUARD_H
#define PGY_LLVM_EXPR_CONSTRUCTOR_CHANNEL_GUARD_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const char *llvm_constructor_find_host_channel_field(LLVMGenCtx *ctx,
                                                     ASTNode *decl);
void llvm_constructor_reject_channel_field(ASTNode *node,
                                           LLVMGenCtx *ctx,
                                           const char *field_name);

#endif

#endif
