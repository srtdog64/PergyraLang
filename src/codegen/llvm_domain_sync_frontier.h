#ifndef PGY_LLVM_DOMAIN_SYNC_FRONTIER_H
#define PGY_LLVM_DOMAIN_SYNC_FRONTIER_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void llvm_emit_sync_generation_increment(LLVMGenCtx *ctx,
                                         LLVMClassTypeEntry *decl_cls,
                                         LLVMValueRef self_ptr);
void llvm_emit_frontier_overflow_abort(LLVMGenCtx *ctx, const char *reason);
void llvm_finish_domain_sync_emit(LLVMGenCtx *ctx,
                                  LLVMValueRef saved_fn,
                                  LLVMTypeRef saved_ret,
                                  ASTNode *saved_host_decl);

#endif

#endif
