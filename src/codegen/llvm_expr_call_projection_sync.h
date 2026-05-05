#ifndef PGY_LLVM_EXPR_CALL_PROJECTION_SYNC_H
#define PGY_LLVM_EXPR_CALL_PROJECTION_SYNC_H

#include "llvm_internal.h"

void llvm_emit_world_embedded_receiver_projection_sync(LLVMGenCtx *ctx,
                                                       ASTNode *receiver);
void llvm_emit_current_zone_subject_projection_sync(LLVMGenCtx *ctx,
                                                   ASTNode *receiver);

#endif
