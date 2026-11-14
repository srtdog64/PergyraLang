#ifndef PGY_LLVM_DOMAIN_PROJECTION_SYNC_BODY_HELPERS_H
#define PGY_LLVM_DOMAIN_PROJECTION_SYNC_BODY_HELPERS_H

#include "llvm_internal.h"

void llvm_emit_domain_projection_sync_body(ASTNode *stmt,
                                           LLVMClassTypeEntry *decl_cls,
                                           LLVMValueRef sync_fn,
                                           LLVMGenCtx *ctx);

#endif
