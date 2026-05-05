#ifndef PGY_LLVM_EXPR_CALL_METHODS_WORLD_EFFECT_SYNC_H
#define PGY_LLVM_EXPR_CALL_METHODS_WORLD_EFFECT_SYNC_H

#include "llvm_internal.h"

void llvm_emit_world_embedded_action_effect_sync(LLVMGenCtx *ctx,
                                                 ASTNode *receiver,
                                                 ASTNode *method_decl);

#endif
