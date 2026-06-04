#ifndef PGY_LLVM_MIR_BLOCK_EMIT_H
#define PGY_LLVM_MIR_BLOCK_EMIT_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

void llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block,
                                    const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    LLVMBasicBlockRef *llvm_block_heads,
                                    LLVMBasicBlockRef *llvm_block_tails,
                                    LLVMMirVar *vars,
                                    size_t var_count,
                                    ASTNode *func_decl,
                                    LLVMClassTypeEntry *owner_cls,
                                    LLVMFuncEntry *owner_sync,
                                    const char *owner_name);

bool llvm_mir_copy_host_field_to_versioned_local(LLVMGenCtx *ctx,
                                                 const char *field_name,
                                                 LLVMMirVar *target);

#endif
