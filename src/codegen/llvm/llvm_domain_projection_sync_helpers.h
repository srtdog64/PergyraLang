#ifndef PGY_LLVM_DOMAIN_PROJECTION_SYNC_HELPERS_H
#define PGY_LLVM_DOMAIN_PROJECTION_SYNC_HELPERS_H

#include "llvm_internal.h"

void llvm_emit_domain_projection_sync(ASTNode *stmt,
                                      const char *decl_name,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx);

#endif
