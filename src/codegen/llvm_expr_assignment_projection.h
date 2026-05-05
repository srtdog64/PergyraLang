#ifndef PGY_LLVM_EXPR_ASSIGNMENT_PROJECTION_H
#define PGY_LLVM_EXPR_ASSIGNMENT_PROJECTION_H

#include "llvm_internal.h"

void llvm_emit_host_projection_invalidations(LLVMGenCtx *ctx,
                                             ASTNode *target);
void llvm_emit_world_embedded_assignment_sync(LLVMGenCtx *ctx,
                                              ASTNode *target);
bool llvm_world_embedded_projection_source_from_assignment(
    LLVMGenCtx *ctx,
    ASTNode *target,
    const char **zone_slot_out,
    ASTNode **zone_decl_out,
    const char **source_slot_out,
    const char **source_field_out);

#endif
